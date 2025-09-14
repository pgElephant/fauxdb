/*!
 * @file main.rs
 * @brief FauxDB server main entry point - MongoDB compatible server
 */

mod error;
mod config;
mod wire_protocol;
mod database;
mod command_processor;

use error::Result;
use config::Config;
use wire_protocol::WireProtocolHandler;
use database::DatabaseManager;
use command_processor::CommandProcessor;

use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use std::sync::Arc;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("🚀 FauxDB server starting...");
    
    // Load configuration
    let config = Config::load("config/fauxdb.toml").await?;
    println!("✅ Configuration loaded");
    
    // Initialize database manager
    let db_manager = DatabaseManager::new(config.database.clone()).await?;
    println!("✅ Database manager initialized");
    
    // Initialize command processor
    let command_processor = Arc::new(CommandProcessor::new(db_manager));
    println!("✅ Command processor initialized");
    
    let listener = TcpListener::bind("127.0.0.1:27018").await?;
    println!("✅ FauxDB listening on 127.0.0.1:27018");
    println!("📝 Ready for MongoDB client connections!");
    
    loop {
        match listener.accept().await {
            Ok((stream, addr)) => {
                println!("📡 New connection from: {}", addr);
                let processor = command_processor.clone();
                tokio::spawn(handle_client(stream, processor));
            }
            Err(e) => {
                eprintln!("❌ Failed to accept connection: {}", e);
            }
        }
    }
}

async fn handle_client(mut stream: TcpStream, command_processor: Arc<CommandProcessor>) {
    let mut buffer = [0; 4096];
    let wire_handler = WireProtocolHandler::new();
    
    loop {
        match stream.read(&mut buffer).await {
            Ok(0) => {
                println!("📤 Client disconnected");
                break;
            }
            Ok(n) => {
                println!("📨 Received {} bytes", n);
                
                // Parse MongoDB wire protocol message
                if n >= 16 {
                    match wire_handler.parse_message(&buffer[..n]) {
                        Ok(message) => {
                            println!("🔍 Message: length={}, request_id={}, response_to={}, op_code={}", 
                                     message.message_length, message.request_id, message.response_to, message.op_code);
                            
                            // Process message
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
                                    // Send error response
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
