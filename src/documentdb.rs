/*!
 * @file documentdb.rs
 * @brief DocumentDB integration for FauxDB
 */

use crate::error::{FauxDBError, Result};
use crate::config::DatabaseConfig;
use bson::Document;
use std::collections::HashMap;
use tokio_postgres::NoTls;
use deadpool_postgres::{Pool, Manager};
use serde_json::Value;

pub struct DocumentDBManager {
    pool: Pool,
    config: DatabaseConfig,
}

impl DocumentDBManager {
    pub async fn new(config: DatabaseConfig) -> Result<Self> {
        let pg_config = config.uri.parse()
            .map_err(|e| FauxDBError::Database(format!("Invalid PostgreSQL URI: {}", e)))?;

        let manager = Manager::new(pg_config, NoTls);
        let pool = Pool::builder(manager)
            .max_size(config.max_connections as usize)
            .build()
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to build connection pool: {}", e)))?;

        // Initialize DocumentDB extension
        let client = pool.get().await
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to get database connection: {}", e)))?;
        
        // Create DocumentDB core extension if it doesn't exist
        client.execute("CREATE EXTENSION IF NOT EXISTS documentdb_core", &[]).await
            .map_err(|e| FauxDBError::Database(format!("Failed to create DocumentDB core extension: {}", e)))?;

        Ok(Self { pool, config })
    }

    pub async fn create_collection(&self, database: &str, collection: &str) -> Result<()> {
        let client = self.pool.get().await
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to get database connection: {}", e)))?;

        let schema_name = format!("documentdb_{}", database);
        let table_name = format!("{}_collections", collection);

        // Create schema if it doesn't exist
        let create_schema = format!("CREATE SCHEMA IF NOT EXISTS {}", schema_name);
        client.execute(&create_schema, &[]).await
            .map_err(|e| FauxDBError::Database(format!("Failed to create schema: {}", e)))?;

        // Create collection table with BSON support
        let create_table = format!(
            "CREATE TABLE IF NOT EXISTS {}.{} (
                id SERIAL PRIMARY KEY,
                document JSONB,
                bson_document BYTEA,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )",
            schema_name, table_name
        );
        
        client.execute(&create_table, &[]).await
            .map_err(|e| FauxDBError::Database(format!("Failed to create collection table: {}", e)))?;

        // Create indexes for better performance
        let create_indexes = format!(
            "CREATE INDEX IF NOT EXISTS idx_{}_document_gin ON {}.{} USING GIN (document);
             CREATE INDEX IF NOT EXISTS idx_{}_created_at ON {}.{} (created_at);
             CREATE INDEX IF NOT EXISTS idx_{}_updated_at ON {}.{} (updated_at);",
            collection, schema_name, table_name,
            collection, schema_name, table_name,
            collection, schema_name, table_name
        );
        
        client.execute(&create_indexes, &[]).await
            .map_err(|e| FauxDBError::Database(format!("Failed to create indexes: {}", e)))?;

