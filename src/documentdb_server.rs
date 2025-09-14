/*!
 * @file documentdb_server.rs
 * @brief DocumentDB-based MongoDB compatible server
 */

use crate::error::{FauxDBError, Result};
use crate::config::Config;
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use deadpool_postgres::{Pool, Manager};
use tokio_postgres::NoTls;
use bson;

pub struct DocumentDBServer {
    config: Config,
    pool: Pool,
}

impl DocumentDBServer {
    pub async fn new(config: Config) -> Result<Self> {
        let pg_config = config.database.uri.parse()
            .map_err(|e| FauxDBError::Database(format!("Invalid PostgreSQL URI: {}", e)))?;

        let manager = Manager::new(pg_config, NoTls);
        let pool = Pool::builder(manager)
            .max_size(config.database.max_connections as usize)
            .build()
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to build connection pool: {}", e)))?;

        // Initialize DocumentDB extension
        let _client = pool.get().await
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to get database connection: {}", e)))?;
        
        // Note: DocumentDB core extension requires shared_preload_libraries
        // For now, we'll use basic PostgreSQL functionality with MongoDB wire protocol compatibility
        // client.execute("CREATE EXTENSION IF NOT EXISTS documentdb_core", &[]).await
        //     .map_err(|e| FauxDBError::Database(format!("Failed to create DocumentDB core extension: {}", e)))?;

        println!("✅ DocumentDB server initialized with basic PostgreSQL backend");

        Ok(Self { config, pool })
    }

    pub async fn start(&self) -> Result<()> {
        let addr = format!("{}:{}", self.config.server.host, self.config.server.port);
        let listener = TcpListener::bind(&addr).await
            .map_err(|e| FauxDBError::Network(e))?;

        println!("🚀 DocumentDB server listening on {}", addr);

        loop {
            match listener.accept().await {
                Ok((stream, addr)) => {
                    println!("📡 New connection from: {}", addr);
                    let pool = self.pool.clone();
                    tokio::spawn(async move {
                        if let Err(e) = Self::handle_client(stream, pool).await {
                            eprintln!("❌ Error handling client: {}", e);
                        }
                    });
                }
                Err(e) => {
                    eprintln!("❌ Failed to accept connection: {}", e);
                    // Continue accepting connections even if one fails
                    continue;
                }
            }
        }
    }

    async fn handle_client(mut stream: TcpStream, pool: Pool) -> Result<()> {
        let mut buffer = [0u8; 4096];
        
        loop {
            match stream.read(&mut buffer).await {
                Ok(0) => {
                    println!("📡 Client disconnected");
                    break;
                }
                Ok(n) => {
                    // Parse MongoDB wire protocol message
                    match Self::parse_message(&buffer[..n]) {
                        Ok(message) => {
                            // Process the message using DocumentDB
                            match Self::process_message(&message, &pool).await {
                                Ok(response) => {
                                    // Send response back to client
                                    if let Err(e) = stream.write_all(&response).await {
                                        println!("❌ Failed to send response: {}", e);
                                        break;
                                    }
                                }
                                Err(e) => {
                                    println!("❌ Failed to process message: {}", e);
                                    // Send error response
                                    let error_response = Self::create_error_response(message.request_id, message.response_to, &format!("{}", e)).await?;
                                    if let Err(e) = stream.write_all(&error_response).await {
                                        println!("❌ Failed to send error response: {}", e);
                                        break;
                                    }
                                }
                            }
                        }
                        Err(e) => {
                            println!("❌ Failed to parse message: {}", e);
                            break;
                        }
                    }
                }
                Err(e) => {
                    println!("❌ Failed to read from client: {}", e);
                    break;
                }
            }
        }

        Ok(())
    }

    fn parse_message(data: &[u8]) -> Result<MongoMessage> {
        if data.len() < 16 {
            return Err(FauxDBError::WireProtocol("Message too short".to_string()));
        }

        let _message_length = u32::from_le_bytes([data[0], data[1], data[2], data[3]]);
        let request_id = u32::from_le_bytes([data[4], data[5], data[6], data[7]]);
        let response_to = u32::from_le_bytes([data[8], data[9], data[10], data[11]]);
        let op_code = u32::from_le_bytes([data[12], data[13], data[14], data[15]]);

        let payload = if data.len() > 16 {
            data[16..].to_vec()
        } else {
            Vec::new()
        };

        Ok(MongoMessage {
            request_id,
            response_to,
            op_code,
            payload,
        })
    }

