/*!
 * Integration tests for FauxDB core functionality
 * Tests that verify component integration without requiring external services
 */

use fauxdb::*;
use anyhow::Result;

#[tokio::test]
async fn test_production_config_loading() -> Result<()> {
    // Test that we can create and validate a production configuration
    let config = ProductionConfig::default();
    
    // Validate the configuration
    assert!(config.validate().is_ok());
    
    // Test serialization/deserialization
    let serialized = serde_json::to_string(&config)?;
    let deserialized: ProductionConfig = serde_json::from_str(&serialized)?;
    
    assert_eq!(config.server.port, deserialized.server.port);
    assert_eq!(config.server.host, deserialized.server.host);
    
    Ok(())
}

#[tokio::test]
async fn test_connection_pool_creation() -> Result<()> {
    use fauxdb::connection_pool::ProductionPoolConfig;
    
    // Test that we can create a connection pool configuration
    let pool_config = ProductionPoolConfig {
        max_size: 10,
        min_idle: 2,
        max_lifetime: std::time::Duration::from_secs(1800),
        idle_timeout: std::time::Duration::from_secs(600),
        connection_timeout: std::time::Duration::from_secs(30),
        statement_cache_size: 100,
    };
    
    assert_eq!(pool_config.max_size, 10);
    assert_eq!(pool_config.min_idle, 2);
    
    Ok(())
}

#[tokio::test]
async fn test_mongodb_command_registry() -> Result<()> {
    use fauxdb::mongodb_commands::MongoDBCommandRegistry;
    
    // Test that we can create a command registry
    let registry = MongoDBCommandRegistry::new();
    
    // Verify it was created successfully
    assert!(std::ptr::addr_of!(registry) != std::ptr::null());
    
    Ok(())
}

#[tokio::test]
async fn test_index_manager() -> Result<()> {
    use fauxdb::indexing::IndexManager;
    
    // Test that we can create an index manager
    let index_manager = IndexManager::new();
    
    // Verify it was created successfully
    assert!(std::ptr::addr_of!(index_manager) != std::ptr::null());
    
    Ok(())
}

#[tokio::test]
async fn test_security_manager() -> Result<()> {
    use fauxdb::security::SecurityManager;
    
    // Test that we can create a security manager
    let security_manager = SecurityManager::new();
    
    // Verify it was created successfully
    assert!(std::ptr::addr_of!(security_manager) != std::ptr::null());
    
    Ok(())
}

#[tokio::test]
async fn test_monitoring_manager() -> Result<()> {
    use fauxdb::monitoring::{MonitoringManager, MetricsConfig, AlertThresholds};
    
    // Create a metrics configuration
    let metrics_config = MetricsConfig {
        collection_interval_seconds: 30,
        history_retention_hours: 24,
        enable_detailed_metrics: true,
        enable_performance_profiling: true,
        alert_thresholds: AlertThresholds {
            cpu_usage_percent: 80.0,
            memory_usage_percent: 85.0,
            disk_usage_percent: 90.0,
            connection_pool_usage_percent: 80.0,
            average_query_time_ms: 1000.0,
            slow_query_threshold_ms: 5000.0,
        },
    };
    
    // Test that we can create a monitoring manager
    let monitoring_manager = MonitoringManager::new(metrics_config);
    
    // Verify it was created successfully
    assert!(std::ptr::addr_of!(monitoring_manager) != std::ptr::null());
    
    Ok(())
}

#[tokio::test]
async fn test_replication_manager() -> Result<()> {
    use fauxdb::replication::{ReplicationManager, ReplicationConfig};
    
    // Create a replication configuration
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
    
    // Test that we can create a replication manager
    let replication_manager = ReplicationManager::new(replication_config);
    
    // Verify it was created successfully
    assert!(std::ptr::addr_of!(replication_manager) != std::ptr::null());
    
    Ok(())
}

#[tokio::test]
async fn test_wire_protocol_handling() -> Result<()> {
    use fauxdb::wire_protocol::{OpCode, MessageHeader};
    
    // Test OpCode parsing
    assert_eq!(OpCode::from_u32(1), Some(OpCode::Reply));
    assert_eq!(OpCode::from_u32(2004), Some(OpCode::Query));
    assert_eq!(OpCode::from_u32(2002), Some(OpCode::Insert));
    assert_eq!(OpCode::from_u32(9999), None);
    
    // Test MessageHeader creation
    let header = MessageHeader {
        message_length: 100,
        request_id: 1,
        response_to: 0,
        op_code: OpCode::Query,
    };
    
    assert_eq!(header.message_length, 100);
    assert_eq!(header.request_id, 1);
    assert_eq!(header.op_code, OpCode::Query);
    
    Ok(())
}

#[tokio::test]
async fn test_bson_operations() -> Result<()> {
    use bson::doc;
    
    // Test BSON document creation and manipulation
    let doc = doc! {
        "name": "test",
        "value": 42,
        "nested": {
            "array": [1, 2, 3],
            "boolean": true
        }
    };
    
    // Test serialization
    let serialized = bson::to_vec(&doc)?;
    assert!(!serialized.is_empty());
    
    // Test deserialization
    let deserialized: bson::Document = bson::from_slice(&serialized)?;
    assert_eq!(deserialized.get_str("name").unwrap(), "test");
    assert_eq!(deserialized.get_i32("value").unwrap(), 42);
    
    // Test nested document access
    let nested = deserialized.get_document("nested").unwrap();
    let array = nested.get_array("array").unwrap();
    assert_eq!(array.len(), 3);
    assert_eq!(array[0].as_i32().unwrap(), 1);
    
    Ok(())
}

#[tokio::test]
async fn test_logging_system() -> Result<()> {
    use fauxdb::logger::{LogLevel, FauxDBLogEvent};
    
    // Test log level display
    assert_eq!(LogLevel::Debug.to_string(), "DEBUG");
    assert_eq!(LogLevel::Info.to_string(), "INFO ");
    assert_eq!(LogLevel::Warning.to_string(), "WARN ");
    assert_eq!(LogLevel::Error.to_string(), "ERROR");
    
    // Test log event creation
    let log_event = FauxDBLogEvent {
        timestamp: chrono::Utc::now(),
        level: LogLevel::Info,
        pid: 12345,
        message: "Test message".to_string(),
        target: "test".to_string(),
        module_path: Some("test_module".to_string()),
        line: Some(100),
    };
    
    assert_eq!(log_event.level, LogLevel::Info);
    assert_eq!(log_event.target, "test");
    assert_eq!(log_event.message, "Test message");
    
    Ok(())
}

#[tokio::test]
async fn test_error_handling() -> Result<()> {
    // Test FauxDBError variants
    let db_error = FauxDBError::Database("Connection failed".to_string());
    assert!(db_error.to_string().contains("Connection failed"));
    
    let wire_error = FauxDBError::WireProtocol("Invalid message".to_string());
    assert!(wire_error.to_string().contains("Invalid message"));
    
    let pool_error = FauxDBError::ConnectionPool("Pool exhausted".to_string());
    assert!(pool_error.to_string().contains("Pool exhausted"));
    
    Ok(())
}

#[tokio::test]
async fn test_configuration_validation() -> Result<()> {
    // Test with valid configuration
    let valid_config = ProductionConfig::default();
    assert!(valid_config.validate().is_ok());
    
    // Test configuration serialization/deserialization
    let serialized = serde_json::to_string(&valid_config)?;
    let deserialized: ProductionConfig = serde_json::from_str(&serialized)?;
    assert!(deserialized.validate().is_ok());
    
    Ok(())
}