        Ok(())
    }

    pub async fn insert_document(&self, database: &str, collection: &str, document: &Document) -> Result<String> {
        let client = self.pool.get().await
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to get database connection: {}", e)))?;

        let schema_name = format!("documentdb_{}", database);
        let table_name = format!("{}_collections", collection);

        // Convert BSON document to JSON
        let json_value = bson::to_bson(document)
            .map_err(|e| FauxDBError::Bson(format!("Failed to convert BSON to JSON: {}", e)))?;
        
        let json_str = serde_json::to_string(&json_value)
            .map_err(|e| FauxDBError::Serialization(format!("Failed to serialize JSON: {}", e)))?;

        // Convert BSON to bytes
        let bson_bytes = bson::to_vec(document)
            .map_err(|e| FauxDBError::Bson(format!("Failed to serialize BSON: {}", e)))?;

        let insert_query = format!(
            "INSERT INTO {}.{} (document, bson_document) VALUES ($1::jsonb, $2) RETURNING id",
            schema_name, table_name
        );

        let row = client.query_one(&insert_query, &[&json_str, &bson_bytes]).await
            .map_err(|e| FauxDBError::Database(format!("Failed to insert document: {}", e)))?;

        let id: i32 = row.get("id");
        Ok(id.to_string())
    }

    pub async fn find_documents(&self, database: &str, collection: &str, filter: Option<&Document>, skip: Option<u64>, limit: Option<u64>) -> Result<Vec<Document>> {
        let client = self.pool.get().await
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to get database connection: {}", e)))?;

        let schema_name = format!("documentdb_{}", database);
        let table_name = format!("{}_collections", collection);

        let mut query = format!("SELECT document FROM {}.{}", schema_name, table_name);
        let mut params: Vec<Box<dyn tokio_postgres::types::ToSql + Sync>> = Vec::new();
        let mut param_count = 0;

        // Add WHERE clause if filter is provided
        if let Some(filter_doc) = filter {
            if !filter_doc.is_empty() {
                query.push_str(" WHERE ");
                for (key, value) in filter_doc {
                    if param_count > 0 {
                        query.push_str(" AND ");
                    }
                    param_count += 1;
                    query.push_str(&format!("document->>'{}' = ${}", key, param_count));
                    
                    let value_str = match value.as_str() {
                        Some(s) => s.to_string(),
                        None => value.to_string(),
                    };
                    params.push(Box::new(value_str));
                }
            }
        }

        // Add ORDER BY
        query.push_str(" ORDER BY id");

        // Add LIMIT and OFFSET
        if let Some(skip_val) = skip {
            query.push_str(&format!(" OFFSET {}", skip_val));
        }
        if let Some(limit_val) = limit {
            query.push_str(&format!(" LIMIT {}", limit_val));
        }

        let rows = client.query(&query, &params.iter().map(|p| p.as_ref()).collect::<Vec<_>>()).await
            .map_err(|e| FauxDBError::Database(format!("Failed to query documents: {}", e)))?;

        let mut documents = Vec::new();
        for row in rows {
            let json_str: String = row.get("document");
            let json_value: Value = serde_json::from_str(&json_str)
                .map_err(|e| FauxDBError::Serialization(format!("Failed to parse JSON: {}", e)))?;
            
            let document = bson::to_bson(&json_value)
                .map_err(|e| FauxDBError::Bson(format!("Failed to convert JSON to BSON: {}", e)))?
                .as_document()
                .ok_or_else(|| FauxDBError::Bson("Failed to convert to document".to_string()))?
                .clone();
            
            documents.push(document);
        }

        Ok(documents)
    }

    pub async fn update_document(&self, database: &str, collection: &str, filter: &Document, update: &Document) -> Result<u64> {
        let client = self.pool.get().await
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to get database connection: {}", e)))?;

        let schema_name = format!("documentdb_{}", database);
        let table_name = format!("{}_collections", collection);

        // Convert update document to JSON
        let update_json = bson::to_bson(update)
            .map_err(|e| FauxDBError::Bson(format!("Failed to convert update to JSON: {}", e)))?;
        
        let update_str = serde_json::to_string(&update_json)
            .map_err(|e| FauxDBError::Serialization(format!("Failed to serialize update: {}", e)))?;

        // Build WHERE clause from filter
        let mut where_clause = String::new();
        let mut params: Vec<Box<dyn tokio_postgres::types::ToSql + Sync>> = Vec::new();
        let mut param_count = 0;

        for (key, value) in filter {
            if param_count > 0 {
                where_clause.push_str(" AND ");
            }
            param_count += 1;
            where_clause.push_str(&format!("document->>'{}' = ${}", key, param_count));
            
            let value_str = match value.as_str() {
                Some(s) => s.to_string(),
                None => value.to_string(),
            };
            params.push(Box::new(value_str));
        }

        let update_query = format!(
            "UPDATE {}.{} SET document = document || $1::jsonb, updated_at = CURRENT_TIMESTAMP WHERE {}",
            schema_name, table_name, where_clause
        );

        params.insert(0, Box::new(update_str));

        let modified_count = client.execute(&update_query, &params.iter().map(|p| p.as_ref()).collect::<Vec<_>>()).await
            .map_err(|e| FauxDBError::Database(format!("Failed to update document: {}", e)))?;

        Ok(modified_count)
    }

    pub async fn delete_document(&self, database: &str, collection: &str, filter: &Document) -> Result<u64> {
        let client = self.pool.get().await
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to get database connection: {}", e)))?;

        let schema_name = format!("documentdb_{}", database);
        let table_name = format!("{}_collections", collection);

        // Build WHERE clause from filter
        let mut where_clause = String::new();
        let mut params: Vec<Box<dyn tokio_postgres::types::ToSql + Sync>> = Vec::new();
        let mut param_count = 0;

        for (key, value) in filter {
            if param_count > 0 {
                where_clause.push_str(" AND ");
            }
            param_count += 1;
            where_clause.push_str(&format!("document->>'{}' = ${}", key, param_count));
            
            let value_str = match value.as_str() {
                Some(s) => s.to_string(),
                None => value.to_string(),
            };
            params.push(Box::new(value_str));
        }

        let delete_query = format!(
            "DELETE FROM {}.{} WHERE {}",
            schema_name, table_name, where_clause
        );

        let deleted_count = client.execute(&delete_query, &params.iter().map(|p| p.as_ref()).collect::<Vec<_>>()).await
            .map_err(|e| FauxDBError::Database(format!("Failed to delete document: {}", e)))?;

        Ok(deleted_count)
    }

    pub async fn count_documents(&self, database: &str, collection: &str, filter: Option<&Document>) -> Result<u64> {
        let client = self.pool.get().await
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to get database connection: {}", e)))?;

        let schema_name = format!("documentdb_{}", database);
        let table_name = format!("{}_collections", collection);

        let mut query = format!("SELECT COUNT(*) FROM {}.{}", schema_name, table_name);
        let mut params: Vec<Box<dyn tokio_postgres::types::ToSql + Sync>> = Vec::new();
        let mut param_count = 0;

        // Add WHERE clause if filter is provided
        if let Some(filter_doc) = filter {
            if !filter_doc.is_empty() {
                query.push_str(" WHERE ");
                for (key, value) in filter_doc {
                    if param_count > 0 {
                        query.push_str(" AND ");
                    }
                    param_count += 1;
                    query.push_str(&format!("document->>'{}' = ${}", key, param_count));
                    
                    let value_str = match value.as_str() {
                        Some(s) => s.to_string(),
                        None => value.to_string(),
                    };
                    params.push(Box::new(value_str));
                }
            }
        }

        let row = client.query_one(&query, &params.iter().map(|p| p.as_ref()).collect::<Vec<_>>()).await
            .map_err(|e| FauxDBError::Database(format!("Failed to count documents: {}", e)))?;

        let count: i64 = row.get(0);
        Ok(count as u64)
    }

    pub async fn list_collections(&self, database: &str) -> Result<Vec<String>> {
        let client = self.pool.get().await
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to get database connection: {}", e)))?;

        let schema_name = format!("documentdb_{}", database);

        let query = format!(
            "SELECT table_name FROM information_schema.tables 
             WHERE table_schema = '{}' AND table_name LIKE '%_collections'",
            schema_name
        );

        let rows = client.query(&query, &[]).await
            .map_err(|e| FauxDBError::Database(format!("Failed to list collections: {}", e)))?;

        let mut collections = Vec::new();
        for row in rows {
            let table_name: String = row.get("table_name");
            let collection_name = table_name.replace("_collections", "");
            collections.push(collection_name);
        }

        Ok(collections)
    }

    pub async fn list_databases(&self) -> Result<Vec<String>> {
        let client = self.pool.get().await
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to get database connection: {}", e)))?;

        let query = "
            SELECT schema_name FROM information_schema.schemata 
            WHERE schema_name LIKE 'documentdb_%'
        ";

        let rows = client.query(query, &[]).await
            .map_err(|e| FauxDBError::Database(format!("Failed to list databases: {}", e)))?;

        let mut databases = Vec::new();
        for row in rows {
            let schema_name: String = row.get("schema_name");
            let database_name = schema_name.replace("documentdb_", "");
            databases.push(database_name);
        }

        Ok(databases)
    }
}
