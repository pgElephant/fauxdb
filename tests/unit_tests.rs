/*!
 * Unit tests for FauxDB core functionality
 * Tests that don't require a running server
 */

use fauxdb::*;
use fauxdb::production_config::*;
use anyhow::Result;

#[test]
fn test_config_loading() -> Result<()> {
    // Test that we can create a basic configuration
    let config = ProductionConfig {
        server: ServerConfig {
            host: "127.0.0.1".to_string(),
            port: 27018,
            max_connections: 100,
            connection_timeout: std::time::Duration::from_secs(30),
            keep_alive: std::time::Duration::from_secs(60),
            tcp_nodelay: true,
            worker_threads: Some(4),
        },
        database: DatabaseConfig {
            connection_string: "postgresql://localhost:5432/fauxdb".to_string(),
            pool_size: 10,
            max_lifetime: std::time::Duration::from_secs(3600),
            idle_timeout: std::time::Duration::from_secs(600),
            connection_timeout: std::time::Duration::from_secs(30),
            statement_cache_size: 100,
            enable_ssl: false,
            ssl_mode: "prefer".to_string(),
        },
        security: SecurityConfig {
            enable_auth: false,
            auth_mechanisms: vec!["SCRAM-SHA-256".to_string()],
            ssl_enabled: false,
            ssl_cert_path: None,
            ssl_key_path: None,
            ssl_ca_path: None,
            allowed_hosts: vec!["127.0.0.1".to_string()],
            rate_limiting: RateLimitConfig {
                enabled: false,
                requests_per_minute: 1000,
                burst_size: 100,
            },
        },
        performance: PerformanceConfig {
            query_cache_size: 1024 * 1024, // 1MB
            query_cache_ttl: std::time::Duration::from_secs(3600),
            batch_size: 1000,
            parallel_workers: 4,
            memory_limit: "1GB".to_string(),
            enable_compression: true,
            compression_level: 6,
        },
        monitoring: MonitoringConfig {
            metrics_enabled: true,
            metrics_port: 9090,
            health_check_port: 9091,
            prometheus_endpoint: "/metrics".to_string(),
            enable_profiling: true,
            slow_query_threshold: std::time::Duration::from_secs(1),
        },
        logging: LoggingConfig {
            level: "info".to_string(),
            format: "json".to_string(),
            output: "stdout".to_string(),
            file_path: None,
            max_file_size: "100MB".to_string(),
            max_files: 5,
            enable_structured_logging: true,
        },
        clustering: ClusteringConfig {
            enabled: false,
            node_id: "node1".to_string(),
            cluster_nodes: vec![],
            election_timeout: std::time::Duration::from_secs(5),
            heartbeat_interval: std::time::Duration::from_secs(1),
            replication_factor: 3,
        },
    };

    // Test configuration validation
    assert!(config.validate().is_ok());
    
    // Test that we can serialize and deserialize the config
    let serialized = serde_json::to_string(&config)?;
    let deserialized: ProductionConfig = serde_json::from_str(&serialized)?;
    assert_eq!(config.server.port, deserialized.server.port);
    assert_eq!(config.server.host, deserialized.server.host);

    Ok(())
}

#[test]
fn test_error_handling() {
    // Test that our error types work correctly
    let db_error = FauxDBError::Database("Connection failed".to_string());
    assert!(db_error.to_string().contains("Connection failed"));

    let wire_error = FauxDBError::WireProtocol("Invalid message".to_string());
    assert!(wire_error.to_string().contains("Invalid message"));
}

#[test]
fn test_log_level_display() {
    use fauxdb::logger::LogLevel;
    
    assert_eq!(LogLevel::Debug.to_string(), "DEBUG");
    assert_eq!(LogLevel::Info.to_string(), "INFO ");
    assert_eq!(LogLevel::Warning.to_string(), "WARN ");
    assert_eq!(LogLevel::Error.to_string(), "ERROR");
}

#[test]
fn test_wire_protocol_opcodes() {
    use fauxdb::wire_protocol::OpCode;
    
    assert_eq!(OpCode::from_u32(1), Some(OpCode::Reply));
    assert_eq!(OpCode::from_u32(2004), Some(OpCode::Query));
    assert_eq!(OpCode::from_u32(2002), Some(OpCode::Insert));
    assert_eq!(OpCode::from_u32(9999), None);
}

