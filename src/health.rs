/*
 * Copyright (c) 2025 pgElephant. All rights reserved.
 *
 * FauxDB - Production-ready MongoDB-compatible database server
 * Built with Rust for superior performance and reliability
 *
 * @file health.rs
 * @brief Health checks and monitoring endpoints
 */

use serde::{Serialize, Deserialize};
use std::collections::HashMap;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};
use tokio::time::timeout;
use deadpool_postgres::Pool;
// use metrics::{counter, histogram, gauge}; // Temporarily disabled

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthStatus {
    pub status: HealthState,
    pub timestamp: u64,
    pub uptime_seconds: u64,
    pub version: String,
    pub checks: HashMap<String, HealthCheck>,
    pub metrics: SystemMetrics,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum HealthState {
    #[serde(rename = "healthy")]
    Healthy,
    #[serde(rename = "degraded")]
    Degraded,
    #[serde(rename = "unhealthy")]
    Unhealthy,
}

impl HealthState {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Healthy => "healthy",
            Self::Degraded => "degraded",
            Self::Unhealthy => "unhealthy",
        }
    }

    pub fn http_status(&self) -> u16 {
        match self {
            Self::Healthy => 200,
            Self::Degraded => 200, // Still operational but with warnings
            Self::Unhealthy => 503,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthCheck {
    pub status: HealthState,
    pub message: String,
    pub duration_ms: u64,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub details: Option<HashMap<String, String>>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SystemMetrics {
    pub memory: MemoryMetrics,
    pub database: DatabaseMetrics,
    pub network: NetworkMetrics,
    pub performance: PerformanceMetrics,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MemoryMetrics {
    pub used_bytes: u64,
    pub total_bytes: u64,
    pub free_bytes: u64,
    pub cache_bytes: u64,
    pub buffer_bytes: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DatabaseMetrics {
    pub active_connections: usize,
    pub idle_connections: usize,
    pub max_connections: usize,
    pub connection_utilization: f64,
    pub query_count: u64,
    pub error_count: u64,
    pub avg_query_time_ms: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NetworkMetrics {
    pub active_connections: usize,
    pub total_requests: u64,
    pub requests_per_second: f64,
    pub bytes_sent: u64,
    pub bytes_received: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PerformanceMetrics {
    pub cpu_usage_percent: f64,
    pub load_average: [f64; 3],
    pub disk_usage_percent: f64,
    pub disk_io_ops: u64,
}

pub struct HealthChecker {
    start_time: Instant,
    pool: Pool,
    version: String,
}

impl HealthChecker {
    pub fn new(pool: Pool, version: String) -> Self {
        Self {
            start_time: Instant::now(),
            pool,
            version,
        }
    }

    pub async fn check_health(&self) -> HealthStatus {
        let timestamp = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_secs();
        
        let uptime_seconds = self.start_time.elapsed().as_secs();
        
        let mut checks = HashMap::new();
        
        // Database connectivity check
        checks.insert("database".to_string(), self.check_database().await);
        
        // Memory check
        checks.insert("memory".to_string(), self.check_memory().await);
        
        // Disk space check
        checks.insert("disk".to_string(), self.check_disk_space().await);
        
        // Network connectivity check
        checks.insert("network".to_string(), self.check_network().await);
        
        // Determine overall health status
        let status = self.determine_overall_status(&checks);
        
        // Collect system metrics
        let metrics = self.collect_metrics().await;
        
        HealthStatus {
            status,
            timestamp,
            uptime_seconds,
            version: self.version.clone(),
            checks,
            metrics,
        }
    }

    async fn check_database(&self) -> HealthCheck {
        let start = Instant::now();
        
        match timeout(Duration::from_secs(5), async {
            let client = self.pool.get().await;
            match client {
                Ok(conn) => {
                    match conn.query_one("SELECT 1", &[]).await {
                        Ok(_) => HealthCheck {
                            status: HealthState::Healthy,
                            message: "Database connection successful".to_string(),
                            duration_ms: start.elapsed().as_millis() as u64,
                            details: None,
                        },
                        Err(e) => HealthCheck {
                            status: HealthState::Unhealthy,
                            message: format!("Database query failed: {}", e),
                            duration_ms: start.elapsed().as_millis() as u64,
                            details: Some(HashMap::from([
                                ("error".to_string(), e.to_string()),
                                ("error_type".to_string(), "query_error".to_string()),
                            ])),
                        },
                    }
                }
                Err(e) => HealthCheck {
                    status: HealthState::Unhealthy,
                    message: format!("Database connection failed: {}", e),
                    duration_ms: start.elapsed().as_millis() as u64,
                    details: Some(HashMap::from([
                        ("error".to_string(), e.to_string()),
                        ("error_type".to_string(), "connection_error".to_string()),
                    ])),
                },
            }
        }).await {
            Ok(result) => result,
            Err(_) => HealthCheck {
                status: HealthState::Unhealthy,
                message: "Database check timeout".to_string(),
                duration_ms: start.elapsed().as_millis() as u64,
                details: Some(HashMap::from([
                    ("error".to_string(), "timeout".to_string()),
                    ("timeout_seconds".to_string(), "5".to_string()),
                ])),
            },
        }
    }

    async fn check_memory(&self) -> HealthCheck {
        let start = Instant::now();
        
        // Get memory information from /proc/meminfo
        match tokio::fs::read_to_string("/proc/meminfo").await {
            Ok(content) => {
                let mut mem_info = HashMap::new();
                for line in content.lines() {
                    if let Some((key, value)) = line.split_once(':') {
                        let value = value.trim().split_whitespace().next().unwrap_or("0");
                        mem_info.insert(key.trim().to_string(), value.to_string());
                    }
                }
                
                let total = mem_info.get("MemTotal")
                    .and_then(|v| v.parse::<u64>().ok())
                    .unwrap_or(0) * 1024; // Convert from KB to bytes
                
                let available = mem_info.get("MemAvailable")
                    .and_then(|v| v.parse::<u64>().ok())
                    .unwrap_or(0) * 1024;
                
                let used = total - available;
                let usage_percent = if total > 0 { (used as f64 / total as f64) * 100.0 } else { 0.0 };
                
                let status = if usage_percent > 90.0 {
                    HealthState::Unhealthy
                } else if usage_percent > 80.0 {
                    HealthState::Degraded
                } else {
                    HealthState::Healthy
                };
                
                HealthCheck {
                    status,
                    message: format!("Memory usage: {:.1}%", usage_percent),
                    duration_ms: start.elapsed().as_millis() as u64,
                    details: Some(HashMap::from([
                        ("total_bytes".to_string(), total.to_string()),
                        ("used_bytes".to_string(), used.to_string()),
                        ("available_bytes".to_string(), available.to_string()),
                        ("usage_percent".to_string(), usage_percent.to_string()),
                    ])),
                }
            }
            Err(e) => HealthCheck {
                status: HealthState::Degraded,
                message: format!("Could not read memory info: {}", e),
                duration_ms: start.elapsed().as_millis() as u64,
                details: Some(HashMap::from([
                    ("error".to_string(), e.to_string()),
                ])),
            },
        }
    }

    async fn check_disk_space(&self) -> HealthCheck {
        let start = Instant::now();
        
        // Check disk space using df command
        match tokio::process::Command::new("df")
            .arg("-h")
            .arg("/")
            .output()
            .await
        {
            Ok(output) => {
                let output_str = String::from_utf8_lossy(&output.stdout);
                let lines: Vec<&str> = output_str.lines().collect();
                
                if lines.len() >= 2 {
                    let parts: Vec<&str> = lines[1].split_whitespace().collect();
                    if parts.len() >= 5 {
                        if let Ok(usage) = parts[4].trim_end_matches('%').parse::<f64>() {
                            let status = if usage > 95.0 {
                                HealthState::Unhealthy
                            } else if usage > 85.0 {
                                HealthState::Degraded
                            } else {
                                HealthState::Healthy
                            };
                            
                            HealthCheck {
                                status,
                                message: format!("Disk usage: {:.1}%", usage),
                                duration_ms: start.elapsed().as_millis() as u64,
                                details: Some(HashMap::from([
                                    ("filesystem".to_string(), parts[0].to_string()),
                                    ("total".to_string(), parts[1].to_string()),
                                    ("used".to_string(), parts[2].to_string()),
                                    ("available".to_string(), parts[3].to_string()),
                                    ("usage_percent".to_string(), usage.to_string()),
                                ])),
                            }
                        } else {
                            HealthCheck {
                                status: HealthState::Degraded,
                                message: "Could not parse disk usage".to_string(),
                                duration_ms: start.elapsed().as_millis() as u64,
                                details: None,
                            }
                        }
                    } else {
                        HealthCheck {
                            status: HealthState::Degraded,
                            message: "Unexpected df output format".to_string(),
                            duration_ms: start.elapsed().as_millis() as u64,
                            details: None,
                        }
                    }
                } else {
                    HealthCheck {
                        status: HealthState::Degraded,
                        message: "No disk usage data available".to_string(),
                        duration_ms: start.elapsed().as_millis() as u64,
                        details: None,
                    }
                }
            }
            Err(e) => HealthCheck {
                status: HealthState::Degraded,
                message: format!("Could not check disk space: {}", e),
                duration_ms: start.elapsed().as_millis() as u64,
                details: Some(HashMap::from([
                    ("error".to_string(), e.to_string()),
                ])),
            },
        }
    }

    async fn check_network(&self) -> HealthCheck {
        let start = Instant::now();
        
        // Simple network check - try to resolve a hostname
        match timeout(Duration::from_secs(3), async {
            tokio::net::lookup_host("8.8.8.8:53").await
        }).await {
            Ok(Ok(_)) => HealthCheck {
                status: HealthState::Healthy,
                message: "Network connectivity OK".to_string(),
                duration_ms: start.elapsed().as_millis() as u64,
                details: None,
            },
            Ok(Err(e)) => HealthCheck {
                status: HealthState::Degraded,
                message: format!("Network connectivity issue: {}", e),
                duration_ms: start.elapsed().as_millis() as u64,
                details: Some(HashMap::from([
                    ("error".to_string(), e.to_string()),
                ])),
            },
            Err(_) => HealthCheck {
                status: HealthState::Degraded,
                message: "Network check timeout".to_string(),
                duration_ms: start.elapsed().as_millis() as u64,
                details: Some(HashMap::from([
                    ("timeout_seconds".to_string(), "3".to_string()),
                ])),
            },
        }
    }

    fn determine_overall_status(&self, checks: &HashMap<String, HealthCheck>) -> HealthState {
        let mut has_unhealthy = false;
        let mut has_degraded = false;
        
        for check in checks.values() {
            match check.status {
                HealthState::Unhealthy => has_unhealthy = true,
                HealthState::Degraded => has_degraded = true,
                HealthState::Healthy => {}
            }
        }
        
        if has_unhealthy {
            HealthState::Unhealthy
        } else if has_degraded {
            HealthState::Degraded
        } else {
            HealthState::Healthy
        }
    }

    async fn collect_metrics(&self) -> SystemMetrics {
        SystemMetrics {
            memory: self.collect_memory_metrics().await,
            database: self.collect_database_metrics().await,
            network: self.collect_network_metrics().await,
            performance: self.collect_performance_metrics().await,
        }
    }

    async fn collect_memory_metrics(&self) -> MemoryMetrics {
        // This would be implemented with actual system calls
        // For now, return placeholder values
        MemoryMetrics {
            used_bytes: 1024 * 1024 * 512, // 512 MB
            total_bytes: 1024 * 1024 * 1024 * 8, // 8 GB
            free_bytes: 1024 * 1024 * 1024 * 7, // 7 GB
            cache_bytes: 1024 * 1024 * 256, // 256 MB
            buffer_bytes: 1024 * 1024 * 128, // 128 MB
        }
    }

    async fn collect_database_metrics(&self) -> DatabaseMetrics {
        let pool_status = self.pool.status();
        
        DatabaseMetrics {
            active_connections: pool_status.size - pool_status.available,
            idle_connections: pool_status.available,
            max_connections: pool_status.max_size,
            connection_utilization: if pool_status.max_size > 0 {
                (pool_status.size as f64 / pool_status.max_size as f64) * 100.0
            } else {
                0.0
            },
            query_count: 0, // Would be populated from actual metrics
            error_count: 0, // Would be populated from actual metrics
            avg_query_time_ms: 0.0, // Would be populated from actual metrics
        }
    }

    async fn collect_network_metrics(&self) -> NetworkMetrics {
        NetworkMetrics {
            active_connections: 0, // Would be populated from actual metrics
            total_requests: 0, // Would be populated from actual metrics
            requests_per_second: 0.0, // Would be populated from actual metrics
            bytes_sent: 0, // Would be populated from actual metrics
            bytes_received: 0, // Would be populated from actual metrics
        }
    }

    async fn collect_performance_metrics(&self) -> PerformanceMetrics {
        // This would be implemented with actual system monitoring
        // For now, return placeholder values
        PerformanceMetrics {
            cpu_usage_percent: 25.5,
            load_average: [0.5, 0.8, 1.2],
            disk_usage_percent: 45.0,
            disk_io_ops: 1234,
        }
    }
}
