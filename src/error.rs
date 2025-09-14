/*!
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

    #[error("MongoDB error: {0}")]
    MongoDB(String),

    #[error("Wire protocol error: {0}")]
    WireProtocol(String),

    #[error("Command error: {0}")]
    Command(String),

    #[error("Configuration error: {0}")]
    Config(String),

    #[error("Network error: {0}")]
    Network(#[from] std::io::Error),

    #[error("Serialization error: {0}")]
    Serialization(#[from] serde_json::Error),

    #[error("Connection pool error: {0}")]
    ConnectionPool(String),

    #[error("Authentication error: {0}")]
    Authentication(String),

    #[error("Authorization error: {0}")]
    Authorization(String),

    #[error("Internal error: {0}")]
    Internal(String),

    #[error("Transaction error: {0}")]
    Transaction(String),

    #[error("Geospatial error: {0}")]
    Geospatial(String),
}

pub type Result<T> = std::result::Result<T, FauxDBError>;
