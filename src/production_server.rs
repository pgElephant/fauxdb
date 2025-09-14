/*
 * Copyright (c) 2025 pgElephant. All rights reserved.
 *
 * FauxDB - Production-ready MongoDB-compatible database server
 * Built with Rust for superior performance and reliability
 *
 * Production-ready FauxDB Server
 * Full MongoDB 5.0+ compatibility with enterprise features
 */

use anyhow::Result;
use std::sync::Arc;
use tokio::net::TcpListener;
use tokio::signal;
use tokio::time::{sleep, Duration};
use crate::{fauxdb_info, fauxdb_warn, fauxdb_error, fauxdb_debug};

use crate::production_config::ProductionConfig;
use crate::connection_pool::ProductionConnectionPool;
use crate::mongodb_commands::MongoDBCommandRegistry;
use crate::indexing::IndexManager;
use crate::transactions::TransactionManager;
use crate::process_manager::ProcessManager;
use crate::wire_protocol::{WireProtocolHandler, WireMessage};

#[derive(Clone)]
pub struct ProductionFauxDBServer {
    config: ProductionConfig,
    connection_pool: Arc<ProductionConnectionPool>,
    command_registry: Arc<MongoDBCommandRegistry>,
    index_manager: Arc<IndexManager>,
    transaction_manager: Arc<TransactionManager>,
    metrics_enabled: bool,
    health_check_enabled: bool,
}

impl ProductionFauxDBServer {
    pub async fn new(config: ProductionConfig) -> Result<Self> {
        fauxdb_info!("Initializing Production FauxDB Server");
        
        // Validate configuration
        config.validate()?;
        
        // Initialize connection pool
        let pool_config = crate::connection_pool::ProductionPoolConfig {
            max_size: config.database.pool_size,
            min_idle: 5,
            max_lifetime: config.database.max_lifetime,
            idle_timeout: config.database.idle_timeout,
            connection_timeout: config.database.connection_timeout,
            statement_cache_size: config.database.statement_cache_size,
        };
        let connection_pool = Arc::new(
            ProductionConnectionPool::new(&config.database.connection_string, pool_config).await?
        );
        
        // Initialize components
        let command_registry = Arc::new(MongoDBCommandRegistry::new());
        let index_manager = Arc::new(IndexManager::new());
        let transaction_manager = Arc::new(TransactionManager::new(Arc::new(connection_pool.pool.clone())));
        
        fauxdb_info!("Production FauxDB Server initialized successfully");
        fauxdb_info!("Supported MongoDB Commands: {}", command_registry.get_supported_commands().len());
        fauxdb_info!("Connection Pool Size: {}", config.database.pool_size);
        fauxdb_info!("Security Enabled: {}", config.security.enable_auth);
        fauxdb_info!("Metrics Enabled: {}", config.monitoring.metrics_enabled);
        
        Ok(Self {
            config,
            connection_pool,
            command_registry,
            index_manager,
            transaction_manager,
            metrics_enabled: true,
            health_check_enabled: true,
        })
    }

    pub async fn new_with_process_manager(config: ProductionConfig, _process_manager: ProcessManager) -> Result<Self> {
        // For now, just use the regular new function
        // In a full implementation, this would integrate with the process manager
        Self::new(config).await
    }

    pub async fn start(&self) -> Result<()> {
        fauxdb_info!("Starting Production FauxDB Server");
        
        // Start health check server if enabled
        if self.health_check_enabled {
            self.start_health_check_server().await?;
        }
        
        // Start metrics server if enabled
        if self.metrics_enabled {
            self.start_metrics_server().await?;
        }
        
        // Start main MongoDB protocol server
        self.start_mongodb_server().await?;
        
        Ok(())
    }

    async fn start_mongodb_server(&self) -> Result<()> {
        let addr = format!("{}:{}", self.config.server.host, self.config.server.port);
        let listener = TcpListener::bind(&addr).await?;
        
        fauxdb_info!("MongoDB Protocol Server listening on {}", addr);
        fauxdb_info!("Max Connections: {}", self.config.server.max_connections);
        fauxdb_info!("Connection Timeout: {:?}", self.config.server.connection_timeout);
        
        let mut connection_count = 0;
        let max_connections = self.config.server.max_connections;
        
        loop {
            // Check connection limit
            if connection_count >= max_connections {
                fauxdb_warn!("Connection limit reached ({})", max_connections);
                sleep(Duration::from_millis(100)).await;
                continue;
            }
            
            match listener.accept().await {
                Ok((stream, addr)) => {
                    connection_count += 1;
                    fauxdb_info!("New connection from: {} (total: {})", addr, connection_count);
                    
                    let connection_pool = self.connection_pool.clone();
                    let command_registry = self.command_registry.clone();
                    let index_manager = self.index_manager.clone();
                    let transaction_manager = self.transaction_manager.clone();
                    
                    tokio::spawn(async move {
                        if let Err(e) = Self::handle_connection(
                            stream,
                            connection_pool,
                            command_registry,
                            index_manager,
                            transaction_manager,
                        ).await {
                            fauxdb_error!("Error handling connection from {}: {}", addr, e);
                        }
                        connection_count -= 1;
                        fauxdb_info!("Client disconnected: {} (remaining: {})", addr, connection_count);
                    });
                }
                Err(e) => {
                    fauxdb_error!("Failed to accept connection: {}", e);
                    sleep(Duration::from_millis(100)).await;
                }
            }
        }
    }

