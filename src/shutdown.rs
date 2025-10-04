/*
 * Copyright (c) 2025 pgElephant. All rights reserved.
 *
 * FauxDB - Production-ready MongoDB-compatible database server
 * Built with Rust for superior performance and reliability
 *
 * @file shutdown.rs
 * @brief Graceful shutdown and signal handling
 */

use std::sync::Arc;
use std::time::{Duration, Instant};
use tokio::sync::{broadcast, Mutex, RwLock};
use tokio::time::{sleep, timeout};
use tracing::{info, warn, error, debug};

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ShutdownReason {
    Signal(i32),
    Request,
    Timeout,
    Error(String),
}

impl ShutdownReason {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Signal(_) => "signal",
            Self::Request => "request",
            Self::Timeout => "timeout",
            Self::Error(_) => "error",
        }
    }
}

#[derive(Debug, Clone)]
pub struct ShutdownConfig {
    pub graceful_timeout: Duration,
    pub force_timeout: Duration,
    pub drain_timeout: Duration,
    pub signal_handling: bool,
}

impl Default for ShutdownConfig {
    fn default() -> Self {
        Self {
            graceful_timeout: Duration::from_secs(30),
            force_timeout: Duration::from_secs(60),
            drain_timeout: Duration::from_secs(10),
            signal_handling: true,
        }
    }
}

#[derive(Debug)]
pub struct ShutdownManager {
    shutdown_tx: broadcast::Sender<ShutdownReason>,
    shutdown_rx: broadcast::Receiver<ShutdownReason>,
    is_shutting_down: Arc<RwLock<bool>>,
    shutdown_reason: Arc<Mutex<Option<ShutdownReason>>>,
    config: ShutdownConfig,
    shutdown_started: Arc<RwLock<Option<Instant>>>,
}

impl ShutdownManager {
    pub fn new(config: ShutdownConfig) -> Self {
        let (shutdown_tx, shutdown_rx) = broadcast::channel(16);
        
        Self {
            shutdown_tx,
            shutdown_rx,
            is_shutting_down: Arc::new(RwLock::new(false)),
            shutdown_reason: Arc::new(Mutex::new(None)),
            config,
            shutdown_started: Arc::new(RwLock::new(None)),
        }
    }

    pub async fn start_signal_handling(&self) -> Result<(), Box<dyn std::error::Error + Send + Sync>> {
        if !self.config.signal_handling {
            return Ok(());
        }

        let shutdown_tx = self.shutdown_tx.clone();
        
        // Handle SIGTERM
        let shutdown_tx_sigterm = shutdown_tx.clone();
        tokio::spawn(async move {
            let mut sigterm = match tokio::signal::unix::signal(tokio::signal::unix::SignalKind::terminate()) {
                Ok(signal) => signal,
                Err(e) => {
                    error!("Failed to register SIGTERM handler: {}", e);
                    return;
                }
            };
            
            loop {
                sigterm.recv().await;
                info!("Received SIGTERM, initiating graceful shutdown");
                let _ = shutdown_tx_sigterm.send(ShutdownReason::Signal(15));
                break;
            }
        });

        // Handle SIGINT (Ctrl+C)
        let shutdown_tx_sigint = shutdown_tx.clone();
        tokio::spawn(async move {
            loop {
                tokio::signal::ctrl_c().await.expect("Failed to wait for SIGINT");
                info!("Received SIGINT, initiating graceful shutdown");
                let _ = shutdown_tx_sigint.send(ShutdownReason::Signal(2));
            }
        });

        Ok(())
    }

    pub fn shutdown_sender(&self) -> broadcast::Sender<ShutdownReason> {
        self.shutdown_tx.clone()
    }

    pub fn subscribe(&self) -> broadcast::Receiver<ShutdownReason> {
        self.shutdown_tx.subscribe()
    }

    pub async fn is_shutting_down(&self) -> bool {
        *self.is_shutting_down.read().await
    }

    pub async fn shutdown_reason(&self) -> Option<ShutdownReason> {
        self.shutdown_reason.lock().await.clone()
    }

    pub async fn initiate_shutdown(&self, reason: ShutdownReason) {
        let mut is_shutting_down = self.is_shutting_down.write().await;
        if *is_shutting_down {
            debug!("Shutdown already in progress");
            return;
        }
        
        *is_shutting_down = true;
        drop(is_shutting_down);

        let mut shutdown_reason = self.shutdown_reason.lock().await;
        *shutdown_reason = Some(reason.clone());
        drop(shutdown_reason);

        let mut shutdown_started = self.shutdown_started.write().await;
        *shutdown_started = Some(Instant::now());
        drop(shutdown_started);

        info!("Initiating graceful shutdown: {:?}", reason);
        
        // Notify all subscribers
        let _ = self.shutdown_tx.send(reason);
    }

    pub async fn wait_for_shutdown(&self) -> ShutdownReason {
        let mut rx = self.subscribe();
        rx.recv().await.expect("Shutdown channel closed")
    }