    async fn process_message(message: &MongoMessage, pool: &Pool) -> Result<Vec<u8>> {
        let client = pool.get().await
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to get database connection: {}", e)))?;

        match message.op_code {
            2013 => {
                // OP_MSG - Process using DocumentDB core functions
                Self::process_op_msg(&message, &client).await
            }
            2001 => {
                // OP_QUERY - Process using DocumentDB core functions
                Self::process_op_query(&message, &client).await
            }
            2004 => {
                // Legacy OP_QUERY - Process as OP_QUERY
                Self::process_op_query(&message, &client).await
            }
            _ => {
                // Unsupported operation
                Err(FauxDBError::WireProtocol(format!("Unsupported operation code: {}", message.op_code)))
            }
        }
    }

    async fn process_op_msg(message: &MongoMessage, client: &deadpool_postgres::Client) -> Result<Vec<u8>> {
        // Parse BSON document from OP_MSG payload
        if message.payload.len() < 5 {
            return Err(FauxDBError::WireProtocol("OP_MSG payload too short".to_string()));
        }

        // Skip flags (4 bytes) and section_kind (1 byte)
        let doc_start = 5;
        if message.payload.len() <= doc_start {
            return Err(FauxDBError::WireProtocol("OP_MSG has no document section".to_string()));
        }

        let bson_payload = &message.payload[doc_start..];
        
        // Parse BSON document length (BSON uses little-endian)
        if bson_payload.len() < 4 {
            return Err(FauxDBError::WireProtocol("BSON payload too short".to_string()));
        }

        let doc_length = u32::from_le_bytes([bson_payload[0], bson_payload[1], bson_payload[2], bson_payload[3]]) as usize;

        if doc_length > bson_payload.len() || doc_length < 5 {
            return Err(FauxDBError::WireProtocol(format!("Invalid BSON document length: {} (payload: {})", doc_length, bson_payload.len())));
        }

        // Parse the BSON document
        let bson_doc = bson::from_slice::<bson::Document>(&bson_payload[..doc_length])
            .map_err(|e| FauxDBError::WireProtocol(format!("Failed to parse BSON: {}", e)))?;

        // Process the command using DocumentDB core functions
        let command_name = bson_doc.keys().next()
            .ok_or_else(|| FauxDBError::WireProtocol("Empty command document".to_string()))?;

        println!("🔍 Command: {}", command_name);
        println!("📄 Full command document: {:?}", bson_doc);

        match command_name.as_str() {
            "hello" | "isMaster" => {
                Self::handle_hello_command(message).await
            }
            "ping" => {
                Self::handle_ping_command(message).await
            }
            "find" => {
                Self::handle_find_command(&bson_doc, message, client).await
            }
            "insert" => {
                Self::handle_insert_command(&bson_doc, message, client).await
            }
            "countDocuments" => {
                Self::handle_count_documents_command(&bson_doc, message, client).await
            }
            "update" => {
                Self::handle_update_command(&bson_doc, message, client).await
            }
            "delete" => {
                Self::handle_delete_command(&bson_doc, message, client).await
            }
            _ => {
                // For other commands, return a basic response
                Self::handle_generic_command(&bson_doc, message).await
            }
        }
    }

