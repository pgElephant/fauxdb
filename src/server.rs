/*!
 * @file server.rs
 * @brief FauxDB server implementation
 */

use crate::error::Result;
use crate::config::Config;
use crate::documentdb::DocumentDBManager;
use crate::command_processor::CommandProcessor;
use std::sync::Arc;
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{AsyncReadExt, AsyncWriteExt};

pub struct FauxDBServer {
    config: Config,
    db_manager: DocumentDBManager,
    command_processor: Arc<CommandProcessor>,
}

impl FauxDBServer {
    pub async fn new(config: Config) -> Result<Self> {
        let db_manager = DocumentDBManager::new(config.database.clone()).await?;
        let command_processor = Arc::new(CommandProcessor::new(db_manager.clone()));
        
        Ok(Self {
            config,
            db_manager,
            command_processor,
        })
    }

    pub async fn start(&self) -> Result<()> {
        let addr = format!("{}:{}", self.config.server.host, self.config.server.port);
        let listener = TcpListener::bind(&addr).await?;
        
        println!("🚀 FauxDB server listening on {}", addr);
        
        loop {
            match listener.accept().await {
                Ok((stream, addr)) => {
                    println!("📡 New connection from: {}", addr);
                    let processor = self.command_processor.clone();
                    tokio::spawn(handle_client(stream, processor));
                }
                Err(e) => {
                    eprintln!("❌ Failed to accept connection: {}", e);
                }
            }
        }
    }
}

async fn handle_client(mut stream: TcpStream, command_processor: Arc<CommandProcessor>) {
    let mut buffer = [0; 4096];
    let wire_handler = crate::wire_protocol::WireProtocolHandler::new();
    
    loop {
        match stream.read(&mut buffer).await {
            Ok(0) => {
                println!("📤 Client disconnected");
                break;
            }
            Ok(n) => {
                println!("📨 Received {} bytes", n);
                
                if n >= 16 {
                    match wire_handler.parse_message(&buffer[..n]) {
                        Ok(message) => {
                            println!("🔍 Message: length={}, request_id={}, response_to={}, op_code={}", 
                                     message.message_length, message.request_id, message.response_to, message.op_code);
                            
                            match command_processor.process_message(&message).await {
                                Ok(response) => {
                                    if let Err(e) = stream.write_all(&response).await {
                                        eprintln!("❌ Failed to send response: {}", e);
                                        break;
                                    }
                                    println!("📤 Sent response: {} bytes", response.len());
                                }
                                Err(e) => {
                                    eprintln!("❌ Failed to process message: {}", e);
                                    if let Ok(error_response) = wire_handler.create_error_response(
                                        message.request_id, 
                                        message.response_to, 
                                        &format!("{}", e)
                                    ) {
                                        let _ = stream.write_all(&error_response).await;
                                    }
                                    break;
                                }
                            }
                        }
                        Err(e) => {
                            eprintln!("❌ Failed to parse message: {}", e);
                            break;
                        }
                    }
                }
            }
            Err(e) => {
                eprintln!("❌ Failed to read from client: {}", e);
                break;
            }
        }
    }
}
