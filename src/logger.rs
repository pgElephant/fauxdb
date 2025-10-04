/*!
 * Production logging system for FauxDB
 * Structured logging with proper log levels and formatting
 */

use std::fmt;
use std::sync::Mutex;
use std::time::{SystemTime, UNIX_EPOCH};
use chrono::{DateTime, Utc};
use tracing::{Level, Subscriber};
use tracing_subscriber::{
    layer::SubscriberExt,
    util::SubscriberInitExt,
    EnvFilter, Registry,
};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LogLevel {
    Debug,
    Info,
    Warning,
    Error,
}

impl fmt::Display for LogLevel {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            LogLevel::Debug => write!(f, "DEBUG"),
            LogLevel::Info => write!(f, "INFO "),
            LogLevel::Warning => write!(f, "WARN "),
            LogLevel::Error => write!(f, "ERROR"),
        }
    }
}

impl From<Level> for LogLevel {
    fn from(level: Level) -> Self {
        match level {
            Level::TRACE | Level::DEBUG => LogLevel::Debug,
            Level::INFO => LogLevel::Info,
            Level::WARN => LogLevel::Warning,
            Level::ERROR => LogLevel::Error,
        }
    }
}

#[derive(Debug, Clone)]
pub struct FauxDBLogEvent {
    pub timestamp: DateTime<Utc>,
    pub level: LogLevel,
    pub pid: u32,
    pub message: String,
    pub target: String,
    pub module_path: Option<String>,
    pub line: Option<u32>,
}

impl FauxDBLogEvent {
    pub fn new(level: LogLevel, message: String) -> Self {
        Self {
            timestamp: Utc::now(),
            level,
            pid: std::process::id(),
            message,
            target: "FauxDB".to_string(),
            module_path: None,
            line: None,
        }
    }

    pub fn format_compact(&self) -> String {
        let symbol = match self.level {
            LogLevel::Debug => "•",
            LogLevel::Info => "✓",
            LogLevel::Warning => "⚠",
            LogLevel::Error => "✗",
        };

        format!(
            "{} {} {} : FauxDB [{}]",
            symbol,
            self.pid,
            self.timestamp.format("%Y-%m-%d %H:%M:%S%.3f"),
            self.message
        )
    }

    pub fn format_detailed(&self) -> String {
        let symbol = match self.level {
            LogLevel::Debug => "•",
            LogLevel::Info => "✓",
            LogLevel::Warning => "⚠",
            LogLevel::Error => "✗",
        };

        let module_info = if let Some(path) = &self.module_path {
            if let Some(line) = self.line {
                format!(" {}:{}", path, line)
            } else {
                format!(" {}", path)
            }
        } else {
            String::new()
        };

        format!(
            "{} {} {} : FauxDB [{}]{}{}",
            symbol,
            self.pid,
            self.timestamp.format("%Y-%m-%d %H:%M:%S%.3f"),
            self.message,
            module_info,
            if self.message.contains('\n') { "\n" } else { "" }
        )
    }
}

#[derive(Clone)]
pub struct FauxDBLogger {
    level: LogLevel,
    compact: bool,
}

impl FauxDBLogger {
    pub fn new(level: LogLevel) -> Self {
        Self {
            level,
            compact: true,
        }
    }

    pub fn with_detailed_format(mut self) -> Self {
        self.compact = false;
        self
    }

    pub fn log(&self, level: LogLevel, message: &str) {
        if level as u8 >= self.level as u8 {
            let event = FauxDBLogEvent::new(level, message.to_string());
            let formatted = if self.compact {
                event.format_compact()
            } else {
                event.format_detailed()
            };
            
            // Output to stderr for logs, stdout for normal output
            eprintln!("{}", formatted);
        }
    }

    pub fn debug(&self, message: &str) {
        self.log(LogLevel::Debug, message);
    }

    pub fn info(&self, message: &str) {
        self.log(LogLevel::Info, message);
    }

    pub fn warning(&self, message: &str) {
        self.log(LogLevel::Warning, message);
    }

    pub fn error(&self, message: &str) {
        self.log(LogLevel::Error, message);
    }
}

// Global logger instance
static GLOBAL_LOGGER: Mutex<Option<FauxDBLogger>> = Mutex::new(None);

pub fn init_logger(level: LogLevel, detailed: bool) {
    let logger = if detailed {
        FauxDBLogger::new(level).with_detailed_format()
    } else {
        FauxDBLogger::new(level)
    };
    
    let mut global = GLOBAL_LOGGER.lock().unwrap();
    *global = Some(logger);
}

pub fn get_logger() -> Option<FauxDBLogger> {
    GLOBAL_LOGGER.lock().unwrap().clone()
}

// Convenience macros for logging
#[macro_export]
macro_rules! fauxdb_debug {
    ($($arg:tt)*) => {
        if let Some(logger) = $crate::logger::get_logger() {
            logger.debug(&format!($($arg)*));
        }
    };
}

#[macro_export]
macro_rules! fauxdb_info {
    ($($arg:tt)*) => {
        if let Some(logger) = $crate::logger::get_logger() {
            logger.info(&format!($($arg)*));
        }
    };
}

