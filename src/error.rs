/*
 * Copyright (c) 2025 pgElephant. All rights reserved.
 *
 * FauxDB - Production-ready MongoDB-compatible database server
 * Built with Rust for superior performance and reliability
 *
 * @file error.rs
 * @brief FauxDB error handling
 */

use thiserror::Error;

#[derive(Error, Debug)]
pub enum FauxDBError {
    #[error("Database error: {0}")]
    Database(String),

    #[error("BSON error: {0}")]
    Bson(#[from] bson::de::Error),

    #[error("BSON serialization error: {0}")]
    BsonSerialization(#[from] bson::ser::Error),

    #[error("Wire protocol error: {0}")]
    WireProtocol(String),

    #[error("Network error: {0}")]
    Network(#[from] std::io::Error),

    #[error("Serialization error: {0}")]
    Serialization(#[from] serde_json::Error),

    #[error("Connection pool error: {0}")]
    ConnectionPool(String),
}

pub type Result<T> = std::result::Result<T, FauxDBError>;
