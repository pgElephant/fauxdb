/*!
 * @file lib.rs
 * @brief FauxDB library entry point
 */

pub mod error;
pub mod config;
pub mod documentdb_server;

// Production-ready modules
pub mod production_config;
pub mod connection_pool;
pub mod mongodb_commands;
pub mod aggregation_pipeline;
pub mod indexing;
pub mod transactions;
pub mod production_server;
pub mod logger;
pub mod process_manager;
pub mod security;
pub mod monitoring;
pub mod replication;
pub mod wire_protocol;

// Re-export main types for external use
pub use error::{FauxDBError, Result};
pub use config::Config;
pub use documentdb_server::DocumentDBServer;
pub use production_config::ProductionConfig;
pub use connection_pool::ProductionConnectionPool;
pub use mongodb_commands::MongoDBCommandRegistry;
pub use aggregation_pipeline::AggregationPipeline;
pub use indexing::IndexManager;
pub use transactions::TransactionManager;
pub use production_server::ProductionFauxDBServer;
pub use logger::{LogLevel, FauxDBLogger, init_logger, init_tracing_logger};
pub use process_manager::{ProcessManager, ProcessConfig};
pub use security::{SecurityManager, User, Role, Privilege, AuthSession, AuthMechanism};
pub use monitoring::{MonitoringManager, MetricsSnapshot, HealthReport, Alert, MetricsConfig};
pub use replication::{ReplicationManager, ClusterManager, ReplicaSetConfig, ReplicaSetMember, ReplicationConfig};