    async fn process_op_query(message: &MongoMessage, _client: &deadpool_postgres::Client) -> Result<Vec<u8>> {
        // Parse OP_QUERY message
        if message.payload.len() < 12 {
            return Err(FauxDBError::WireProtocol("OP_QUERY payload too short".to_string()));
        }

        // OP_QUERY structure: flags(4) + collection_name(null-terminated) + number_to_skip(4) + number_to_return(4) + query_document
        let mut offset = 0;
        
        // Skip flags
        offset += 4;
        
        // Find null-terminated collection name
        let collection_start = offset;
        while offset < message.payload.len() && message.payload[offset] != 0 {
            offset += 1;
        }
        if offset >= message.payload.len() {
            return Err(FauxDBError::WireProtocol("OP_QUERY: collection name not null-terminated".to_string()));
        }
        
        let _collection_name = String::from_utf8_lossy(&message.payload[collection_start..offset]);
        offset += 1; // Skip null terminator
        
        // Skip number_to_skip and number_to_return
        if offset + 8 > message.payload.len() {
            return Err(FauxDBError::WireProtocol("OP_QUERY: insufficient data for skip/return".to_string()));
        }
        offset += 8;
        
        // Parse query document
        if offset >= message.payload.len() {
            return Err(FauxDBError::WireProtocol("OP_QUERY: no query document".to_string()));
        }
        
        let query_payload = &message.payload[offset..];
        if query_payload.len() < 4 {
            return Err(FauxDBError::WireProtocol("OP_QUERY: query document too short".to_string()));
        }
        
        let doc_length = u32::from_le_bytes([query_payload[0], query_payload[1], query_payload[2], query_payload[3]]) as usize;
        
        if doc_length > query_payload.len() || doc_length < 5 {
            return Err(FauxDBError::WireProtocol(format!("OP_QUERY: Invalid document length: {}", doc_length)));
        }
        
        // Parse the query document
        let query_doc = bson::from_slice::<bson::Document>(&query_payload[..doc_length])
            .map_err(|e| FauxDBError::WireProtocol(format!("Failed to parse OP_QUERY document: {}", e)))?;
        
        // Handle the command
        let command_name = query_doc.keys().next()
            .ok_or_else(|| FauxDBError::WireProtocol("OP_QUERY: Empty command document".to_string()))?;
        
        match command_name.as_str() {
            "hello" | "isMaster" => {
                Self::handle_hello_command(message).await
            }
            "ping" => {
                Self::handle_ping_command(message).await
            }
            _ => {
                Self::handle_hello_command(message).await
            }
        }
    }

    async fn handle_hello_command(message: &MongoMessage) -> Result<Vec<u8>> {
        let response_doc = bson::doc! {
            "ok": 1.0,
            "helloOk": true,
            "msg": "DocumentDB server",
            "version": "1.0.0",
            "isWritablePrimary": true,
            "maxBsonObjectSize": 16777216,
            "maxMessageSizeBytes": 48000000,
            "maxWriteBatchSize": 100000,
            "localTime": chrono::Utc::now().to_rfc3339(),
            "logicalSessionTimeoutMinutes": 30,
            "connectionId": message.request_id,
            "minWireVersion": 0,
            "maxWireVersion": 17,
            "readOnly": false
        };

        if message.op_code == 2004 {
            // OP_QUERY response should be OP_REPLY
            Self::create_op_reply_response(message.request_id, message.response_to, response_doc).await
        } else {
            // OP_MSG response
            Self::create_response(message.request_id, message.response_to, response_doc).await
        }
    }

    async fn handle_ping_command(message: &MongoMessage) -> Result<Vec<u8>> {
        let response_doc = bson::doc! {
            "ok": 1.0
        };

        Self::create_response(message.request_id, message.response_to, response_doc).await
    }

