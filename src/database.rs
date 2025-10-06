/*!
 * @file database.rs
 * @brief Database management for FauxDB
 */

use crate::error::{FauxDBError, Result};
use crate::config::DatabaseConfig;
use bson::Document;
use std::collections::HashMap;
use tokio_postgres::NoTls;
use deadpool_postgres::{Pool, Manager};
use serde_json::Value;

pub struct DatabaseManager {
    pool: Pool,
    config: DatabaseConfig,
}

impl DatabaseManager {
    pub async fn new(config: DatabaseConfig) -> Result<Self> {
        let pg_config = config.uri.parse()
            .map_err(|e| FauxDBError::Database(format!("Invalid PostgreSQL URI: {}", e)))?;

        let manager = Manager::new(pg_config, NoTls);
        let pool = Pool::builder(manager)
            .max_size(config.max_connections as usize)
            .build()
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to build connection pool: {}", e)))?;

        Ok(Self { pool, config })
    }

    pub async fn execute_query(&self, query: &str, params: &[&(dyn tokio_postgres::types::ToSql + Sync)]) -> Result<Vec<HashMap<String, Value>>> {
        let client = self.pool.get().await
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to get database connection: {}", e)))?;

        let rows = client.query(query, params).await
            .map_err(|e| FauxDBError::Database(format!("Query execution failed: {}", e)))?;

        let mut results = Vec::new();
        for row in rows {
            let mut map = HashMap::new();
            for (i, column) in row.columns().iter().enumerate() {
                let value_str: String = row.get(i);
                let value: Value = serde_json::from_str(&value_str).unwrap_or(Value::Null);
                map.insert(column.name().to_string(), value);
            }
            results.push(map);
        }

        Ok(results)
    }

    pub async fn execute_command(&self, query: &str, params: &[&(dyn tokio_postgres::types::ToSql + Sync)]) -> Result<u64> {
        let client = self.pool.get().await
            .map_err(|e| FauxDBError::ConnectionPool(format!("Failed to get database connection: {}", e)))?;

        client.execute(query, params).await
            .map_err(|e| FauxDBError::Database(format!("Command execution failed: {}", e)))
    }

    pub async fn query_documents(&self, collection: &str, _filter: Option<&Document>, _projection: Option<&Document>, skip: Option<i64>, limit: Option<i64>) -> Result<Vec<Document>> {
        let mut query = format!("SELECT data FROM {}", collection);

        if let Some(s) = skip {
            query.push_str(&format!(" OFFSET {}", s));
        }
        if let Some(l) = limit {
            query.push_str(&format!(" LIMIT {}", l));
        }

        let results = self.execute_query(&query, &[]).await?;

        let mut documents = Vec::new();
        for row in results {
            if let Some(data_value) = row.get("data") {
                if let Ok(data_str) = serde_json::to_string(data_value) {
                    if let Ok(doc) = serde_json::from_str::<Document>(&data_str) {
                        documents.push(doc);
                    }
                }
            }
        }

        Ok(documents)
    }

    pub async fn insert_document(&self, collection: &str, document: &Document) -> Result<()> {
        // Validate collection name
        if collection.is_empty() || !collection.chars().all(|c| c.is_alphanumeric() || c == '_') {
            return Err(FauxDBError::Database("Invalid collection name".to_string()));
        }
        
        // Validate document is not empty
        if document.is_empty() {
            return Err(FauxDBError::Database("Document cannot be empty".to_string()));
        }
        
        let doc_json = serde_json::to_string(document)
            .map_err(|e| FauxDBError::Database(format!("Failed to serialize document: {}", e)))?;
            
        let query = format!("INSERT INTO {} (data) VALUES ($1)", collection);
        self.execute_command(&query, &[&doc_json]).await?;
        
        println!("✅ Document inserted into collection: {}", collection);
        Ok(())
    }

    pub async fn update_document(&self, collection: &str, filter: &Document, update: &Document, upsert: bool) -> Result<u64> {
        let existing_docs = self.query_documents(collection, Some(filter), None, None, None).await?;
        
        if existing_docs.is_empty() && !upsert {
            return Ok(0);
        }

        let mut updated_count = 0;
        for mut doc in existing_docs.clone() {
            // Apply update logic (simplified)
            for (key, value) in update.iter() {
                doc.insert(key, value.clone());
            }
            let doc_json = serde_json::to_string(&doc)?;
            let filter_json = serde_json::to_string(filter)?;
            let query = format!("UPDATE {} SET data = $1 WHERE data @> $2", collection);
            updated_count += self.execute_command(&query, &[&doc_json, &filter_json]).await?;
        }

        if existing_docs.is_empty() && upsert {
            self.insert_document(collection, update).await?;
            updated_count += 1;
        }

        Ok(updated_count)
    }

    pub async fn delete_document(&self, collection: &str, filter: &Document) -> Result<u64> {
        let filter_json = serde_json::to_string(filter)?;
        let query = format!("DELETE FROM {} WHERE data @> $1", collection);
        self.execute_command(&query, &[&filter_json]).await
    }

    pub async fn count_documents(&self, collection: &str, _filter: Option<&Document>) -> Result<i64> {
        let query = format!("SELECT COUNT(*) as count FROM {}", collection);
        let results = self.execute_query(&query, &[]).await?;

        for row in results {
            if let Some(count_value) = row.get("count") {
                if let Some(count) = count_value.as_i64() {
                    return Ok(count);
                }
            }
        }

        Ok(0)
    }

    pub async fn create_collection(&self, collection: &str) -> Result<()> {
        let query = format!(
            "CREATE TABLE IF NOT EXISTS {} (
                id SERIAL PRIMARY KEY,
                data JSONB NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )",
            collection
        );
        
        self.execute_command(&query, &[]).await?;
        
        // Create index on data column for better performance
        let index_query = format!("CREATE INDEX IF NOT EXISTS idx_{}_data ON {} USING GIN (data)", collection, collection);
        self.execute_command(&index_query, &[]).await?;
        
        Ok(())
    }

    pub async fn drop_collection(&self, collection: &str) -> Result<()> {
        let query = format!("DROP TABLE IF EXISTS {}", collection);
        self.execute_command(&query, &[]).await?;
        Ok(())
    }

    pub async fn list_collections(&self) -> Result<Vec<String>> {
        let query = "SELECT table_name FROM information_schema.tables WHERE table_schema = 'public' AND table_type = 'BASE TABLE'";
        let results = self.execute_query(query, &[]).await?;

        let mut collections = Vec::new();
        for row in results {
            if let Some(table_name) = row.get("table_name") {
                if let Some(name) = table_name.as_str() {
                    collections.push(name.to_string());
                }
            }
        }

        Ok(collections)
    }
}

impl Clone for DatabaseManager {
    fn clone(&self) -> Self {
        Self {
            pool: self.pool.clone(),
            config: self.config.clone(),
        }
    }
}