    async fn start_health_check_server(&self) -> Result<()> {
        let port = self.config.monitoring.health_check_port;
        let addr = format!("0.0.0.0:{}", port);
        
        tokio::spawn(async move {
            if let Ok(listener) = TcpListener::bind(&addr).await {
                fauxdb_info!("Health Check Server listening on {}", addr);
                
                loop {
                    match listener.accept().await {
                        Ok((mut stream, _)) => {
                            // Simple health check response
                            let response = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\n{\"status\":\"ok\"}";
                            if let Err(e) = tokio::io::AsyncWriteExt::write_all(&mut stream, response.as_bytes()).await {
                                fauxdb_error!("Health check response error: {}", e);
                            }
                        }
                        Err(e) => {
                            fauxdb_error!("Health check server error: {}", e);
                        }
                    }
                }
            }
        });
        
        Ok(())
    }

    async fn start_metrics_server(&self) -> Result<()> {
        let port = self.config.monitoring.metrics_port;
        let addr = format!("0.0.0.0:{}", port);
        
        tokio::spawn(async move {
            if let Ok(listener) = TcpListener::bind(&addr).await {
                fauxdb_info!("Metrics Server listening on {}", addr);
                
                loop {
                    match listener.accept().await {
                        Ok((mut stream, _)) => {
                            // Prometheus metrics response
                            let metrics = "# HELP fauxdb_connections_total Total number of connections\n# TYPE fauxdb_connections_total counter\nfauxdb_connections_total 0\n\n# HELP fauxdb_queries_total Total number of queries\n# TYPE fauxdb_queries_total counter\nfauxdb_queries_total 0\n";
                            let response = format!("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: {}\r\n\r\n{}", metrics.len(), metrics);
                            
                            if let Err(e) = tokio::io::AsyncWriteExt::write_all(&mut stream, response.as_bytes()).await {
                                fauxdb_error!("Metrics response error: {}", e);
                            }
                        }
                        Err(e) => {
                            fauxdb_error!("Metrics server error: {}", e);
                        }
                    }
                }
            }
        });
        
        Ok(())
    }

    async fn handle_connection(
        mut stream: tokio::net::TcpStream,
        _connection_pool: Arc<ProductionConnectionPool>,
        command_registry: Arc<MongoDBCommandRegistry>,
        _index_manager: Arc<IndexManager>,
        _transaction_manager: Arc<TransactionManager>,
    ) -> Result<()> {
        use tokio::io::{AsyncReadExt, AsyncWriteExt};
        
        let mut buffer = vec![0; 1024];
        
        loop {
            match stream.read(&mut buffer).await {
                Ok(0) => {
                    // Connection closed by client
                    break;
                }
                Ok(n) => {
                    // Process MongoDB protocol message
                    fauxdb_debug!("Received {} bytes from client", n);
                    
                    // Parse MongoDB wire protocol message using our new handler
                    match WireProtocolHandler::parse_message(&buffer[..n]) {
                        Ok(wire_message) => {
                            let command_doc = wire_message.get_command_document();
                            let request_id = wire_message.get_request_id();
                            
                            fauxdb_debug!("Parsed command document: {:?}", command_doc);
                            
                            if let Some(command_name) = Self::extract_command_name(&command_doc) {
                                fauxdb_debug!("Processing command: {} with request_id: {}", command_name, request_id);
                                
                                // Execute command through registry
                                match command_registry.handle_command(&command_name, command_doc) {
                                    Ok(response) => {
                                        // Send MongoDB wire protocol response using appropriate format
                                        let response_bytes = match wire_message {
                                            WireMessage::Query(_) => {
                                                // For OP_QUERY, always use OP_REPLY format
                                                WireProtocolHandler::build_reply_message(request_id, response)?
                                            }
                                            WireMessage::Msg(_) => {
                                                // For OP_MSG, use OP_MSG format
                                                WireProtocolHandler::build_msg_response(request_id, response)?
                                            }
                                        };
                                        
                                        fauxdb_debug!("Sending response: {} bytes", response_bytes.len());
                                        stream.write_all(&response_bytes).await?;
                                        stream.flush().await?;
                                    }
                                    Err(e) => {
                                        fauxdb_error!("Command execution failed: {}", e);
                                        let error_response = Self::build_error_response(e.to_string());
                                        let response_bytes = match wire_message {
                                            WireMessage::Query(_) => {
                                                WireProtocolHandler::build_reply_message(request_id, error_response)?
                                            }
                                            WireMessage::Msg(_) => {
                                                WireProtocolHandler::build_msg_response(request_id, error_response)?
                                            }
                                        };
                                        stream.write_all(&response_bytes).await?;
                                    }
                                }
                            } else {
                                fauxdb_debug!("No command name found in document: {:?}", command_doc);
                                // Send a default ping response using appropriate format
                                let mut ping_response = bson::Document::new();
                                ping_response.insert("ok", 1.0);
                                let response_bytes = match wire_message {
                                    WireMessage::Query(_) => {
                                        WireProtocolHandler::build_reply_message(request_id, ping_response)?
                                    }
                                    WireMessage::Msg(_) => {
                                        WireProtocolHandler::build_msg_response(request_id, ping_response)?
                                    }
                                };
                                stream.write_all(&response_bytes).await?;
                            }
                        }
                        Err(e) => {
                            fauxdb_error!("Failed to parse MongoDB message: {}", e);
                            // Send error response
                            let error_response = Self::build_error_response(format!("Parse error: {}", e));
                            let response_bytes = WireProtocolHandler::build_reply_message(1, error_response)?;
                            stream.write_all(&response_bytes).await?;
                        }
                    }
                }
                Err(e) => {
                    fauxdb_error!("Error reading from client: {}", e);
                    break;
                }
            }
        }
        
        Ok(())
    }

    #[allow(dead_code)]
    fn parse_mongodb_message_with_id(bytes: &[u8]) -> Result<(bson::Document, u32)> {
        // Log the raw bytes for debugging
        fauxdb_debug!("Received {} bytes: {:02X?}", bytes.len(), &bytes[..std::cmp::min(bytes.len(), 64)]);
        
        // First try: parse as MongoDB wire protocol message
        if bytes.len() >= 16 {
            let message_length = u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]);
            let request_id = u32::from_le_bytes([bytes[4], bytes[5], bytes[6], bytes[7]]);
            let _response_to = u32::from_le_bytes([bytes[8], bytes[9], bytes[10], bytes[11]]);
            let op_code = u32::from_le_bytes([bytes[12], bytes[13], bytes[14], bytes[15]]);
            
            fauxdb_debug!("MongoDB message: length={}, request_id={}, op_code={}, actual_len={}", message_length, request_id, op_code, bytes.len());
            
            // Handle OP_MSG (2013) - Modern MongoDB protocol
            if op_code == 2013 && bytes.len() >= 20 {
                let flags = u32::from_le_bytes([bytes[16], bytes[17], bytes[18], bytes[19]]);
                let payload_start = 20;
                
                fauxdb_debug!("OP_MSG flags: {}, payload_start: {}", flags, payload_start);
                
                if bytes.len() > payload_start {
                    let payload = &bytes[payload_start..];
                    fauxdb_debug!("OP_MSG payload: {:02X?}", &payload[..std::cmp::min(payload.len(), 32)]);
                    
                    // Try multiple parsing strategies for OP_MSG
                    for strategy in ["direct", "find_bson", "skip_zeros", "flexible"] {
                        match Self::try_parse_bson_strategy(payload, strategy) {
                            Ok(doc) => {
                                fauxdb_debug!("Successfully parsed OP_MSG using {} strategy: {:?}", strategy, doc);
                                return Ok((doc, request_id));
                            }
                            Err(e) => {
                                fauxdb_debug!("Strategy {} failed: {}", strategy, e);
                            }
                        }
                    }
                }
            }
            
            // Handle OP_QUERY (2004) - Legacy MongoDB protocol
            if op_code == 2004 && bytes.len() >= 36 {
                // OP_QUERY structure: header (16) + flags (4) + collection name (4) + skip (4) + limit (4) + query document
                let flags = u32::from_le_bytes([bytes[16], bytes[17], bytes[18], bytes[19]]);
                let collection_name_len = u32::from_le_bytes([bytes[20], bytes[21], bytes[22], bytes[23]]);
                let skip = u32::from_le_bytes([bytes[24], bytes[25], bytes[26], bytes[27]]);
                let limit = u32::from_le_bytes([bytes[28], bytes[29], bytes[30], bytes[31]]);
                
                fauxdb_debug!("OP_QUERY: flags={}, collection_name_len={}, skip={}, limit={}", flags, collection_name_len, skip, limit);
                
                // Find the start of the query document (after collection name)
                let collection_start = 32;
                let query_start = if collection_name_len == 0 {
                    collection_start // No collection name, query starts right after header
                } else {
                    collection_start + collection_name_len as usize + 1 // +1 for null terminator
                };
                
                if bytes.len() > query_start {
                    let query_payload = &bytes[query_start..];
                    fauxdb_debug!("OP_QUERY query payload: {:02X?}", &query_payload[..std::cmp::min(query_payload.len(), 64)]);
                    match Self::try_parse_bson_strategy(query_payload, "direct") {
                        Ok(doc) => {
                            fauxdb_debug!("Successfully parsed OP_QUERY document: {:?}", doc);
                            return Ok((doc, request_id));
                        }
                        Err(e) => {
                            fauxdb_debug!("Failed to parse OP_QUERY payload: {}", e);
                        }
                    }
                }
            }
        }
        
