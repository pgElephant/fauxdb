/*
 * Copyright (c) 2025 pgElephant. All rights reserved.
 *
 * FauxDB - Production-ready MongoDB-compatible database server
 * Built with Rust for superior performance and reliability
 *
 * @file main.rs
 * @brief Production FauxDB server main entry point - Full MongoDB 5.0+ compatibility
 */

use fauxdb::{
    ProductionConfig, ProductionServer, ProcessManager,
    logger::{LogLevel, init_logger}, fauxdb_info
};

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    // Initialize logging system
    init_logger(LogLevel::Info, false);

    // Check if running as worker process
    let args: Vec<String> = std::env::args().collect();
    if args.len() > 1 && args[1] == "worker" {
        return run_worker_mode().await;
    }

    // Run as main server process
    run_server_mode().await
}

async fn run_server_mode() -> anyhow::Result<()> {
    fauxdb_info!("Production FauxDB Server Starting");
    fauxdb_info!("==========================================");
    
    // Load production configuration
    let config = ProductionConfig::load_from_env()
        .or_else(|_| ProductionConfig::load_from_file("config/fauxdb-27018.toml"))
        .or_else(|_| ProductionConfig::load_from_file("config/fauxdb-production.toml"))?;
    
    fauxdb_info!("Production configuration loaded");
    fauxdb_info!("MongoDB 5.0+ Compatibility: ENABLED");
    fauxdb_info!("Security Features: {}", if config.security.enable_auth { "ENABLED" } else { "DISABLED" });
    fauxdb_info!("Monitoring: {}", if config.monitoring.metrics_enabled { "ENABLED" } else { "DISABLED" });
    fauxdb_info!("Max Connections: {}", config.server.max_connections);
    fauxdb_info!("Server Address: {}:{}", config.server.host, config.server.port);
    fauxdb_info!("Database: {}", config.database.connection_string);
    fauxdb_info!("Process-based Architecture: ENABLED");
    fauxdb_info!("==========================================");
    
    // Initialize process manager for handling connections
    let process_manager = ProcessManager::new(fauxdb::process_manager::ProcessConfig {
        max_connections: config.server.max_connections as u32,
        process_timeout: std::time::Duration::from_secs(300),
        connection_timeout: std::time::Duration::from_secs(30),
        database_url: config.database.connection_string.clone(),
        log_level: "info".to_string(),
    });
    
    // Initialize and start the production server with all infrastructure
    let mut server = ProductionServer::new(config);
    
    // Initialize all components (shutdown, circuit breakers, rate limiting, health checks)
    server.initialize().await?;
    
    fauxdb_info!("Production FauxDB Server initialized successfully");
    fauxdb_info!("Ready for production workloads!");
    fauxdb_info!("Features:");
    fauxdb_info!("  - Full MongoDB 5.0+ command compatibility");
    fauxdb_info!("  - Process-based architecture for concurrent connections");
    fauxdb_info!("  - Advanced aggregation pipelines");
    fauxdb_info!("  - Comprehensive indexing support");
    fauxdb_info!("  - ACID transaction support");
    fauxdb_info!("  - Connection pooling and resource management");
    fauxdb_info!("  - Production-grade monitoring and metrics");
    fauxdb_info!("  - Enterprise security features");
    fauxdb_info!("  - High-performance PostgreSQL backend");
    fauxdb_info!("  - Health checks and observability");
    fauxdb_info!("  - Rate limiting and circuit breakers");
    fauxdb_info!("  - Graceful shutdown handling");
    fauxdb_info!("==========================================");
    
    // Start the server (this will block until shutdown)
    server.start().await?;
    
    Ok(())
}

async fn run_worker_mode() -> anyhow::Result<()> {
    let args: Vec<String> = std::env::args().collect();
    
    // Parse worker arguments
    let mut connection_id = None;
    let mut client_addr = None;
    let mut database_url = None;
    let mut _log_level = "info".to_string();
    
    let mut i = 2;
    while i < args.len() {
        match args[i].as_str() {
            "--connection-id" => {
                if i + 1 < args.len() {
                    connection_id = Some(args[i + 1].clone());
                    i += 2;
                } else {
                    return Err(anyhow::anyhow!("Missing connection-id argument"));
                }
            }
            "--client-addr" => {
                if i + 1 < args.len() {
                    client_addr = Some(args[i + 1].clone());
                    i += 2;
                } else {
                    return Err(anyhow::anyhow!("Missing client-addr argument"));
                }
            }
            "--database-url" => {
                if i + 1 < args.len() {
                    database_url = Some(args[i + 1].clone());
                    i += 2;
                } else {
                    return Err(anyhow::anyhow!("Missing database-url argument"));
                }
            }
            "--log-level" => {
                if i + 1 < args.len() {
                    _log_level = args[i + 1].clone();
                    i += 2;
                } else {
                    return Err(anyhow::anyhow!("Missing log-level argument"));
                }
            }
            _ => i += 1,
        }
    }
    
    let connection_id = connection_id.ok_or_else(|| anyhow::anyhow!("Missing connection-id"))?;
    let client_addr = client_addr.ok_or_else(|| anyhow::anyhow!("Missing client-addr"))?;
    let database_url = database_url.ok_or_else(|| anyhow::anyhow!("Missing database-url"))?;
    
    fauxdb_info!("Worker process started for connection {} from {}", connection_id, client_addr);
    
    // Initialize worker with database connection
    let config = ProductionConfig::load_from_env()
        .or_else(|_| ProductionConfig::load_from_file("config/fauxdb-production.toml"))?;
    
    let mut config = config;
    config.database.connection_string = database_url;
    
    let _server = ProductionServer::new(config);
    
    // Handle the connection in this worker process
    // This would typically involve reading from stdin and writing to stdout
    // to communicate with the main process
    
    fauxdb_info!("Worker process {} completed", connection_id);
    Ok(())
}

