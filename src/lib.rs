/*
 * Copyright (c) 2025 pgElephant. All rights reserved.
 *
 * FauxDB - Production-ready MongoDB-compatible database server
 * Built with Rust for superior performance and reliability
 *
 * @file lib.rs
 * @brief FauxDB library entry point
 */

pub mod error;
pub mod config;
pub mod postgresql_manager;
pub mod postgresql_server;

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
pub use postgresql_manager::PostgreSQLManager;
pub use postgresql_server::PostgreSQLServer;
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