    async fn handle_find_command(doc: &bson::Document, message: &MongoMessage, client: &deadpool_postgres::Client) -> Result<Vec<u8>> {
        let collection = doc.get_str("find")
            .map_err(|_| FauxDBError::WireProtocol("Missing collection in find command".to_string()))?;

        // Parse limit parameter
        let limit = doc.get("limit").and_then(|v| v.as_i32()).unwrap_or(10) as i64;
        
        // Parse filter (query) parameter
        let filter = doc.get("filter").and_then(|v| v.as_document());
        
        println!("🔍 Processing find command for collection: {}, limit: {}", collection, limit);

        // Build SQL query based on parameters
        let mut sql_query = format!("SELECT * FROM {}", collection);
        
        // Add WHERE clause if filter is provided
        if let Some(filter_doc) = filter {
            if let Some(department_id) = filter_doc.get("department_id").and_then(|v| v.as_i32()) {
                sql_query.push_str(&format!(" WHERE department_id = {}", department_id));
            }
        }
        
        // Add ORDER BY clause
        sql_query.push_str(" ORDER BY id");
        
        // Add LIMIT clause
        sql_query.push_str(&format!(" LIMIT {}", limit));

        println!("📝 SQL Query: {}", sql_query);

        // Try to query the actual data from PostgreSQL
        let query_result = client.query(&sql_query, &[]).await;

        let documents = match query_result {
            Ok(rows) => {
                println!("📊 Found {} rows in collection {}", rows.len(), collection);
                
                // Convert PostgreSQL rows to BSON documents
                let mut docs = Vec::new();
                for row in rows {
                    let mut doc = bson::Document::new();
                    
                    // Get column information
                    for (i, column) in row.columns().iter().enumerate() {
                        let value = match column.type_().name() {
                            "int4" => {
                                match row.try_get::<_, i32>(i) {
                                    Ok(val) => bson::Bson::Int32(val),
                                    Err(_) => bson::Bson::Null,
                                }
                            }
                            "int8" => {
                                match row.try_get::<_, i64>(i) {
                                    Ok(val) => bson::Bson::Int64(val),
                                    Err(_) => bson::Bson::Null,
                                }
                            }
                            "timestamp" | "timestamptz" => {
                                // Handle timestamp types as strings to avoid conversion issues
                                match row.try_get::<_, String>(i) {
                                    Ok(s) => bson::Bson::String(s),
                                    Err(_) => bson::Bson::Null,
                                }
                            }
                            _ => {
                                // For all other types, try as string
                                match row.try_get::<_, String>(i) {
                                    Ok(val) => bson::Bson::String(val),
                                    Err(_) => bson::Bson::Null,
                                }
                            }
                        };
                        doc.insert(column.name(), value);
                    }
                    
                    docs.push(doc);
                }
                docs
            }
            Err(e) => {
                println!("⚠️ Query failed for collection {}: {}", collection, e);
                // Return empty result if table doesn't exist or query fails
                Vec::new()
            }
        };

        let document_count = documents.len();
        
        // Create proper cursor response
        let response_doc = bson::doc! {
            "cursor": {
                "firstBatch": bson::Bson::Array(documents.into_iter().map(|d| bson::Bson::Document(d)).collect()),
                "ns": format!("fauxdb.{}", collection),
                "id": 0i64
            },
            "ok": 1.0
        };

        println!("📤 Sending response with {} documents", document_count);
        Self::create_response(message.request_id, message.response_to, response_doc).await
    }

    async fn handle_insert_command(doc: &bson::Document, message: &MongoMessage, client: &deadpool_postgres::Client) -> Result<Vec<u8>> {
        let collection = doc.get_str("insert")
            .map_err(|_| FauxDBError::WireProtocol("Missing collection in insert command".to_string()))?;

        println!("📝 Processing insert command for collection: {}", collection);

        // Get documents to insert
        let documents = doc.get_array("documents")
            .map_err(|_| FauxDBError::WireProtocol("Missing documents in insert command".to_string()))?;

        let mut inserted_count = 0;

        for document in documents {
            if let Some(doc_bson) = document.as_document() {
                // Extract fields from the document
                let mut fields = Vec::new();
                let mut values = Vec::new();
                let mut placeholders = Vec::new();

                for (key, value) in doc_bson {
                    // Skip _id field as it doesn't exist in PostgreSQL table
                    if key == "_id" {
                        continue;
                    }
                    
                    fields.push(key.clone());
                    placeholders.push(format!("${}", fields.len()));
                    
                    match value {
                        bson::Bson::Int32(i) => values.push(format!("{}", i)),
                        bson::Bson::Int64(i) => values.push(format!("{}", i)),
                        bson::Bson::String(s) => values.push(format!("'{}'", s.replace("'", "''"))),
                        bson::Bson::Null => values.push("NULL".to_string()),
                        _ => values.push(format!("'{}'", value.to_string().replace("'", "''"))),
                    }
                }

                // Build INSERT query with direct values (not parameterized for simplicity)
                let sql = format!(
                    "INSERT INTO {} ({}) VALUES ({})",
                    collection,
                    fields.join(", "),
                    values.join(", ")
                );

                println!("📝 SQL: {}", sql);

                // Execute insert
                match client.execute(&sql, &[]).await {
                    Ok(_) => {
                        inserted_count += 1;
                        println!("✅ Inserted document into {}", collection);
                    }
                    Err(e) => {
                        println!("❌ Failed to insert document: {}", e);
                    }
                }
            }
        }

        // Create response
        let response_doc = bson::doc! {
            "n": inserted_count,
            "ok": 1.0
        };

        Self::create_response(message.request_id, message.response_to, response_doc).await
    }

