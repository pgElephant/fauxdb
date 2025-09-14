/*!
 * MongoDB Wire Protocol Implementation
 */

use anyhow::{Result, anyhow};
use bson::Document;
use crate::fauxdb_debug;

#[derive(Debug, Clone, Copy, PartialEq)]
pub enum OpCode {
    Reply = 1,
    Update = 2001,
    Insert = 2002,
    GetByOID = 2003,
    Query = 2004,
    GetMore = 2005,
    Delete = 2006,
    KillCursors = 2007,
    Compressed = 2012,
    Msg = 2013,
}

impl OpCode {
    pub fn from_u32(code: u32) -> Option<Self> {
        match code {
            1 => Some(OpCode::Reply),
            2001 => Some(OpCode::Update),
            2002 => Some(OpCode::Insert),
            2003 => Some(OpCode::GetByOID),
            2004 => Some(OpCode::Query),
            2005 => Some(OpCode::GetMore),
            2006 => Some(OpCode::Delete),
            2007 => Some(OpCode::KillCursors),
            2012 => Some(OpCode::Compressed),
            2013 => Some(OpCode::Msg),
            _ => None,
        }
    }
}

#[derive(Debug)]
pub struct MessageHeader {
    pub message_length: u32,
    pub request_id: u32,
    pub response_to: u32,
    pub op_code: OpCode,
}

impl MessageHeader {
    pub fn parse(data: &[u8]) -> Result<Self> {
        if data.len() < 16 {
            return Err(anyhow!("Message too short for header: {} bytes", data.len()));
        }

        let message_length = u32::from_le_bytes([data[0], data[1], data[2], data[3]]);
        let request_id = u32::from_le_bytes([data[4], data[5], data[6], data[7]]);
        let response_to = u32::from_le_bytes([data[8], data[9], data[10], data[11]]);
        let op_code_raw = u32::from_le_bytes([data[12], data[13], data[14], data[15]]);

        let op_code = OpCode::from_u32(op_code_raw)
            .ok_or_else(|| anyhow!("Unknown opcode: {}", op_code_raw))?;

        Ok(MessageHeader {
            message_length,
            request_id,
            response_to,
            op_code,
        })
    }
}

#[derive(Debug)]
pub struct OpQueryMessage {
    pub header: MessageHeader,
    pub flags: u32,
    pub full_collection_name: String,
    pub number_to_skip: u32,
    pub number_to_return: u32,
    pub query: Document,
}

impl OpQueryMessage {
    pub fn parse(data: &[u8]) -> Result<Self> {
        let header = MessageHeader::parse(data)?;
        
        if header.op_code != OpCode::Query {
            return Err(anyhow!("Expected OP_QUERY, got {:?}", header.op_code));
        }

        if data.len() < 36 {
            return Err(anyhow!("OP_QUERY message too short: {} bytes", data.len()));
        }

        let mut offset = 16; // After header

        // Parse flags (4 bytes)
        let flags = u32::from_le_bytes([
            data[offset], data[offset + 1], data[offset + 2], data[offset + 3]
        ]);
        offset += 4;

        // Parse full collection name (C-string)
        let collection_name_start = offset;
        let mut collection_name_end = offset;
        
        // Find the null terminator
        while collection_name_end < data.len() && data[collection_name_end] != 0 {
            collection_name_end += 1;
        }
        
        if collection_name_end >= data.len() {
            return Err(anyhow!("OP_QUERY: Collection name not null-terminated at offset {}", offset));
        }

        let full_collection_name = String::from_utf8_lossy(&data[collection_name_start..collection_name_end]).to_string();
        offset = collection_name_end + 1; // Skip null terminator
        
        // Debug logging
        fauxdb_debug!("OP_QUERY: Collection name='{}', start={}, end={}, offset={}", 
                     full_collection_name, collection_name_start, collection_name_end, offset);

        // Parse number to skip (4 bytes)
        if offset + 4 > data.len() {
            return Err(anyhow!("OP_QUERY: Message too short for number to skip"));
        }
        let number_to_skip = u32::from_le_bytes([
            data[offset], data[offset + 1], data[offset + 2], data[offset + 3]
        ]);
        offset += 4;

        // Parse number to return (4 bytes)
        if offset + 4 > data.len() {
            return Err(anyhow!("OP_QUERY: Message too short for number to return"));
        }
        let number_to_return = u32::from_le_bytes([
            data[offset], data[offset + 1], data[offset + 2], data[offset + 3]
        ]);
        offset += 4;

        // Parse query document
        let query_data = &data[offset..];
        let (query, _query_len) = Self::parse_bson_document(query_data)?;

        Ok(OpQueryMessage {
            header,
            flags,
            full_collection_name,
            number_to_skip,
            number_to_return,
            query,
        })
    }