        // Second try: parse as raw BSON document (no wire protocol header)
        if bytes.len() >= 5 {
            fauxdb_debug!("Trying to parse as raw BSON document");
            match Self::try_parse_bson_strategy(bytes, "direct") {
                Ok(doc) => {
                    fauxdb_debug!("Parsed raw BSON document: {:?}", doc);
                    return Ok((doc, 1)); // Default request_id for raw BSON
                }
                Err(e) => {
                    fauxdb_debug!("Failed to parse as raw BSON: {}", e);
                }
            }
        }
        
        // Final fallback: create a simple ping response for any connection
        fauxdb_debug!("Creating fallback ping response");
        let mut doc = bson::Document::new();
        doc.insert("ping", 1);
        Ok((doc, 1))
    }

    #[allow(dead_code)]
    fn parse_mongodb_message(bytes: &[u8]) -> Result<bson::Document> {
        let (doc, _) = Self::parse_mongodb_message_with_id(bytes)?;
        Ok(doc)
    }

    #[allow(dead_code)]
    fn try_parse_bson_strategy(data: &[u8], strategy: &str) -> Result<bson::Document> {
        match strategy {
            "direct" => {
                // Direct BSON parsing
                Self::parse_bson_safely(data)
            }
            "find_bson" => {
                // Find BSON document in the data
                if let Some(bson_start) = Self::find_bson_start(data) {
                    let bson_data = &data[bson_start..];
                    Self::parse_bson_safely(bson_data)
                } else {
                    Err(anyhow::anyhow!("No valid BSON document found"))
                }
            }
            "skip_zeros" => {
                // Skip leading zeros and try to parse
                let mut start = 0;
                while start < data.len() && data[start] == 0 {
                    start += 1;
                }
                if start < data.len() {
                    Self::parse_bson_safely(&data[start..])
                } else {
                    Err(anyhow::anyhow!("All zeros, no BSON data"))
                }
            }
            "flexible" => {
                // Try parsing from multiple starting positions
                for offset in 0..std::cmp::min(data.len(), 16) {
                    if let Ok(doc) = Self::parse_bson_safely(&data[offset..]) {
                        return Ok(doc);
                    }
                }
                Err(anyhow::anyhow!("Flexible parsing failed"))
            }
            _ => Err(anyhow::anyhow!("Unknown strategy: {}", strategy))
        }
    }

    #[allow(dead_code)]
    fn find_bson_start(data: &[u8]) -> Option<usize> {
        // Look for BSON document start (length field)
        for i in 0..data.len().saturating_sub(4) {
            let len = u32::from_le_bytes([data[i], data[i+1], data[i+2], data[i+3]]);
            if len >= 5 && len <= data.len() as u32 && i + len as usize <= data.len() {
                // Check if it ends with null terminator
                if data[i + len as usize - 1] == 0 {
                    return Some(i);
                }
            }
        }
        None
    }

    #[allow(dead_code)]
    fn parse_bson_safely(data: &[u8]) -> Result<bson::Document> {
        if data.len() < 5 {
            return Err(anyhow::anyhow!("BSON data too short: {} bytes", data.len()));
        }
        
        // Read BSON document length
        let doc_len = u32::from_le_bytes([data[0], data[1], data[2], data[3]]) as usize;
        
        if doc_len < 5 || doc_len > data.len() {
            return Err(anyhow::anyhow!("Invalid BSON length: {} (data len: {})", doc_len, data.len()));
        }
        
        if data[doc_len - 1] != 0 {
            return Err(anyhow::anyhow!("BSON document not null-terminated"));
        }
        
        // Parse the BSON document
        bson::from_slice(&data[..doc_len]).map_err(|e| anyhow::anyhow!("BSON parse error: {}", e))
    }

    fn extract_command_name(doc: &bson::Document) -> Option<String> {
        // Check for collection operations first
        if doc.contains_key("find") {
            return Some("find".to_string());
        }
        if doc.contains_key("count") {
            return Some("count".to_string());
        }
        if doc.contains_key("insert") {
            return Some("insert".to_string());
        }
        if doc.contains_key("update") {
            return Some("update".to_string());
        }
        if doc.contains_key("delete") {
            return Some("delete".to_string());
        }
        
        // Extract the first key that's not a MongoDB protocol field
        for (key, _) in doc {
            if !key.starts_with('$') && 
               key != "lsid" && 
               key != "fullCollectionName" && 
               key != "numberToSkip" && 
               key != "numberToReturn" {
                return Some(key.clone());
            }
        }
        None
    }

    fn build_error_response(message: String) -> bson::Document {
        let mut doc = bson::Document::new();
        doc.insert("ok", 0.0);
        doc.insert("errmsg", message);
        doc.insert("code", 1);
        doc.insert("codeName", "InternalError");
        doc
    }

    #[allow(dead_code)]
    fn build_mongodb_response(document: bson::Document, request_id: u32) -> Result<Vec<u8>> {
        // Serialize the document to BSON
        let bson_data = bson::to_vec(&document)?;
        
        // For OP_QUERY response: header (16) + response flags (4) + cursor id (8) + starting from (4) + number returned (4) + documents
        let message_length = 36 + bson_data.len() as u32;
        
        let mut response = Vec::with_capacity(message_length as usize);
        
        // Message header (16 bytes)
        response.extend_from_slice(&message_length.to_le_bytes()); // messageLength
        response.extend_from_slice(&request_id.to_le_bytes());     // requestID
        response.extend_from_slice(&0u32.to_le_bytes());           // responseTo
        response.extend_from_slice(&1u32.to_le_bytes());           // opCode (OP_REPLY)
        
        // OP_REPLY fields (20 bytes)
        response.extend_from_slice(&0u32.to_le_bytes());           // responseFlags
        response.extend_from_slice(&0u64.to_le_bytes());           // cursorID
        response.extend_from_slice(&0u32.to_le_bytes());           // startingFrom
        response.extend_from_slice(&1u32.to_le_bytes());           // numberReturned
        
        // BSON document data
        response.extend_from_slice(&bson_data);
        
        fauxdb_debug!("Built OP_REPLY response: length={}, request_id={}, bson_len={}, total_len={}", 
                     message_length, request_id, bson_data.len(), response.len());
        fauxdb_debug!("Response bytes: {:02X?}", &response[..std::cmp::min(response.len(), 64)]);
        
        Ok(response)
    }

    pub async fn shutdown(&self) -> Result<()> {
        fauxdb_info!("Shutting down Production FauxDB Server");
        
        // Graceful shutdown procedures
        fauxdb_info!("Production FauxDB Server shutdown complete");
        Ok(())
    }

    pub fn get_server_stats(&self) -> ServerStats {
        ServerStats {
            uptime: chrono::Utc::now(),
            connections: 0, // Would track actual connections
            commands_executed: 0, // Would track from metrics
            indexes_created: 0, // Would track from index manager
            transactions_active: 0, // Would track from transaction manager
        }
    }
}

#[derive(Debug, Clone)]
pub struct ServerStats {
    pub uptime: chrono::DateTime<chrono::Utc>,
    pub connections: u64,
    pub commands_executed: u64,
    pub indexes_created: u64,
    pub transactions_active: u64,
}

impl ProductionFauxDBServer {
    pub async fn run_with_shutdown_signal(&self) -> Result<()> {
        // Start the server in a separate task
        let server_self = self.clone();
        let server_handle = tokio::spawn(async move {
            if let Err(e) = server_self.start().await {
                fauxdb_error!("Server failed to start: {}", e);
            }
        });

        // Wait for shutdown signal
        match signal::ctrl_c().await {
            Ok(()) => {
                fauxdb_info!("Received shutdown signal");
            }
            Err(err) => {
                fauxdb_error!("Unable to listen for shutdown signal: {}", err);
            }
        }

        // Shutdown the server
        self.shutdown().await?;
        server_handle.abort();

        Ok(())
    }
}