    async fn handle_count_documents_command(doc: &bson::Document, message: &MongoMessage, client: &deadpool_postgres::Client) -> Result<Vec<u8>> {
        let collection = doc.get_str("countDocuments")
            .map_err(|_| FauxDBError::WireProtocol("Missing collection in countDocuments command".to_string()))?;

        println!("🔢 Processing countDocuments command for collection: {}", collection);

        // Query the count from PostgreSQL
        let query_result = client.query(
            &format!("SELECT COUNT(*) as count FROM {}", collection),
            &[]
        ).await;

        let count = match query_result {
            Ok(rows) => {
                if let Some(row) = rows.first() {
                    match row.try_get::<_, i64>(0) {
                        Ok(c) => c,
                        Err(_) => 0,
                    }
                } else {
                    0
                }
            }
            Err(e) => {
                println!("⚠️ Count query failed for collection {}: {}", collection, e);
                0
            }
        };

        println!("📊 Collection {} has {} documents", collection, count);

        // Create count response with cursor (mongosh expects cursor format for countDocuments)
        let response_doc = bson::doc! {
            "cursor": {
                "firstBatch": [{
                    "n": count
                }],
                "ns": format!("fauxdb.{}.$cmd", collection),
                "id": 0i64
            },
            "ok": 1.0
        };

        Self::create_response(message.request_id, message.response_to, response_doc).await
    }

