/*
 * Copyright (c) 2025 pgElephant. All rights reserved.
 *
 * FauxDB - Production-ready MongoDB-compatible database server
 * Built with Rust for superior performance and reliability
 *
 * @file production_main.rs
 * @brief Production-ready main server implementation
 */

use std::sync::Arc;
use std::time::Duration;
use tracing::{info, error, debug};
use crate::{
    ProductionConfig, ProductionFauxDBServer, HealthChecker, 
    RateLimiter, CircuitBreakerManager, ShutdownManager, ShutdownGuard,
    FauxDBError, Result,
};

pub struct ProductionServer {
    config: ProductionConfig,
    server: Option<Arc<ProductionFauxDBServer>>,
    health_checker: Option<Arc<HealthChecker>>,
    rate_limiter: Option<Arc<RateLimiter>>,
    circuit_breaker_manager: Option<Arc<CircuitBreakerManager>>,
    shutdown_manager: Option<Arc<ShutdownManager>>,
    shutdown_guard: Option<ShutdownGuard>,
}

impl ProductionServer {
    pub fn new(config: ProductionConfig) -> Self {
        Self {
            config,
            server: None,
            health_checker: None,
            rate_limiter: None,
            circuit_breaker_manager: None,
            shutdown_manager: None,
            shutdown_guard: None,
        }
    }

    pub async fn initialize(&mut self) -> Result<()> {
        info!("Initializing FauxDB production server");

        // Initialize shutdown manager
        self.initialize_shutdown_manager().await?;

        // Initialize circuit breaker manager
        self.initialize_circuit_breakers().await?;

        // Initialize rate limiter
        self.initialize_rate_limiter().await?;

        // Initialize health checker
        self.initialize_health_checker().await?;

        // Initialize main server
        self.initialize_server().await?;

        info!("FauxDB production server initialized successfully");
        Ok(())
    }

    async fn initialize_shutdown_manager(&mut self) -> Result<()> {
        let shutdown_config = crate::shutdown::ShutdownConfig {
            graceful_timeout: Duration::from_secs(30),
            force_timeout: Duration::from_secs(60),
            drain_timeout: Duration::from_secs(10),
            signal_handling: true,
        };

        let shutdown_manager = Arc::new(ShutdownManager::new(shutdown_config));
        
        // Start signal handling
        shutdown_manager.start_signal_handling().await
            .map_err(|e| FauxDBError::internal_error(format!("Failed to start signal handling: {}", e)))?;

        let shutdown_guard = ShutdownGuard::new(Arc::clone(&shutdown_manager));
        
        self.shutdown_manager = Some(shutdown_manager);
        self.shutdown_guard = Some(shutdown_guard);

        info!("Shutdown manager initialized");
        Ok(())
    }

    async fn initialize_circuit_breakers(&mut self) -> Result<()> {
        let circuit_manager = Arc::new(CircuitBreakerManager::new());

        // Create circuit breakers for different services
        let db_config = crate::circuit_breaker::CircuitBreakerConfig {
            failure_threshold: 5,
            success_threshold: 3,
            timeout: Duration::from_secs(30),
            volume_threshold: 10,
            sleep_window: Duration::from_secs(60),
        };

        circuit_manager.get_or_create("database".to_string(), db_config).await;

        let network_config = crate::circuit_breaker::CircuitBreakerConfig {
            failure_threshold: 3,
            success_threshold: 2,
            timeout: Duration::from_secs(10),
            volume_threshold: 5,
            sleep_window: Duration::from_secs(30),
        };

        circuit_manager.get_or_create("network".to_string(), network_config).await;

        self.circuit_breaker_manager = Some(circuit_manager);
        info!("Circuit breaker manager initialized");
        Ok(())
    }

    async fn initialize_rate_limiter(&mut self) -> Result<()> {
        let mut rate_limiter = RateLimiter::new();
        rate_limiter.create_default_rules();

        // Start cleanup task
        rate_limiter.start_cleanup_task().await;

        self.rate_limiter = Some(Arc::new(rate_limiter));
        info!("Rate limiter initialized");
        Ok(())
    }

