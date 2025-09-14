/*!
 * @file wire_protocol.rs
 * @brief MongoDB wire protocol handling
 */

use crate::error::{FauxDBError, Result};
use bson::{Document, Bson, doc};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone)]
pub struct MongoMessage {
    pub message_length: u32,
    pub request_id: u32,
    pub response_to: u32,
    pub op_code: u32,
    pub payload: Vec<u8>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MongoResponse {
    pub ok: i32,
    pub errmsg: Option<String>,
    pub cursor: Option<CursorResponse>,
    pub n: Option<i64>,
    #[serde(flatten)]
    pub extra: Document,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CursorResponse {
    pub first_batch: Vec<Document>,
    pub id: i64,
    pub ns: String,
}

pub struct WireProtocolHandler;

impl WireProtocolHandler {
    pub fn new() -> Self {
        Self
    }

    pub fn parse_message(&self, buffer: &[u8]) -> Result<MongoMessage> {
        if buffer.len() < 16 {
            return Err(FauxDBError::WireProtocol("Message too short".to_string()));
        }

        let message_length = u32::from_le_bytes([buffer[0], buffer[1], buffer[2], buffer[3]]);
        let request_id = u32::from_le_bytes([buffer[4], buffer[5], buffer[6], buffer[7]]);
        let response_to = u32::from_le_bytes([buffer[8], buffer[9], buffer[10], buffer[11]]);
        let op_code = u32::from_le_bytes([buffer[12], buffer[13], buffer[14], buffer[15]]);

        let payload = buffer[16..].to_vec();

        Ok(MongoMessage {
            message_length,
            request_id,
            response_to,
            op_code,
            payload,
        })
    }

    pub fn generate_response(&self, request_id: u32, response_to: u32, op_code: u32, response: &MongoResponse) -> Result<Vec<u8>> {
        let mut buffer = Vec::new();
        
        // Convert response to BSON
        let bson_doc = bson::to_document(response)?;
        let bson_bytes = bson::to_vec(&bson_doc)?;
        
        // Calculate message length
        let message_length = 16 + bson_bytes.len() as u32;
        
        // Write header
        buffer.extend_from_slice(&message_length.to_le_bytes());
        buffer.extend_from_slice(&request_id.to_le_bytes());
        buffer.extend_from_slice(&response_to.to_le_bytes());
        buffer.extend_from_slice(&op_code.to_le_bytes());
        
        // Write BSON payload
        buffer.extend_from_slice(&bson_bytes);
        
        Ok(buffer)
    }

    pub fn parse_op_msg(&self, payload: &[u8]) -> Result<Document> {
        bson::from_slice::<Document>(payload)
            .map_err(|e| FauxDBError::WireProtocol(format!("Failed to parse OP_MSG: {}", e)))
    }

    pub fn create_success_response(&self, request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        let response = MongoResponse {
            ok: 1,
            errmsg: None,
            cursor: None,
            n: None,
            extra: doc! {},
        };
        
        self.generate_response(request_id, response_to, 2004, &response)
    }

    pub fn create_error_response(&self, request_id: u32, response_to: u32, error_msg: &str) -> Result<Vec<u8>> {
        let response = MongoResponse {
            ok: 0,
            errmsg: Some(error_msg.to_string()),
            cursor: None,
            n: None,
            extra: doc! {},
        };
        
        self.generate_response(request_id, response_to, 2004, &response)
    }

    pub fn create_ping_response(&self, request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        let response = MongoResponse {
            ok: 1,
            errmsg: None,
            cursor: None,
            n: None,
            extra: doc! {
                "pong": Bson::Boolean(true)
            },
        };
        
        self.generate_response(request_id, response_to, 2004, &response)
    }

    pub fn create_hello_response(&self, request_id: u32, response_to: u32) -> Result<Vec<u8>> {
        let response = MongoResponse {
            ok: 1,
            errmsg: None,
            cursor: None,
            n: None,
            extra: doc! {
                "hello": "FauxDB",
                "version": "1.0.0",
                "maxBsonObjectSize": 16777216,
                "maxMessageSizeBytes": 48000000,
                "maxWriteBatchSize": 100000,
                "localTime": "2024-01-01T00:00:00.000Z",
                "logicalSessionTimeoutMinutes": 30,
                "connectionId": 1,
                "minWireVersion": 0,
                "maxWireVersion": 13,
                "readOnly": false
            },
        };
        
        self.generate_response(request_id, response_to, 2004, &response)
    }

    pub fn create_find_response(&self, request_id: u32, response_to: u32, documents: Vec<Document>, collection: &str) -> Result<Vec<u8>> {
        let cursor = CursorResponse {
            first_batch: documents,
            id: 0,
            ns: format!("test.{}", collection),
        };

        let response = MongoResponse {
            ok: 1,
            errmsg: None,
            cursor: Some(cursor),
            n: None,
            extra: doc! {},
        };
        
        self.generate_response(request_id, response_to, 2004, &response)
    }

    pub fn create_count_response(&self, request_id: u32, response_to: u32, count: i64) -> Result<Vec<u8>> {
        let response = MongoResponse {
            ok: 1,
            errmsg: None,
            cursor: None,
            n: Some(count),
            extra: doc! {},
        };
        
        self.generate_response(request_id, response_to, 2004, &response)
    }

    pub fn create_insert_response(&self, request_id: u32, response_to: u32, inserted_count: i64) -> Result<Vec<u8>> {
        let response = MongoResponse {
            ok: 1,
            errmsg: None,
            cursor: None,
            n: Some(inserted_count),
            extra: doc! {
                "opTime": doc! {
                    "ts": doc! {
                        "$timestamp": doc! {
                            "t": 1640995200,
                            "i": 1
                        }
                    },
                    "t": 1
                },
                "electionId": doc! {
                    "$oid": "7fffffff0000000000000001"
                }
            },
        };
        
        self.generate_response(request_id, response_to, 2004, &response)
    }
}