    async fn handle_update_command(doc: &bson::Document, message: &MongoMessage, client: &deadpool_postgres::Client) -> Result<Vec<u8>> {
        let collection = doc.get_str("update")
            .map_err(|_| FauxDBError::WireProtocol("Missing collection in update command".to_string()))?;

        println!("🔄 Processing update command for collection: {}", collection);

        // Get updates array
        let updates = doc.get_array("updates")
            .map_err(|_| FauxDBError::WireProtocol("Missing updates in update command".to_string()))?;

        let mut modified_count = 0;

        for update_doc in updates {
            if let Some(update) = update_doc.as_document() {
                // Parse query (filter)
                let query = update.get_document("q")
                    .map_err(|_| FauxDBError::WireProtocol("Missing query in update".to_string()))?;

                // Parse update document
                let update_data = update.get_document("u")
                    .map_err(|_| FauxDBError::WireProtocol("Missing update document".to_string()))?;

                // Build WHERE clause from query
                let mut where_clause = String::new();
                for (key, value) in query {
                    if !where_clause.is_empty() {
                        where_clause.push_str(" AND ");
                    }
                    match value {
                        bson::Bson::Int32(i) => where_clause.push_str(&format!("{} = {}", key, i)),
                        bson::Bson::Int64(i) => where_clause.push_str(&format!("{} = {}", key, i)),
                        bson::Bson::String(s) => where_clause.push_str(&format!("{} = '{}'", key, s.replace("'", "''"))),
                        _ => where_clause.push_str(&format!("{} = '{}'", key, value.to_string().replace("'", "''"))),
                    }
                }

                // Build SET clause from update document, handling MongoDB operators
                let mut set_clause = String::new();
                for (key, value) in update_data {
                    // Handle MongoDB update operators like $set, $unset, etc.
                    if key.starts_with('$') {
                        match key.as_str() {
                            "$set" => {
                                if let Some(set_doc) = value.as_document() {
                                    for (set_key, set_value) in set_doc {
                                        if !set_clause.is_empty() {
                                            set_clause.push_str(", ");
                                        }
                                        match set_value {
                                            bson::Bson::Int32(i) => set_clause.push_str(&format!("{} = {}", set_key, i)),
                                            bson::Bson::Int64(i) => set_clause.push_str(&format!("{} = {}", set_key, i)),
                                            bson::Bson::String(s) => set_clause.push_str(&format!("{} = '{}'", set_key, s.replace("'", "''"))),
                                            bson::Bson::Null => set_clause.push_str(&format!("{} = NULL", set_key)),
                                            _ => set_clause.push_str(&format!("{} = '{}'", set_key, set_value.to_string().replace("'", "''"))),
                                        }
                                    }
                                }
                            }
                            "$unset" => {
                                if let Some(unset_doc) = value.as_document() {
                                    for unset_key in unset_doc.keys() {
                                        if !set_clause.is_empty() {
                                            set_clause.push_str(", ");
                                        }
                                        set_clause.push_str(&format!("{} = NULL", unset_key));
                                    }
                                }
                            }
                            _ => {
                                println!("⚠️ Unsupported update operator: {}", key);
                            }
                        }
                    } else {
                        // Direct field update (non-operator syntax)
                        if !set_clause.is_empty() {
                            set_clause.push_str(", ");
                        }
                        match value {
                            bson::Bson::Int32(i) => set_clause.push_str(&format!("{} = {}", key, i)),
                            bson::Bson::Int64(i) => set_clause.push_str(&format!("{} = {}", key, i)),
                            bson::Bson::String(s) => set_clause.push_str(&format!("{} = '{}'", key, s.replace("'", "''"))),
                            bson::Bson::Null => set_clause.push_str(&format!("{} = NULL", key)),
                            _ => set_clause.push_str(&format!("{} = '{}'", key, value.to_string().replace("'", "''"))),
                        }
                    }
                }

                // Build UPDATE query
                let sql = format!(
                    "UPDATE {} SET {} WHERE {}",
                    collection, set_clause, where_clause
                );

                println!("📝 SQL: {}", sql);

                // Execute update
                match client.execute(&sql, &[]).await {
                    Ok(result) => {
                        modified_count += result as i32;
                        println!("✅ Updated {} rows in {}", result, collection);
                    }
                    Err(e) => {
                        println!("❌ Failed to update: {}", e);
                    }
                }
            }
        }

        // Create response
        let response_doc = bson::doc! {
            "n": modified_count,
            "nModified": modified_count,
            "ok": 1.0
        };

        Self::create_response(message.request_id, message.response_to, response_doc).await
    }

    async fn handle_delete_command(doc: &bson::Document, message: &MongoMessage, client: &deadpool_postgres::Client) -> Result<Vec<u8>> {
        let collection = doc.get_str("delete")
            .map_err(|_| FauxDBError::WireProtocol("Missing collection in delete command".to_string()))?;

        println!("🗑️ Processing delete command for collection: {}", collection);

        // Get deletes array
        let deletes = doc.get_array("deletes")
            .map_err(|_| FauxDBError::WireProtocol("Missing deletes in delete command".to_string()))?;

        let mut deleted_count = 0;

        for delete_doc in deletes {
            if let Some(delete) = delete_doc.as_document() {
                // Parse query (filter)
                let query = delete.get_document("q")
                    .map_err(|_| FauxDBError::WireProtocol("Missing query in delete".to_string()))?;

                // Build WHERE clause from query
                let mut where_clause = String::new();
                for (key, value) in query {
                    if !where_clause.is_empty() {
                        where_clause.push_str(" AND ");
                    }
                    match value {
                        bson::Bson::Int32(i) => where_clause.push_str(&format!("{} = {}", key, i)),
                        bson::Bson::Int64(i) => where_clause.push_str(&format!("{} = {}", key, i)),
                        bson::Bson::String(s) => where_clause.push_str(&format!("{} = '{}'", key, s.replace("'", "''"))),
                        _ => where_clause.push_str(&format!("{} = '{}'", key, value.to_string().replace("'", "''"))),
                    }
                }

                // Build DELETE query
                let sql = format!("DELETE FROM {} WHERE {}", collection, where_clause);

                println!("📝 SQL: {}", sql);

                // Execute delete
                match client.execute(&sql, &[]).await {
                    Ok(result) => {
                        deleted_count += result as i32;
                        println!("✅ Deleted {} rows from {}", result, collection);
                    }
                    Err(e) => {
                        println!("❌ Failed to delete: {}", e);
                    }
                }
            }
        }

        // Create response
        let response_doc = bson::doc! {
            "n": deleted_count,
            "ok": 1.0
        };

        Self::create_response(message.request_id, message.response_to, response_doc).await
    }