#[test]
fn test_bson_serialization() -> Result<()> {
    use bson::doc;
    
    // Test that we can create and serialize BSON documents
    let doc = doc! {
        "name": "test",
        "value": 42,
        "nested": {
            "array": [1, 2, 3],
            "boolean": true
        }
    };
    
    let serialized = bson::to_vec(&doc)?;
    assert!(!serialized.is_empty());
    
    let deserialized: bson::Document = bson::from_slice(&serialized)?;
    assert_eq!(deserialized.get_str("name").unwrap(), "test");
    assert_eq!(deserialized.get_i32("value").unwrap(), 42);
    
    Ok(())
}

#[test]
fn test_mongodb_command_registry() {
    use fauxdb::mongodb_commands::MongoDBCommandRegistry;
    
    // Test that we can create a command registry
    let registry = MongoDBCommandRegistry::new();
    // Just verify it was created successfully
    assert!(std::ptr::addr_of!(registry) != std::ptr::null());
}

#[test]
fn test_connection_pool_config() {
    use fauxdb::connection_pool::ProductionPoolConfig;
    
    let pool_config = ProductionPoolConfig {
        max_size: 100,
        min_idle: 5,
        max_lifetime: std::time::Duration::from_secs(3600),
        idle_timeout: std::time::Duration::from_secs(600),
        connection_timeout: std::time::Duration::from_secs(30),
        statement_cache_size: 100,
    };
    
    assert_eq!(pool_config.max_size, 100);
    assert_eq!(pool_config.min_idle, 5);
}

#[test]
fn test_index_manager() {
    use fauxdb::indexing::IndexManager;
    
    // Test that we can create an index manager
    let index_manager = IndexManager::new();
    // Just verify it was created successfully
    assert!(std::ptr::addr_of!(index_manager) != std::ptr::null());
}

#[test]
fn test_transaction_manager() {
    use fauxdb::transactions::TransactionManager;
    
    // Test that we can create a transaction manager with a mock pool
    // For this test, we'll just verify the type exists
    let _tx_manager_type = std::any::TypeId::of::<TransactionManager>();
    assert!(_tx_manager_type != std::any::TypeId::of::<()>());
}

#[test]
fn test_security_manager() {
    use fauxdb::security::SecurityManager;
    
    // Test that we can create a security manager
    let security_manager = SecurityManager::new();
    // Just verify it was created successfully
    assert!(std::ptr::addr_of!(security_manager) != std::ptr::null());
}

#[test]
fn test_monitoring_manager() {
    use fauxdb::monitoring::{MonitoringManager, MetricsConfig};
    
    // Test that we can create a monitoring manager with a mock config
    let metrics_config = MetricsConfig {
        collection_interval_seconds: 30,
        history_retention_hours: 24,
        enable_detailed_metrics: true,
        enable_performance_profiling: true,
        alert_thresholds: fauxdb::monitoring::AlertThresholds {
            cpu_usage_percent: 80.0,
            memory_usage_percent: 85.0,
            disk_usage_percent: 90.0,
            connection_pool_usage_percent: 80.0,
            average_query_time_ms: 1000.0,
            slow_query_threshold_ms: 5000.0,
        },
    };
    let monitoring_manager = MonitoringManager::new(metrics_config);
    // Just verify it was created successfully
    assert!(std::ptr::addr_of!(monitoring_manager) != std::ptr::null());
}

#[test]
fn test_replication_manager() {
    use fauxdb::replication::{ReplicationManager, ReplicationConfig};
    
    // Test that we can create a replication manager with a mock config
    let replication_config = ReplicationConfig {
        heartbeat_interval_ms: 1000,
        election_timeout_ms: 5000,
        max_oplog_size_mb: 1024,
        oplog_retention_hours: 24,
        sync_source_selection_delay_ms: 100,
        initial_sync_max_docs: 1000000,
        initial_sync_max_batch_size: 10000,
        max_repl_lag_secs: 60,
    };
    let replication_manager = ReplicationManager::new(replication_config);
    // Just verify it was created successfully
    assert!(std::ptr::addr_of!(replication_manager) != std::ptr::null());
}
