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
use serde::{Serialize, Deserialize};

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

    #[error("Authentication error: {0}")]
    Authentication(String),

    #[error("Authorization error: {0}")]
    Authorization(String),

    #[error("Rate limit exceeded: {0}")]
    RateLimit(String),

    #[error("Validation error: {0}")]
    Validation(String),

    #[error("Configuration error: {0}")]
    Configuration(String),

    #[error("Resource exhausted: {0}")]
    ResourceExhausted(String),

    #[error("Timeout error: {0}")]
    Timeout(String),

    #[error("Circuit breaker open: {0}")]
    CircuitBreaker(String),

    #[error("Internal server error: {0}")]
    Internal(String),
}

pub type Result<T> = std::result::Result<T, FauxDBError>;

impl FauxDBError {
    /// Create a new database error
    pub fn database_error(message: impl Into<String>) -> Self {
        Self::Database(message.into())
    }

    /// Create a new wire protocol error
    pub fn wire_protocol_error(message: impl Into<String>) -> Self {
        Self::WireProtocol(message.into())
    }

    /// Create a new connection pool error
    pub fn connection_pool_error(message: impl Into<String>) -> Self {
        Self::ConnectionPool(message.into())
    }

    /// Create a new authentication error
    pub fn authentication_error(message: impl Into<String>) -> Self {
        Self::Authentication(message.into())
    }

    /// Create a new authorization error
    pub fn authorization_error(message: impl Into<String>) -> Self {
        Self::Authorization(message.into())
    }

    /// Create a new rate limit error
    pub fn rate_limit_error(message: impl Into<String>) -> Self {
        Self::RateLimit(message.into())
    }

    /// Create a new validation error
    pub fn validation_error(message: impl Into<String>) -> Self {
        Self::Validation(message.into())
    }

    /// Create a new configuration error
    pub fn configuration_error(message: impl Into<String>) -> Self {
        Self::Configuration(message.into())
    }

    /// Create a new resource exhausted error
    pub fn resource_exhausted_error(message: impl Into<String>) -> Self {
        Self::ResourceExhausted(message.into())
    }

    /// Create a new timeout error
    pub fn timeout_error(message: impl Into<String>) -> Self {
        Self::Timeout(message.into())
    }

    /// Create a new circuit breaker error
    pub fn circuit_breaker_error(message: impl Into<String>) -> Self {
        Self::CircuitBreaker(message.into())
    }

    /// Create a new internal server error
    pub fn internal_error(message: impl Into<String>) -> Self {
        Self::Internal(message.into())
    }

    /// Get error severity level for monitoring
    pub fn severity(&self) -> ErrorSeverity {
        match self {
            Self::Database(_) => ErrorSeverity::High,
            Self::WireProtocol(_) => ErrorSeverity::Medium,
            Self::Network(_) => ErrorSeverity::High,
            Self::ConnectionPool(_) => ErrorSeverity::High,
            Self::Authentication(_) => ErrorSeverity::Medium,
            Self::Authorization(_) => ErrorSeverity::Low,
            Self::RateLimit(_) => ErrorSeverity::Low,
            Self::Validation(_) => ErrorSeverity::Low,
            Self::Configuration(_) => ErrorSeverity::High,
            Self::ResourceExhausted(_) => ErrorSeverity::Critical,
            Self::Timeout(_) => ErrorSeverity::Medium,
            Self::CircuitBreaker(_) => ErrorSeverity::Medium,
            Self::Internal(_) => ErrorSeverity::Critical,
            Self::Bson(_) | Self::BsonSerialization(_) | Self::Serialization(_) => ErrorSeverity::Medium,
        }
    }

    /// Check if error is retryable
    pub fn is_retryable(&self) -> bool {
        match self {
            Self::Network(_) => true,
            Self::Timeout(_) => true,
            Self::ConnectionPool(_) => true,
            Self::CircuitBreaker(_) => false,
            Self::RateLimit(_) => true,
            Self::Database(_) => false, // Simplified for now
            _ => false,
        }
    }

    /// Get retry delay in milliseconds
    pub fn retry_delay_ms(&self) -> Option<u64> {
        match self {
            Self::RateLimit(_) => Some(1000),
            Self::CircuitBreaker(_) => Some(5000),
            _ => Some(1000), // Default 1 second
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ErrorSeverity {
    Low,
    Medium,
    High,
    Critical,
}

impl ErrorSeverity {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Low => "low",
            Self::Medium => "medium",
            Self::High => "high",
            Self::Critical => "critical",
        }
    }
}