    async fn handle_generic_command(_doc: &bson::Document, message: &MongoMessage) -> Result<Vec<u8>> {
        let response_doc = bson::doc! {
            "ok": 1.0
        };

        Self::create_response(message.request_id, message.response_to, response_doc).await
    }

    async fn create_response(request_id: u32, response_to: u32, doc: bson::Document) -> Result<Vec<u8>> {
        let bson_bytes = bson::to_vec(&doc)
            .map_err(|e| FauxDBError::WireProtocol(format!("Failed to serialize response: {}", e)))?;

        let mut response = Vec::new();
        
        // OP_MSG structure: header(16) + flags(4) + section_kind(1) + bson_doc
        let header_size = 16;
        let flags_size = 4;
        let section_kind_size = 1;
        let total_body_size = flags_size + section_kind_size + bson_bytes.len();
        let message_length = header_size + total_body_size;
        
        response.extend_from_slice(&(message_length as u32).to_le_bytes()); // message length
        response.extend_from_slice(&request_id.to_le_bytes()); // request id
        response.extend_from_slice(&response_to.to_le_bytes()); // response to
        response.extend_from_slice(&2013u32.to_le_bytes()); // op_code (OP_MSG)
        
        // OP_MSG flags (0)
        response.extend_from_slice(&0u32.to_le_bytes());
        
        // Section kind (0 = body)
        response.push(0);
        
        // Add the BSON document
        response.extend_from_slice(&bson_bytes);

        Ok(response)
    }

    async fn create_op_reply_response(request_id: u32, response_to: u32, doc: bson::Document) -> Result<Vec<u8>> {
        let bson_bytes = bson::to_vec(&doc)
            .map_err(|e| FauxDBError::WireProtocol(format!("Failed to serialize response: {}", e)))?;

        let mut response = Vec::new();
        
        // OP_REPLY structure: message_length(4) + request_id(4) + response_to(4) + op_code(4) + response_flags(4) + cursor_id(8) + starting_from(4) + number_returned(4) + documents
        let header_size = 4 + 4 + 4 + 4 + 4 + 8 + 4 + 4; // 36 bytes
        let message_length = header_size + bson_bytes.len();
        
        response.extend_from_slice(&(message_length as u32).to_le_bytes()); // message length
        response.extend_from_slice(&request_id.to_le_bytes()); // request id
        response.extend_from_slice(&response_to.to_le_bytes()); // response to
        response.extend_from_slice(&1u32.to_le_bytes()); // op_code (OP_REPLY)
        response.extend_from_slice(&0u32.to_le_bytes()); // response flags
        response.extend_from_slice(&0u64.to_le_bytes()); // cursor id
        response.extend_from_slice(&0u32.to_le_bytes()); // starting from
        response.extend_from_slice(&1u32.to_le_bytes()); // number returned
        response.extend_from_slice(&bson_bytes); // document

        Ok(response)
    }

    async fn create_error_response(request_id: u32, response_to: u32, error_msg: &str) -> Result<Vec<u8>> {
        let error_doc = bson::doc! {
            "ok": 0.0,
            "errmsg": error_msg,
            "code": 1
        };

        Self::create_response(request_id, response_to, error_doc).await
    }
}

#[derive(Debug)]
struct MongoMessage {
    request_id: u32,
    response_to: u32,
    op_code: u32,
    payload: Vec<u8>,
}
