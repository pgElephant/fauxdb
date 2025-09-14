/*!
 * @file command_processor.rs
 * @brief MongoDB command processing for FauxDB
 */

use crate::postgresql_manager::PostgreSQLManager;
use crate::error::{FauxDBError, Result};
use crate::wire_protocol::{WireProtocolHandler, MongoMessage};
use bson::{Document, Bson, doc};

pub struct CommandProcessor {
    db_manager: PostgreSQLManager,
    wire_handler: WireProtocolHandler,
    default_database: String,
}

impl CommandProcessor {
    pub fn new(db_manager: PostgreSQLManager) -> Self {
        Self {
            db_manager,
            wire_handler: WireProtocolHandler::new(),
            default_database: "fauxdb".to_string(),
        }
    }

    pub async fn process_message(&self, message: &MongoMessage) -> Result<Vec<u8>> {
        match message.op_code {
            2004 => self.process_op_msg(&message.payload, message.request_id, message.response_to).await,
            2001 => self.process_op_query(&message.payload, message.request_id, message.response_to).await,
            _ => {
                Err(FauxDBError::Command(format!("Unsupported operation code: {}", message.op_code)))
            }
        }
    }

    async fn process_op_msg(&self, payload: &[u8], request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        let document = self.wire_handler.parse_op_msg(payload)?;
        
        if let Some((cmd_name, _)) = document.iter().next() {
            match cmd_name.as_str() {
                "ping" => {
                    println!("🏓 Processing ping command");
                    self.wire_handler.create_ping_response(request_id, response_to)
                }
                "hello" => {
                    println!("👋 Processing hello command");
                    self.wire_handler.create_hello_response(request_id, response_to)
                }
                "find" => {
                    println!("🔍 Processing find command");
                    self.process_find(&document, request_id, response_to).await
                }
                "insert" => {
                    println!("📝 Processing insert command");
                    self.process_insert(&document, request_id, response_to).await
                }
                "update" => {
                    println!("🔄 Processing update command");
                    self.process_update(&document, request_id, response_to).await
                }
                "delete" => {
                    println!("🗑️ Processing delete command");
                    self.process_delete(&document, request_id, response_to).await
                }
                "count" => {
                    println!("🔢 Processing count command");
                    self.process_count(&document, request_id, response_to).await
                }
                "aggregate" => {
                    println!("📊 Processing aggregate command");
                    self.process_aggregate(&document, request_id, response_to).await
                }
                "listCollections" => {
                    println!("📋 Processing listCollections command");
                    self.process_list_collections(&document, request_id, response_to).await
                }
                "create" => {
                    println!("🆕 Processing create command");
                    self.process_create_collection(&document, request_id, response_to).await
                }
                "drop" => {
                    println!("🗑️ Processing drop command");
                    self.process_drop_collection(&document, request_id, response_to).await
                }
                _ => {
                    println!("⚠️ Unknown command: {}", cmd_name);
                    Err(FauxDBError::Command(format!("Unknown command: {}", cmd_name)))
                }
            }
        } else {
            self.wire_handler.create_error_response(request_id, response_to, "Empty command document")
        }
    }

    async fn process_op_query(&self, _payload: &[u8], request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        // OP_QUERY is legacy, but we can handle it
        self.wire_handler.create_success_response(request_id, response_to)
    }

    async fn process_find(&self, document: &Document, request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        let collection = document.get_str("find")
            .map_err(|_| FauxDBError::Command("Missing collection name in find command".to_string()))?;

        // Ensure collection exists
        self.db_manager.create_collection(&self.default_database, collection).await?;

        // Parse filter, projection, skip, limit
        let filter = document.get_document("filter").ok();
        let projection = document.get_document("projection").ok();
        let skip = document.get_i64("skip").ok().map(|v| v as u64);
        let limit = document.get_i64("limit").ok().map(|v| v as u64);

        // Query documents
        let documents = self.db_manager.query_documents(&self.default_database, collection, filter, projection, skip, limit).await?;

        // Create response
        self.wire_handler.create_find_response(request_id, response_to, documents, collection)
    }

    async fn process_insert(&self, document: &Document, request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        let collection = document.get_str("insert")
            .map_err(|_| FauxDBError::Command("Missing collection name in insert command".to_string()))?;

        let documents = document.get_array("documents")
            .map_err(|_| FauxDBError::Command("Missing documents in insert command".to_string()))?;

        // Ensure collection exists
        self.db_manager.create_collection(&self.default_database, collection).await?;

        let mut inserted_count = 0;
        for doc_value in documents {
            if let Bson::Document(doc) = doc_value {
                self.db_manager.insert_document(&self.default_database, collection, &doc).await?;
                inserted_count += 1;
            }
        }

        self.wire_handler.create_insert_response(request_id, response_to, inserted_count)
    }