    pub async fn graceful_shutdown<F, Fut>(&self, cleanup_fn: F) -> Result<(), String>
    where
        F: FnOnce() -> Fut,
        Fut: std::future::Future<Output = Result<(), String>>,
    {
        if !self.is_shutting_down().await {
            return Err("Shutdown not initiated".to_string());
        }

        let start_time = Instant::now();
        
        info!("Starting graceful shutdown process");
        
        // Phase 1: Stop accepting new connections
        info!("Phase 1: Stopping new connections");
        self.stop_new_connections().await;
        
        // Phase 2: Drain existing connections
        info!("Phase 2: Draining existing connections");
        self.drain_connections().await;
        
        // Phase 3: Execute cleanup function
        info!("Phase 3: Executing cleanup");
        if let Err(e) = timeout(self.config.graceful_timeout, cleanup_fn()).await {
            match e {
                tokio::time::error::Elapsed { .. } => {
                    warn!("Cleanup function timed out after {:?}", self.config.graceful_timeout);
                }
                _ => {
                    error!("Cleanup function failed: {}", e);
                }
            }
        }
        
        let elapsed = start_time.elapsed();
        info!("Graceful shutdown completed in {:?}", elapsed);
        
        Ok(())
    }

    async fn stop_new_connections(&self) {
        // This would be implemented to stop accepting new connections
        // For now, just log the action
        debug!("Stopping new connection acceptance");
        sleep(Duration::from_millis(100)).await; // Simulate work
    }

    async fn drain_connections(&self) {
        // This would be implemented to gracefully close existing connections
        debug!("Draining existing connections");
        
        let drain_result = timeout(self.config.drain_timeout, async {
            // Simulate draining connections
            sleep(Duration::from_millis(500)).await;
        }).await;
        
        if drain_result.is_err() {
            warn!("Connection drain timed out after {:?}", self.config.drain_timeout);
        }
    }

    pub async fn force_shutdown(&self) {
        warn!("Force shutdown initiated");
        
        // This would implement immediate termination
        // For production, this might involve:
        // - Killing all remaining connections
        // - Flushing logs
        // - Saving critical state
        
        std::process::exit(1);
    }

    pub async fn shutdown_with_timeout<F, Fut>(&self, cleanup_fn: F) -> Result<(), String>
    where
        F: FnOnce() -> Fut,
        Fut: std::future::Future<Output = Result<(), String>>,
    {
        let graceful_result = timeout(
            self.config.graceful_timeout,
            self.graceful_shutdown(cleanup_fn)
        ).await;
        
        match graceful_result {
            Ok(Ok(())) => {
                info!("Graceful shutdown completed successfully");
                Ok(())
            }
            Ok(Err(e)) => {
                error!("Graceful shutdown failed: {}", e);
                self.force_shutdown().await;
                Err(e)
            }
            Err(_) => {
                warn!("Graceful shutdown timed out after {:?}", self.config.graceful_timeout);
                self.force_shutdown().await;
                Err("Graceful shutdown timeout".to_string())
            }
        }
    }
}

pub struct ShutdownGuard {
    manager: Arc<ShutdownManager>,
    reason: Option<ShutdownReason>,
}

impl ShutdownGuard {
    pub fn new(manager: Arc<ShutdownManager>) -> Self {
        Self {
            manager,
            reason: None,
        }
    }

    pub async fn wait_for_shutdown_signal(&mut self) -> ShutdownReason {
        let reason = self.manager.wait_for_shutdown().await;
        self.reason = Some(reason.clone());
        reason
    }

    pub async fn execute_shutdown<F, Fut>(&self, cleanup_fn: F) -> Result<(), String>
    where
        F: FnOnce() -> Fut,
        Fut: std::future::Future<Output = Result<(), String>>,
    {
        if let Some(reason) = &self.reason {
            info!("Executing shutdown due to: {:?}", reason);
            self.manager.shutdown_with_timeout(cleanup_fn).await
        } else {
            Err("No shutdown reason available".to_string())
        }
    }
}

impl Drop for ShutdownGuard {
    fn drop(&mut self) {
        if let Some(reason) = &self.reason {
            info!("ShutdownGuard dropped, reason: {:?}", reason);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_shutdown_manager() {
        let config = ShutdownConfig::default();
        let manager = ShutdownManager::new(config);
        
        assert!(!manager.is_shutting_down().await);
        assert!(manager.shutdown_reason().await.is_none());
        
        manager.initiate_shutdown(ShutdownReason::Request).await;
        
        assert!(manager.is_shutting_down().await);
        assert_eq!(manager.shutdown_reason().await, Some(ShutdownReason::Request));
    }

    #[tokio::test]
    async fn test_shutdown_subscription() {
        let config = ShutdownConfig::default();
        let manager = ShutdownManager::new(config);
        
        let mut rx = manager.subscribe();
        
        // Initiate shutdown
        manager.initiate_shutdown(ShutdownReason::Signal(15)).await;
        
        // Wait for shutdown signal
        let reason = rx.recv().await.unwrap();
        assert_eq!(reason, ShutdownReason::Signal(15));
    }
}