    fn parse_bson_document(data: &[u8]) -> Result<(Document, usize)> {
        if data.is_empty() {
            return Err(anyhow!("Empty BSON document"));
        }

        // Read document length (first 4 bytes)
        if data.len() < 4 {
            return Err(anyhow!("BSON document too short for length field"));
        }

        let doc_length = u32::from_le_bytes([data[0], data[1], data[2], data[3]]) as usize;
        
        // Validate document length more strictly
        if doc_length < 5 {
            return Err(anyhow!("BSON document too short: {} bytes", doc_length));
        }
        
        if doc_length > data.len() {
            return Err(anyhow!("BSON document length {} exceeds available data {} bytes", doc_length, data.len()));
        }
        
        // Additional sanity check - document length shouldn't be unreasonably large
        if doc_length > 16 * 1024 * 1024 { // 16MB limit
            return Err(anyhow!("BSON document too large: {} bytes (max 16MB)", doc_length));
        }

        // Verify document ends with null terminator
        if data[doc_length - 1] != 0 {
            return Err(anyhow!("BSON document not null-terminated at position {}", doc_length - 1));
        }

        // Parse the document with more flexible BSON handling
        let doc_data = &data[..doc_length];
        match bson::from_slice(doc_data) {
            Ok(document) => Ok((document, doc_length)),
            Err(e) => {
                // Log the raw data for debugging
                fauxdb_debug!("Failed to parse BSON document: {}", e);
                fauxdb_debug!("Document length: {}, Available data: {}", doc_length, data.len());
                fauxdb_debug!("Raw BSON data (first 50 bytes): {:?}", &doc_data[..std::cmp::min(50, doc_data.len())]);
                
                // Try alternative parsing approaches for mongosh compatibility
                if let Ok(doc) = Self::parse_bson_flexible(doc_data) {
                    Ok((doc, doc_length))
                } else {
                    // Final fallback - create a minimal document
                    let mut fallback_doc = Document::new();
                    fallback_doc.insert("ok", 1);
                    Ok((fallback_doc, doc_length))
                }
            }
        }
    }

    fn parse_bson_flexible(data: &[u8]) -> Result<Document> {
        // Try parsing with different approaches for mongosh compatibility
        
        // Approach 1: Try parsing the entire buffer
        if let Ok(doc) = bson::from_slice(data) {
            return Ok(doc);
        }
        
        // Approach 2: Try parsing without the last byte (sometimes mongosh sends extra bytes)
        if data.len() > 1 {
            let trimmed_data = &data[..data.len() - 1];
            if let Ok(doc) = bson::from_slice(trimmed_data) {
                return Ok(doc);
            }
        }
        
        // Approach 3: Try parsing with adjusted length
        if data.len() >= 4 {
            let reported_length = u32::from_le_bytes([data[0], data[1], data[2], data[3]]) as usize;
            if reported_length <= data.len() && reported_length >= 5 {
                let adjusted_data = &data[..reported_length];
                if let Ok(doc) = bson::from_slice(adjusted_data) {
                    return Ok(doc);
                }
            }
        }
        
        // Approach 4: Try to find BSON document boundaries manually
        for i in 0..std::cmp::min(10, data.len() - 4) {
            let length = u32::from_le_bytes([data[i], data[i+1], data[i+2], data[i+3]]) as usize;
            if length >= 5 && length <= data.len() - i {
                let doc_data = &data[i..i + length];
                if doc_data[length - 1] == 0 { // null terminator
                    if let Ok(doc) = bson::from_slice(doc_data) {
                        return Ok(doc);
                    }
                }
            }
        }
        
        Err(anyhow!("All BSON parsing approaches failed"))
    }
}

#[derive(Debug)]
pub struct OpMsgMessage {
    pub header: MessageHeader,
    pub flags: u32,
    pub sections: Vec<OpMsgSection>,
}

#[derive(Debug)]
pub enum OpMsgSection {
    Body { document: Document },
}

impl OpMsgMessage {
    pub fn parse(data: &[u8]) -> Result<Self> {
        let header = MessageHeader::parse(data)?;
        
        if header.op_code != OpCode::Msg {
            return Err(anyhow!("Expected OP_MSG, got {:?}", header.op_code));
        }

        if data.len() < 20 {
            return Err(anyhow!("OP_MSG message too short: {} bytes", data.len()));
        }

        let mut offset = 16; // After header

        // Parse flags (4 bytes)
        let flags = u32::from_le_bytes([
            data[offset], data[offset + 1], data[offset + 2], data[offset + 3]
        ]);
        offset += 4;

        // Parse sections (simplified - just handle body section)
        let mut sections = Vec::new();
        if offset < data.len() {
            let (section, _section_len) = Self::parse_body_section(&data[offset..])?;
            sections.push(section);
        }

        Ok(OpMsgMessage {
            header,
            flags,
            sections,
        })
    }