    async fn process_update(&self, document: &Document, request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        let collection = document.get_str("update")
            .map_err(|_| FauxDBError::Command("Missing collection name in update command".to_string()))?;

        let updates = document.get_array("updates")
            .map_err(|_| FauxDBError::Command("Missing updates in update command".to_string()))?;

        let mut modified_count = 0;
        for update_doc in updates {
            if let Bson::Document(update) = update_doc {
                let filter = update.get_document("q")
                    .map_err(|_| FauxDBError::Command("Missing query in update".to_string()))?;
                
                let update_ops = update.get_document("u")
                    .map_err(|_| FauxDBError::Command("Missing update operations".to_string()))?;
                
                let _upsert = update.get_bool("upsert").unwrap_or(false);

                let count = self.db_manager.update_document(&self.default_database, collection, filter, update_ops).await?;
                modified_count += count;
            }
        }

        let response = doc! {
            "ok": 1,
            "n": modified_count as i64,
            "nModified": modified_count as i64
        };

        let bson_doc = bson::to_document(&response)?;
        let bson_bytes = bson::to_vec(&bson_doc)?;
        
        let mut buffer = Vec::new();
        let message_length = 16 + bson_bytes.len() as u32;
        
        buffer.extend_from_slice(&message_length.to_le_bytes());
        buffer.extend_from_slice(&request_id.to_le_bytes());
        buffer.extend_from_slice(&response_to.to_le_bytes());
        buffer.extend_from_slice(&2004u32.to_le_bytes());
        buffer.extend_from_slice(&bson_bytes);

        Ok(buffer)
    }

    async fn process_delete(&self, document: &Document, request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        let collection = document.get_str("delete")
            .map_err(|_| FauxDBError::Command("Missing collection name in delete command".to_string()))?;

        let deletes = document.get_array("deletes")
            .map_err(|_| FauxDBError::Command("Missing deletes in delete command".to_string()))?;

        let mut deleted_count = 0;
        for delete_doc in deletes {
            if let Bson::Document(delete) = delete_doc {
                let filter = delete.get_document("q")
                    .map_err(|_| FauxDBError::Command("Missing query in delete".to_string()))?;

                let count = self.db_manager.delete_document(&self.default_database, collection, filter).await?;
                deleted_count += count;
            }
        }

        let response = doc! {
            "ok": 1,
            "n": deleted_count as i64
        };

        let bson_doc = bson::to_document(&response)?;
        let bson_bytes = bson::to_vec(&bson_doc)?;
        
        let mut buffer = Vec::new();
        let message_length = 16 + bson_bytes.len() as u32;
        
        buffer.extend_from_slice(&message_length.to_le_bytes());
        buffer.extend_from_slice(&request_id.to_le_bytes());
        buffer.extend_from_slice(&response_to.to_le_bytes());
        buffer.extend_from_slice(&2004u32.to_le_bytes());
        buffer.extend_from_slice(&bson_bytes);

        Ok(buffer)
    }

    async fn process_count(&self, document: &Document, request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        let collection = document.get_str("count")
            .map_err(|_| FauxDBError::Command("Missing collection name in count command".to_string()))?;

        let filter = document.get_document("query").ok();
        let count = self.db_manager.count_documents(&self.default_database, collection, filter).await?;

        self.wire_handler.create_count_response(request_id, response_to, count as i64)
    }

    async fn process_aggregate(&self, document: &Document, request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        let collection = document.get_str("aggregate")
            .map_err(|_| FauxDBError::Command("Missing collection name in aggregate command".to_string()))?;

        let _pipeline = document.get_array("pipeline")
            .map_err(|_| FauxDBError::Command("Missing pipeline in aggregate command".to_string()))?;

        // For now, just return a simple aggregation result
        // In a full implementation, this would process the aggregation pipeline
        let count = self.db_manager.count_documents(&self.default_database, collection, None).await?;
        let result_doc = doc! {
            "_id": null,
            "count": count as i64
        };

        self.wire_handler.create_find_response(request_id, response_to, vec![result_doc], collection)
    }

    async fn process_list_collections(&self, _document: &Document, request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        let collections = self.db_manager.list_collections(&self.default_database).await?;
        
        let mut cursor_batch = Vec::new();
        for collection in collections {
            let collection_doc = doc! {
                "name": collection,
                "type": "collection"
            };
            cursor_batch.push(collection_doc);
        }

        let response = doc! {
            "ok": 1,
            "cursor": {
                "firstBatch": cursor_batch,
                "id": 0,
                "ns": "test.$cmd.listCollections"
            }
        };

        let bson_doc = bson::to_document(&response)?;
        let bson_bytes = bson::to_vec(&bson_doc)?;
        
        let mut buffer = Vec::new();
        let message_length = 16 + bson_bytes.len() as u32;
        
        buffer.extend_from_slice(&message_length.to_le_bytes());
        buffer.extend_from_slice(&request_id.to_le_bytes());
        buffer.extend_from_slice(&response_to.to_le_bytes());
        buffer.extend_from_slice(&2004u32.to_le_bytes());
        buffer.extend_from_slice(&bson_bytes);

        Ok(buffer)
    }

    async fn process_create_collection(&self, document: &Document, request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        let collection = document.get_str("create")
            .map_err(|_| FauxDBError::Command("Missing collection name in create command".to_string()))?;

        self.db_manager.create_collection(&self.default_database, collection).await?;

        self.wire_handler.create_success_response(request_id, response_to)
    }

    async fn process_drop_collection(&self, document: &Document, request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        let collection = document.get_str("drop")
            .map_err(|_| FauxDBError::Command("Missing collection name in drop command".to_string()))?;

        self.db_manager.drop_collection(&self.default_database, collection).await?;

        self.wire_handler.create_success_response(request_id, response_to)
    }
}