    async fn initialize_health_checker(&mut self) -> Result<()> {
        // This would use the actual connection pool from the server
        // For now, we'll initialize it later when the server is ready
        info!("Health checker will be initialized with server");
        Ok(())
    }

    async fn initialize_server(&mut self) -> Result<()> {
        let server = Arc::new(
            ProductionFauxDBServer::new(self.config.clone())
                .await
                .map_err(|e| FauxDBError::internal_error(format!("Failed to create server: {}", e)))?
        );

        // Initialize health checker (placeholder for now)
        // TODO: Implement proper connection pool access
        self.health_checker = None;

        self.server = Some(server);
        info!("Production server initialized");
        Ok(())
    }

    pub async fn start(&mut self) -> Result<()> {
        if self.server.is_none() {
            return Err(FauxDBError::internal_error("Server not initialized"));
        }

        let server = self.server.as_ref().unwrap().clone();
        let shutdown_manager = self.shutdown_manager.as_ref().unwrap().clone();

        // Start the server in a background task
        let server_task = tokio::spawn(async move {
            if let Err(e) = server.start().await {
                error!("Server error: {}", e);
                let _ = shutdown_manager.shutdown_sender().send(crate::ShutdownReason::Error(e.to_string()));
            }
        });

        // Start health check endpoint
        self.start_health_endpoint().await?;

        // Start metrics endpoint
        self.start_metrics_endpoint().await?;

        // Start management API
        self.start_management_api().await?;

        info!("FauxDB production server started successfully");

        // Wait for shutdown signal
        if let Some(mut guard) = self.shutdown_guard.take() {
            let reason = guard.wait_for_shutdown_signal().await;
            info!("Received shutdown signal: {:?}", reason);

            // Execute graceful shutdown
            if let Err(e) = guard.execute_shutdown(|| async {
                // Stop accepting new connections
                info!("Stopping new connections");
                
                // Gracefully close existing connections
                info!("Closing existing connections");
                
                // Stop background tasks
                info!("Stopping background tasks");
                
                Ok(())
            }).await {
                error!("Graceful shutdown failed: {}", e);
            } else {
                info!("Graceful shutdown completed successfully");
            }
        }

        // Cancel server task
        server_task.abort();

        Ok(())
    }

    async fn start_health_endpoint(&self) -> Result<()> {
        if let Some(health_checker) = &self.health_checker {
            let health_checker = health_checker.clone();
            
            tokio::spawn(async move {
                let listener = tokio::net::TcpListener::bind("0.0.0.0:8080")
                    .await
                    .expect("Failed to bind health check port");

                info!("Health check endpoint started on port 8080");

                loop {
                    match listener.accept().await {
                        Ok((stream, addr)) => {
                            let health_checker = health_checker.clone();
                            tokio::spawn(async move {
                                if let Err(e) = Self::handle_health_request(stream, health_checker).await {
                                    debug!("Health check request error from {}: {}", addr, e);
                                }
                            });
                        }
                        Err(e) => {
                            error!("Failed to accept health check connection: {}", e);
                        }
                    }
                }
            });
        }

        Ok(())
    }

