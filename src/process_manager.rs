/*!
 * Process-based architecture for FauxDB
 * Spawns separate processes for each MongoDB connection
 */

use std::process::{Child, Command, Stdio};
use std::sync::{Arc, Mutex};
use std::collections::HashMap;
use std::time::{Duration, Instant};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use anyhow::{Result, anyhow};
use serde::{Serialize, Deserialize};
use uuid::Uuid;

use crate::logger::ConnectionTracker;
use crate::{fauxdb_info, fauxdb_warn, fauxdb_error};
use std::io::{Read, Write};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProcessConfig {
    pub max_connections: u32,
    pub process_timeout: Duration,
    pub connection_timeout: Duration,
    pub database_url: String,
    pub log_level: String,
}

impl Default for ProcessConfig {
    fn default() -> Self {
        Self {
            max_connections: 100,
            process_timeout: Duration::from_secs(300), // 5 minutes
            connection_timeout: Duration::from_secs(30),
            database_url: "postgresql://localhost/fauxdb".to_string(),
            log_level: "info".to_string(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct ProcessInfo {
    pub pid: u32,
    pub connection_id: Uuid,
    pub client_addr: String,
    pub started_at: Instant,
    pub last_activity: Instant,
    pub status: ProcessStatus,
    pub bytes_processed: u64,
    pub commands_executed: u64,
}

#[derive(Debug, Clone, PartialEq)]
pub enum ProcessStatus {
    Starting,
    Running,
    Idle,
    Error,
    Terminated,
}

pub struct ProcessManager {
    config: ProcessConfig,
    active_processes: Arc<Mutex<HashMap<Uuid, ProcessInfo>>>,
    connection_tracker: Arc<ConnectionTracker>,
    binary_path: String,
}

impl ProcessManager {
    pub fn new(config: ProcessConfig) -> Self {
        let max_connections = config.max_connections;
        Self {
            config,
            active_processes: Arc::new(Mutex::new(HashMap::new())),
            connection_tracker: Arc::new(ConnectionTracker::new(max_connections)),
            binary_path: std::env::current_exe()
                .unwrap_or_else(|_| "fauxdb-worker".into())
                .to_string_lossy()
                .to_string(),
        }
    }

    pub async fn handle_connection(&self, mut stream: TcpStream) -> Result<()> {
        let client_addr = stream.peer_addr()?.to_string();
        let connection_id = Uuid::new_v4();
        
        fauxdb_info!("New connection from {} (ID: {})", client_addr, connection_id);

        // Check connection limit
        if self.connection_tracker.get_connection_count() >= self.config.max_connections as usize {
            fauxdb_warn!("Connection limit reached, rejecting connection from {}", client_addr);
            let _ = stream.shutdown().await;
            return Err(anyhow!("Connection limit reached"));
        }

        // Add to connection tracker
        if let Err(e) = self.connection_tracker.add_connection(connection_id.as_u128() as u32, client_addr.clone()) {
            fauxdb_error!("Failed to track connection: {}", e);
            let _ = stream.shutdown().await;
            return Err(anyhow!("Connection tracking error: {}", e));
        }

        // Spawn worker process
        let worker_process = self.spawn_worker_process(connection_id, client_addr.clone())?;
        
        // Store process info
        let process_info = ProcessInfo {
            pid: worker_process.id(),
            connection_id,
            client_addr: client_addr.clone(),
            started_at: Instant::now(),
            last_activity: Instant::now(),
            status: ProcessStatus::Starting,
            bytes_processed: 0,
            commands_executed: 0,
        };

        {
            let mut processes = self.active_processes.lock().unwrap();
            processes.insert(connection_id, process_info);
        }

        // Forward connection to worker process
        match self.forward_connection_to_worker(stream, worker_process).await {
            Ok(_) => {
                fauxdb_info!("Connection {} completed successfully", connection_id);
            }
            Err(e) => {
                fauxdb_error!("Connection {} failed: {}", connection_id, e);
            }
        }

        // Cleanup
        self.cleanup_connection(connection_id).await;

        Ok(())
    }

    fn spawn_worker_process(&self, connection_id: Uuid, client_addr: String) -> Result<Child> {
        let mut cmd = Command::new(&self.binary_path);
        
        // Pass connection information to worker process
        cmd.arg("worker")
           .arg("--connection-id")
           .arg(connection_id.to_string())
           .arg("--client-addr")
           .arg(client_addr)
           .arg("--database-url")
           .arg(&self.config.database_url)
           .arg("--log-level")
           .arg(&self.config.log_level)
           .stdin(Stdio::piped())
           .stdout(Stdio::piped())
           .stderr(Stdio::piped());

        let child = cmd.spawn()?;
        
        fauxdb_info!("Spawned worker process PID {} for connection {}", 
                    child.id(), connection_id);
        
        Ok(child)
    }

    async fn forward_connection_to_worker(&self, mut stream: TcpStream, mut worker_process: Child) -> Result<()> {
        let connection_id = Uuid::new_v4(); // This should be passed from the caller
        
        // Get worker process stdio
        let mut worker_stdin = worker_process.stdin.take()
            .ok_or_else(|| anyhow!("Failed to get worker stdin"))?;
        
        let worker_stdout = worker_process.stdout.take()
            .ok_or_else(|| anyhow!("Failed to get worker stdout"))?;

        // Create async readers/writers
        let mut worker_stdout = tokio::process::ChildStdout::from_std(worker_stdout)?;

        // Buffer for data transfer
        let mut buffer = vec![0u8; 4096];
        let mut worker_buffer = vec![0u8; 4096];

        loop {
            tokio::select! {
                // Read from client and forward to worker
                result = stream.read(&mut buffer) => {
                    match result {
                        Ok(0) => {
                            fauxdb_info!("Client disconnected, closing connection {}", connection_id);
                            break;
                        }
                        Ok(n) => {
                            // Update activity tracking
                            self.connection_tracker.update_activity(
                                connection_id.as_u128() as u32, 
                                n as u64, 
                                0
                            );
                            
                            // Forward to worker process
                            if let Err(e) = worker_stdin.write_all(&buffer[..n]) {
                                fauxdb_error!("Failed to write to worker stdin: {}", e);
                                break;
                            }
                            
                            // Update process info
                            self.update_process_activity(connection_id, n as u64);
                        }
                        Err(e) => {
                            fauxdb_error!("Error reading from client: {}", e);
                            break;
                        }
                    }
                }

                // Read from worker and forward to client
                result = worker_stdout.read(&mut worker_buffer) => {
                    match result {
                        Ok(0) => {
                            fauxdb_info!("Worker process finished for connection {}", connection_id);
                            break;
                        }
                        Ok(n) => {
                            // Update activity tracking
                            self.connection_tracker.update_activity(
                                connection_id.as_u128() as u32, 
                                0, 
                                n as u64
                            );
                            
                            // Forward to client
                            if let Err(e) = stream.write_all(&worker_buffer[..n]).await {
                                fauxdb_error!("Failed to write to client: {}", e);
                                break;
                            }
                        }
                        Err(e) => {
                            fauxdb_error!("Error reading from worker: {}", e);
                            break;
                        }
                    }
                }

                // Check for worker process termination
                _ = tokio::time::sleep(Duration::from_millis(100)) => {
                    if let Ok(Some(status)) = worker_process.try_wait() {
                        fauxdb_info!("Worker process terminated with status: {:?}", status);
                        break;
                    }
                }
            }
        }

        Ok(())
    }

    fn update_process_activity(&self, connection_id: Uuid, bytes: u64) {
        let mut processes = self.active_processes.lock().unwrap();
        if let Some(process_info) = processes.get_mut(&connection_id) {
            process_info.last_activity = Instant::now();
            process_info.bytes_processed += bytes;
            process_info.status = ProcessStatus::Running;
        }
    }

    async fn cleanup_connection(&self, connection_id: Uuid) {
        // Remove from connection tracker
        self.connection_tracker.remove_connection(connection_id.as_u128() as u32);
        
        // Remove from active processes
        {
            let mut processes = self.active_processes.lock().unwrap();
            if let Some(process_info) = processes.remove(&connection_id) {
                let duration = process_info.started_at.elapsed();
                fauxdb_info!("Cleaned up connection {} (PID: {}, Duration: {:?}, Bytes: {}, Commands: {})",
                           connection_id, process_info.pid, duration, 
                           process_info.bytes_processed, process_info.commands_executed);
            }
        }
    }

    pub fn get_connection_count(&self) -> usize {
        self.connection_tracker.get_connection_count()
    }

    pub fn get_active_processes(&self) -> Vec<ProcessInfo> {
        self.active_processes.lock().unwrap().values().cloned().collect()
    }

    pub fn get_process_info(&self, connection_id: Uuid) -> Option<ProcessInfo> {
        self.active_processes.lock().unwrap().get(&connection_id).cloned()
    }

    pub async fn cleanup_idle_processes(&self) {
        let now = Instant::now();
        let timeout = self.config.process_timeout;
        
        let idle_connections: Vec<Uuid> = {
            let processes = self.active_processes.lock().unwrap();
            processes.iter()
                .filter(|(_, info)| {
                    info.status == ProcessStatus::Idle && 
                    now.duration_since(info.last_activity) > timeout
                })
                .map(|(id, _)| *id)
                .collect()
        };

        for connection_id in idle_connections {
            fauxdb_warn!("Cleaning up idle connection {}", connection_id);
            self.cleanup_connection(connection_id).await;
        }
    }

    pub async fn shutdown(&self) -> Result<()> {
        fauxdb_info!("Shutting down process manager");
        
        // Terminate all active processes
        let connection_ids: Vec<Uuid> = {
            let processes = self.active_processes.lock().unwrap();
            processes.keys().cloned().collect()
        };

        for connection_id in connection_ids {
            self.cleanup_connection(connection_id).await;
        }

        fauxdb_info!("Process manager shutdown complete");
        Ok(())
    }
}

// Worker process entry point
pub async fn run_worker_process(connection_id: Uuid, client_addr: String, database_url: String) -> Result<()> {
    // Initialize logging for worker process
    crate::logger::init_logger(crate::logger::LogLevel::Info, false);
    
    fauxdb_info!("Worker process started for connection {} from {}", connection_id, client_addr);
    
    // Initialize database connection
    let client = tokio_postgres::connect(&database_url, tokio_postgres::NoTls).await?;
    fauxdb_info!("Database connection established for worker {}", connection_id);
    
    // Process MongoDB commands
    let mut buffer = vec![0u8; 4096];
    
    loop {
        // Read from stdin (from main process)
        let bytes_read = match std::io::stdin().read(&mut buffer) {
            Ok(0) => {
                fauxdb_info!("Worker {} received EOF, shutting down", connection_id);
                break;
            }
            Ok(n) => n,
            Err(e) => {
                fauxdb_error!("Worker {} stdin error: {}", connection_id, e);
                break;
            }
        };

        // Process MongoDB protocol message
        if let Ok(command_doc) = bson::from_slice::<bson::Document>(&buffer[..bytes_read]) {
            fauxdb_info!("Worker {} processing command: {:?}", connection_id, command_doc.keys().collect::<Vec<_>>());
            
            // Execute command (simplified)
            let response = process_mongodb_command(&client.0, command_doc).await?;
            
            // Send response back to main process via stdout
            let response_bytes = bson::to_vec(&response)?;
            std::io::stdout().write_all(&response_bytes)?;
            std::io::stdout().flush()?;
        }
    }

    fauxdb_info!("Worker process {} shutting down", connection_id);
    Ok(())
}

async fn process_mongodb_command(_client: &tokio_postgres::Client, _command: bson::Document) -> Result<bson::Document> {
    // Simplified command processing
    let mut response = bson::Document::new();
    response.insert("ok", 1.0);
    response.insert("processed", true);
    
    // In a real implementation, this would:
    // 1. Parse the MongoDB command
    // 2. Convert to SQL
    // 3. Execute against PostgreSQL
    // 4. Convert results back to MongoDB format
    
    Ok(response)
}
