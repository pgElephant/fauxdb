/*!
 * Production-ready configuration management for FauxDB
 */

use serde::{Deserialize, Serialize};
use std::time::Duration;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProductionConfig {
    pub server: ServerConfig,
    pub database: DatabaseConfig,
    pub security: SecurityConfig,
    pub performance: PerformanceConfig,
    pub monitoring: MonitoringConfig,
    pub logging: LoggingConfig,
    pub clustering: ClusteringConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ServerConfig {
    pub host: String,
    pub port: u16,
    pub max_connections: usize,
    pub connection_timeout: Duration,
    pub keep_alive: Duration,
    pub tcp_nodelay: bool,
    pub worker_threads: Option<usize>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DatabaseConfig {
    pub connection_string: String,
    pub pool_size: u32,
    pub max_lifetime: Duration,
    pub idle_timeout: Duration,
    pub connection_timeout: Duration,
    pub statement_cache_size: usize,
    pub enable_ssl: bool,
    pub ssl_mode: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SecurityConfig {
    pub enable_auth: bool,
    pub auth_mechanisms: Vec<String>,
    pub ssl_enabled: bool,
    pub ssl_cert_path: Option<String>,
    pub ssl_key_path: Option<String>,
    pub ssl_ca_path: Option<String>,
    pub allowed_hosts: Vec<String>,
    pub rate_limiting: RateLimitConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RateLimitConfig {
    pub enabled: bool,
    pub requests_per_minute: u32,
    pub burst_size: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PerformanceConfig {
    pub query_cache_size: usize,
    pub query_cache_ttl: Duration,
    pub batch_size: usize,
    pub parallel_workers: usize,
    pub memory_limit: String,
    pub enable_compression: bool,
    pub compression_level: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MonitoringConfig {
    pub metrics_enabled: bool,
    pub metrics_port: u16,
    pub health_check_port: u16,
    pub prometheus_endpoint: String,
    pub enable_profiling: bool,
    pub slow_query_threshold: Duration,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LoggingConfig {
    pub level: String,
    pub format: String,
    pub output: String,
    pub file_path: Option<String>,
    pub max_file_size: String,
    pub max_files: usize,
    pub enable_structured_logging: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ClusteringConfig {
    pub enabled: bool,
    pub node_id: String,
    pub cluster_nodes: Vec<String>,
    pub election_timeout: Duration,
    pub heartbeat_interval: Duration,
    pub replication_factor: u32,
}

impl Default for ProductionConfig {
    fn default() -> Self {
        Self {
            server: ServerConfig::default(),
            database: DatabaseConfig::default(),
            security: SecurityConfig::default(),
            performance: PerformanceConfig::default(),
            monitoring: MonitoringConfig::default(),
            logging: LoggingConfig::default(),
            clustering: ClusteringConfig::default(),
        }
    }
}

impl Default for ServerConfig {
    fn default() -> Self {
        Self {
            host: "0.0.0.0".to_string(),
            port: 27018,
            max_connections: 1000,
            connection_timeout: Duration::from_secs(30),
            keep_alive: Duration::from_secs(60),
            tcp_nodelay: true,
            worker_threads: None,
        }
    }
}

impl Default for DatabaseConfig {
    fn default() -> Self {
        Self {
            connection_string: "postgresql://localhost/fauxdb".to_string(),
            pool_size: 20,
            max_lifetime: Duration::from_secs(1800),
            idle_timeout: Duration::from_secs(600),
            connection_timeout: Duration::from_secs(10),
            statement_cache_size: 1000,
            enable_ssl: false,
            ssl_mode: "prefer".to_string(),
        }
    }
}

impl Default for SecurityConfig {
    fn default() -> Self {
        Self {
            enable_auth: false,
            auth_mechanisms: vec!["SCRAM-SHA-1".to_string(), "SCRAM-SHA-256".to_string()],
            ssl_enabled: false,
            ssl_cert_path: None,
            ssl_key_path: None,
            ssl_ca_path: None,
            allowed_hosts: vec!["0.0.0.0/0".to_string()],
            rate_limiting: RateLimitConfig::default(),
        }
    }
}

impl Default for RateLimitConfig {
    fn default() -> Self {
        Self {
            enabled: true,
            requests_per_minute: 1000,
            burst_size: 100,
        }
    }
}

impl Default for PerformanceConfig {
    fn default() -> Self {
        Self {
            query_cache_size: 10000,
            query_cache_ttl: Duration::from_secs(300),
            batch_size: 1000,
            parallel_workers: num_cpus::get(),
            memory_limit: "2GB".to_string(),
            enable_compression: true,
            compression_level: 6,
        }
    }
}

impl Default for MonitoringConfig {
    fn default() -> Self {
        Self {
            metrics_enabled: true,
            metrics_port: 9090,
            health_check_port: 8080,
            prometheus_endpoint: "/metrics".to_string(),
            enable_profiling: false,
            slow_query_threshold: Duration::from_millis(100),
        }
    }
}

impl Default for LoggingConfig {
    fn default() -> Self {
        Self {
            level: "info".to_string(),
            format: "json".to_string(),
            output: "stdout".to_string(),
            file_path: None,
            max_file_size: "100MB".to_string(),
            max_files: 10,
            enable_structured_logging: true,
        }
    }
}

impl Default for ClusteringConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            node_id: uuid::Uuid::new_v4().to_string(),
            cluster_nodes: vec![],
            election_timeout: Duration::from_millis(5000),
            heartbeat_interval: Duration::from_millis(1000),
            replication_factor: 3,
        }
    }
}

impl ProductionConfig {
    pub fn load_from_file(path: &str) -> anyhow::Result<Self> {
        let content = std::fs::read_to_string(path)?;
        let config: ProductionConfig = toml::from_str(&content)?;
        Ok(config)
    }

    pub fn load_from_env() -> anyhow::Result<Self> {
        let mut config = ProductionConfig::default();
        
        // Override with environment variables
        if let Ok(host) = std::env::var("FAUXDB_HOST") {
            config.server.host = host;
        }
        if let Ok(port) = std::env::var("FAUXDB_PORT") {
            config.server.port = port.parse()?;
        }
        if let Ok(db_url) = std::env::var("FAUXDB_DATABASE_URL") {
            config.database.connection_string = db_url;
        }
        if let Ok(enable_auth) = std::env::var("FAUXDB_ENABLE_AUTH") {
            config.security.enable_auth = enable_auth.parse()?;
        }
        if let Ok(ssl_enabled) = std::env::var("FAUXDB_SSL_ENABLED") {
            config.security.ssl_enabled = ssl_enabled.parse()?;
        }
        
        Ok(config)
    }

    pub fn validate(&self) -> anyhow::Result<()> {
        if self.server.max_connections == 0 {
            return Err(anyhow::anyhow!("max_connections must be greater than 0"));
        }
        if self.database.pool_size == 0 {
            return Err(anyhow::anyhow!("database pool_size must be greater than 0"));
        }
        if self.security.ssl_enabled {
            if self.security.ssl_cert_path.is_none() || self.security.ssl_key_path.is_none() {
                return Err(anyhow::anyhow!("SSL certificate and key paths required when SSL is enabled"));
            }
        }
        Ok(())
    }
}