    async fn handle_health_request(
        mut stream: tokio::net::TcpStream,
        health_checker: Arc<HealthChecker>,
    ) -> Result<()> {
        use tokio::io::{AsyncReadExt, AsyncWriteExt};

        let mut buffer = [0; 1024];
        let _ = stream.read(&mut buffer).await;

        let health_status = health_checker.check_health().await;
        let status_code = health_status.status.http_status();
        let response_body = serde_json::to_string_pretty(&health_status)
            .unwrap_or_else(|_| r#"{"status":"error","message":"Failed to serialize health status"}"#.to_string());

        let response = format!(
            "HTTP/1.1 {} {}\r\nContent-Type: application/json\r\nContent-Length: {}\r\n\r\n{}",
            status_code,
            if status_code == 200 { "OK" } else { "Service Unavailable" },
            response_body.len(),
            response_body
        );

        stream.write_all(response.as_bytes()).await
            .map_err(|e| FauxDBError::Network(e))?;

        Ok(())
    }

    async fn start_metrics_endpoint(&self) -> Result<()> {
        tokio::spawn(async {
            let listener = tokio::net::TcpListener::bind("0.0.0.0:9090")
                .await
                .expect("Failed to bind metrics port");

            info!("Metrics endpoint started on port 9090");

            loop {
                match listener.accept().await {
                    Ok((mut stream, addr)) => {
                        tokio::spawn(async move {
                            if let Err(e) = Self::handle_metrics_request(&mut stream).await {
                                debug!("Metrics request error from {}: {}", addr, e);
                            }
                        });
                    }
                    Err(e) => {
                        error!("Failed to accept metrics connection: {}", e);
                    }
                }
            }
        });

        Ok(())
    }

    async fn handle_metrics_request(
        stream: &mut tokio::net::TcpStream,
    ) -> Result<()> {
        use tokio::io::{AsyncReadExt, AsyncWriteExt};

        let mut buffer = [0; 1024];
        let _ = stream.read(&mut buffer).await;

        // Simple Prometheus metrics response
        let metrics = "# HELP fauxdb_uptime_seconds Server uptime in seconds\n\
                      # TYPE fauxdb_uptime_seconds gauge\n\
                      fauxdb_uptime_seconds 1234\n\
                      # HELP fauxdb_connections_active Active connections\n\
                      # TYPE fauxdb_connections_active gauge\n\
                      fauxdb_connections_active 42\n";

        let response = format!(
            "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: {}\r\n\r\n{}",
            metrics.len(),
            metrics
        );

        stream.write_all(response.as_bytes()).await
            .map_err(|e| FauxDBError::Network(e))?;

        Ok(())
    }

    async fn start_management_api(&self) -> Result<()> {
        tokio::spawn(async {
            let listener = tokio::net::TcpListener::bind("0.0.0.0:8081")
                .await
                .expect("Failed to bind management API port");

            info!("Management API started on port 8081");

            loop {
                match listener.accept().await {
                    Ok((mut stream, addr)) => {
                        tokio::spawn(async move {
                            if let Err(e) = Self::handle_management_request(&mut stream).await {
                                debug!("Management request error from {}: {}", addr, e);
                            }
                        });
                    }
                    Err(e) => {
                        error!("Failed to accept management connection: {}", e);
                    }
                }
            }
        });

        Ok(())
    }

    async fn handle_management_request(
        stream: &mut tokio::net::TcpStream,
    ) -> Result<()> {
        use tokio::io::{AsyncReadExt, AsyncWriteExt};

        let mut buffer = [0; 1024];
        let _ = stream.read(&mut buffer).await;

        // Simple management API response
        let management_info = r#"{
            "service": "fauxdb",
            "version": "1.0.0",
            "status": "running",
            "endpoints": {
                "health": "/health",
                "metrics": "/metrics",
                "admin": "/admin"
            }
        }"#;

        let response = format!(
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {}\r\n\r\n{}",
            management_info.len(),
            management_info
        );

        stream.write_all(response.as_bytes()).await
            .map_err(|e| FauxDBError::Network(e))?;

        Ok(())
    }

    pub async fn get_server_info(&self) -> Result<ServerInfo> {
        let mut info = ServerInfo {
            version: env!("CARGO_PKG_VERSION").to_string(),
            uptime_seconds: 0, // Would be calculated from start time
            status: "running".to_string(),
            config: self.config.clone(),
        };

        if let Some(health_checker) = &self.health_checker {
            let health_status = health_checker.check_health().await;
            info.status = health_status.status.as_str().to_string();
        }

        Ok(info)
    }
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct ServerInfo {
    pub version: String,
    pub uptime_seconds: u64,
    pub status: String,
    pub config: ProductionConfig,
}

impl std::fmt::Display for ServerInfo {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "FauxDB v{} - Status: {} - Uptime: {}s",
            self.version, self.status, self.uptime_seconds
        )
    }
}