#[macro_export]
macro_rules! fauxdb_warn {
    ($($arg:tt)*) => {
        if let Some(logger) = $crate::logger::get_logger() {
            logger.warning(&format!($($arg)*));
        }
    };
}

#[macro_export]
macro_rules! fauxdb_error {
    ($($arg:tt)*) => {
        if let Some(logger) = $crate::logger::get_logger() {
            logger.error(&format!($($arg)*));
        }
    };
}

// Tracing integration
pub struct FauxDBTracingLayer;

impl<S> tracing_subscriber::Layer<S> for FauxDBTracingLayer
where
    S: Subscriber + for<'a> tracing_subscriber::registry::LookupSpan<'a>,
{
    fn on_event(&self, event: &tracing::Event<'_>, _ctx: tracing_subscriber::layer::Context<'_, S>) {
        let metadata = event.metadata();
        let level = LogLevel::from(*metadata.level());
        
        let mut visitor = LogVisitor::default();
        event.record(&mut visitor);
        
        let message = visitor.message.unwrap_or_else(|| {
            metadata.name().to_string()
        });

        if let Some(logger) = get_logger() {
            logger.log(level, &message);
        }
    }
}

#[derive(Default)]
struct LogVisitor {
    message: Option<String>,
}

impl tracing::field::Visit for LogVisitor {
    fn record_debug(&mut self, field: &tracing::field::Field, value: &dyn fmt::Debug) {
        if field.name() == "message" {
            self.message = Some(format!("{:?}", value));
        }
    }
}

pub fn init_tracing_logger(level: LogLevel, detailed: bool) -> Result<(), Box<dyn std::error::Error>> {
    init_logger(level, detailed);
    
    let filter = match level {
        LogLevel::Debug => EnvFilter::new("debug"),
        LogLevel::Info => EnvFilter::new("info"),
        LogLevel::Warning => EnvFilter::new("warn"),
        LogLevel::Error => EnvFilter::new("error"),
    };

    Registry::default()
        .with(filter)
        .with(FauxDBTracingLayer)
        .init();

    Ok(())
}

// Process information utilities
pub fn get_process_info() -> ProcessInfo {
    ProcessInfo {
        pid: std::process::id(),
        parent_pid: std::process::id(), // Would need system call for real parent PID
        start_time: SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs(),
        memory_usage: get_memory_usage(),
    }
}

#[derive(Debug, Clone)]
pub struct ProcessInfo {
    pub pid: u32,
    pub parent_pid: u32,
    pub start_time: u64,
    pub memory_usage: u64,
}

fn get_memory_usage() -> u64 {
    // Simplified memory usage - in production would use system APIs
    0
}

// Connection tracking
pub struct ConnectionTracker {
    connections: Mutex<std::collections::HashMap<u32, ConnectionInfo>>,
    max_connections: u32,
}

#[derive(Debug, Clone)]
pub struct ConnectionInfo {
    pub connection_id: u32,
    pub client_addr: String,
    pub connected_at: DateTime<Utc>,
    pub last_activity: DateTime<Utc>,
    pub bytes_received: u64,
    pub bytes_sent: u64,
    pub commands_executed: u64,
}

impl ConnectionTracker {
    pub fn new(max_connections: u32) -> Self {
        Self {
            connections: Mutex::new(std::collections::HashMap::new()),
            max_connections,
        }
    }

    pub fn add_connection(&self, connection_id: u32, client_addr: String) -> Result<(), String> {
        let mut connections = self.connections.lock().unwrap();
        
        if connections.len() >= self.max_connections as usize {
            return Err(format!("Maximum connections ({}) reached", self.max_connections));
        }

        let info = ConnectionInfo {
            connection_id,
            client_addr,
            connected_at: Utc::now(),
            last_activity: Utc::now(),
            bytes_received: 0,
            bytes_sent: 0,
            commands_executed: 0,
        };

        connections.insert(connection_id, info);
        Ok(())
    }

    pub fn remove_connection(&self, connection_id: u32) {
        let mut connections = self.connections.lock().unwrap();
        connections.remove(&connection_id);
    }

    pub fn update_activity(&self, connection_id: u32, bytes_received: u64, bytes_sent: u64) {
        let mut connections = self.connections.lock().unwrap();
        if let Some(info) = connections.get_mut(&connection_id) {
            info.last_activity = Utc::now();
            info.bytes_received += bytes_received;
            info.bytes_sent += bytes_sent;
        }
    }

    pub fn increment_commands(&self, connection_id: u32) {
        let mut connections = self.connections.lock().unwrap();
        if let Some(info) = connections.get_mut(&connection_id) {
            info.commands_executed += 1;
        }
    }

    pub fn get_connection_count(&self) -> usize {
        self.connections.lock().unwrap().len()
    }

    pub fn get_connection_info(&self, connection_id: u32) -> Option<ConnectionInfo> {
        self.connections.lock().unwrap().get(&connection_id).cloned()
    }

    pub fn list_connections(&self) -> Vec<ConnectionInfo> {
        self.connections.lock().unwrap().values().cloned().collect()
    }
}