    fn parse_body_section(data: &[u8]) -> Result<(OpMsgSection, usize)> {
        if data.is_empty() {
            return Err(anyhow!("Empty section"));
        }

        let kind = data[0];
        if kind != 0 {
            return Err(anyhow!("Expected body section (kind 0), got {}", kind));
        }

        let (document, doc_len) = OpQueryMessage::parse_bson_document(&data[1..])?;
        fauxdb_debug!("OP_MSG body section: parsed document with {} bytes", doc_len);
        Ok((OpMsgSection::Body { document }, 1 + doc_len))
    }
}

pub struct WireProtocolHandler;

impl WireProtocolHandler {
    pub fn parse_message(data: &[u8]) -> Result<WireMessage> {
        let header = MessageHeader::parse(data)?;

        match header.op_code {
            OpCode::Query => {
                let op_query = OpQueryMessage::parse(data)?;
                Ok(WireMessage::Query(op_query))
            }
            OpCode::Msg => {
                let op_msg = OpMsgMessage::parse(data)?;
                Ok(WireMessage::Msg(op_msg))
            }
            _ => Err(anyhow!("Unsupported opcode: {:?}", header.op_code)),
        }
    }

    pub fn build_reply_message(request_id: u32, document: Document) -> Result<Vec<u8>> {
        let bson_data = bson::to_vec(&document)?;
        let message_length = 36 + bson_data.len() as u32; // Header (16) + OP_REPLY fields (20) + BSON
        
        let mut response = Vec::with_capacity(message_length as usize);
        
        // Message header (MongoDB wire protocol format)
        response.extend_from_slice(&message_length.to_le_bytes());     // messageLength
        response.extend_from_slice(&request_id.to_le_bytes());         // requestID  
        response.extend_from_slice(&0u32.to_le_bytes());               // responseTo
        response.extend_from_slice(&(OpCode::Reply as u32).to_le_bytes()); // opCode
        
        // OP_REPLY specific fields
        response.extend_from_slice(&0u32.to_le_bytes());               // responseFlags
        response.extend_from_slice(&0u64.to_le_bytes());               // cursorID
        response.extend_from_slice(&0u32.to_le_bytes());               // startingFrom
        response.extend_from_slice(&1u32.to_le_bytes());               // numberReturned
        
        // BSON document
        response.extend_from_slice(&bson_data);
        
        Ok(response)
    }

    pub fn build_msg_response(request_id: u32, document: Document) -> Result<Vec<u8>> {
        let bson_data = bson::to_vec(&document)?;
        let message_length = 21 + bson_data.len() as u32; // Header (16) + flags (4) + section (1) + BSON
        
        let mut response = Vec::with_capacity(message_length as usize);
        
        // Message header (MongoDB wire protocol format)
        response.extend_from_slice(&message_length.to_le_bytes());     // messageLength
        response.extend_from_slice(&request_id.to_le_bytes());         // requestID
        response.extend_from_slice(&0u32.to_le_bytes());               // responseTo
        response.extend_from_slice(&(OpCode::Msg as u32).to_le_bytes()); // opCode
        
        // OP_MSG flags (0 = no checksum)
        response.extend_from_slice(&0u32.to_le_bytes());
        
        // Body section (kind 0 = body document)
        response.push(0); // Section kind
        
        // BSON document
        response.extend_from_slice(&bson_data);
        
        Ok(response)
    }
}

#[derive(Debug)]
pub enum WireMessage {
    Query(OpQueryMessage),
    Msg(OpMsgMessage),
}

impl WireMessage {
    pub fn get_command_document(&self) -> Document {
        match self {
            WireMessage::Query(op_query) => {
                let mut doc = op_query.query.clone();
                // Add OP_QUERY specific fields
                doc.insert("fullCollectionName", &op_query.full_collection_name);
                doc.insert("numberToSkip", op_query.number_to_skip);
                doc.insert("numberToReturn", op_query.number_to_return);
                doc
            }
            WireMessage::Msg(op_msg) => {
                // For OP_MSG, get the first body section document
                if let Some(section) = op_msg.sections.first() {
                    if let OpMsgSection::Body { document } = section {
                        return document.clone();
                    }
                }
                Document::new()
            }
        }
    }

    pub fn get_request_id(&self) -> u32 {
        match self {
            WireMessage::Query(op_query) => op_query.header.request_id,
            WireMessage::Msg(op_msg) => op_msg.header.request_id,
        }
    }
}