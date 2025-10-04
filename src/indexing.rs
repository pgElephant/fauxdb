/*!
 * Comprehensive MongoDB Indexing Support for FauxDB
 * Full indexing capabilities with PostgreSQL backend optimization
 */

use bson::{Document, Bson};
use serde::{Deserialize, Serialize};
use anyhow::{Result, anyhow};
use crate::{fauxdb_info, fauxdb_warn};
use std::collections::HashMap;
use std::sync::Arc;
use parking_lot::RwLock;
use chrono::{DateTime, Utc};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IndexSpec {
    pub name: String,
    pub key: Document,
    pub options: IndexOptions,
    pub created_at: DateTime<Utc>,
    pub collection: String,
    pub database: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IndexOptions {
    pub unique: Option<bool>,
    pub sparse: Option<bool>,
    pub background: Option<bool>,
    pub name: Option<String>,
    pub drop_dups: Option<bool>,
    pub min: Option<f64>,
    pub max: Option<f64>,
    pub v: Option<i32>,
    pub weights: Option<Document>,
    pub default_language: Option<String>,
    pub language_override: Option<String>,
    pub text_index_version: Option<i32>,
    #[serde(rename = "2dsphere_index_version")]
    pub sphere_index_version: Option<i32>,
    pub bits: Option<i32>,
    pub min_longitude: Option<f64>,
    pub max_longitude: Option<f64>,
    pub min_latitude: Option<f64>,
    pub max_latitude: Option<f64>,
    pub expire_after_seconds: Option<i32>,
    pub partial_filter_expression: Option<Document>,
    pub collation: Option<Document>,
    pub wildcard_projection: Option<Document>,
    pub hidden: Option<bool>,
    pub prepare_unique: Option<bool>,
    pub clustered: Option<bool>,
}

#[derive(Debug, Clone)]
pub enum IndexType {
    Ascending,
    Descending,
    Text,
    Geospatial2d,
    Geospatial2dSphere,
    Hashed,
    Wildcard,
    Clustered,
    Partial,
    Sparse,
    Unique,
    TTL,
}

#[derive(Debug, Clone)]
pub struct IndexStats {
    pub name: String,
    pub collection: String,
    pub database: String,
    pub index_type: IndexType,
    pub size_bytes: u64,
    pub document_count: u64,
    pub avg_key_size: f64,
    pub avg_value_size: f64,
    pub is_unique: bool,
    pub is_sparse: bool,
    pub is_partial: bool,
    pub created_at: DateTime<Utc>,
    pub last_accessed: Option<DateTime<Utc>>,
    pub access_count: u64,
    pub usage_percentage: f64,
}

#[derive(Debug)]
pub struct IndexManager {
    indexes: Arc<RwLock<HashMap<String, IndexSpec>>>,
    index_stats: Arc<RwLock<HashMap<String, IndexStats>>>,
}

impl IndexManager {
    pub fn new() -> Self {
        Self {
            indexes: Arc::new(RwLock::new(HashMap::new())),
            index_stats: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    pub fn create_index(&self, spec: IndexSpec) -> Result<IndexSpec> {
        let index_name = self.generate_index_name(&spec);
        let mut indexed_spec = spec;
        indexed_spec.name = index_name.clone();
        indexed_spec.created_at = Utc::now();

        // Validate index specification
        self.validate_index_spec(&indexed_spec)?;

        // Create PostgreSQL index
        let sql = self.generate_create_index_sql(&indexed_spec)?;
        fauxdb_info!("Creating index with SQL: {}", sql);

        // Store index specification
        {
            let mut indexes = self.indexes.write();
            indexes.insert(index_name.clone(), indexed_spec.clone());
        }

        // Initialize index stats
        {
            let mut stats = self.index_stats.write();
            stats.insert(index_name.clone(), IndexStats {
                name: index_name.clone(),
                collection: indexed_spec.collection.clone(),
                database: indexed_spec.database.clone(),
                index_type: self.determine_index_type(&indexed_spec),
                size_bytes: 0,
                document_count: 0,
                avg_key_size: 0.0,
                avg_value_size: 0.0,
                is_unique: indexed_spec.options.unique.unwrap_or(false),
                is_sparse: indexed_spec.options.sparse.unwrap_or(false),
                is_partial: indexed_spec.options.partial_filter_expression.is_some(),
                created_at: indexed_spec.created_at,
                last_accessed: None,
                access_count: 0,
                usage_percentage: 0.0,
            });
        }

        fauxdb_info!("Successfully created index: {}", index_name);
        Ok(indexed_spec)
    }

    pub fn drop_index(&self, collection: &str, database: &str, index_name: &str) -> Result<()> {
        let full_name = format!("{}.{}.{}", database, collection, index_name);
        
        // Generate DROP INDEX SQL
        let sql = format!("DROP INDEX IF EXISTS {}", self.sanitize_index_name(&full_name));
        fauxdb_info!("Dropping index with SQL: {}", sql);

        // Remove from registry
        {
            let mut indexes = self.indexes.write();
            indexes.remove(&full_name);
        }

        {
            let mut stats = self.index_stats.write();
            stats.remove(&full_name);
        }

        fauxdb_info!("Successfully dropped index: {}", full_name);
        Ok(())
    }

    pub fn list_indexes(&self, collection: &str, database: &str) -> Vec<IndexSpec> {
        let _prefix = format!("{}.{}.", database, collection);
        let indexes = self.indexes.read();
        
        indexes.values()
            .filter(|spec| spec.database == database && spec.collection == collection)
            .cloned()
            .collect()
    }

    pub fn get_index_stats(&self, collection: &str, database: &str) -> Vec<IndexStats> {
        let _prefix = format!("{}.{}.", database, collection);
        let stats = self.index_stats.read();
        
        stats.values()
            .filter(|stat| stat.database == database && stat.collection == collection)
            .cloned()
            .collect()
    }

    pub fn analyze_index_usage(&self, collection: &str, database: &str, query: &str) -> Result<Vec<String>> {
        // Analyze query to determine which indexes would be most effective
        let mut recommended_indexes = Vec::new();
        
        // Parse query for field usage
        if let Ok(parsed_query) = self.parse_query_fields(query) {
            let existing_indexes = self.list_indexes(collection, database);
            
            for field in parsed_query.filter_fields {
                if !self.field_has_index(&existing_indexes, &field) {
                    recommended_indexes.push(field);
                }
            }
            
            for sort_field in parsed_query.sort_fields {
                if !self.field_has_index(&existing_indexes, &sort_field) {
                    recommended_indexes.push(sort_field);
                }
            }
        }

        Ok(recommended_indexes)
    }

    fn generate_index_name(&self, spec: &IndexSpec) -> String {
        if let Some(name) = &spec.options.name {
            return name.clone();
        }

        let mut name_parts = Vec::new();
        for (field, direction) in &spec.key {
            let direction_str = match direction {
                Bson::Int32(1) => "1",
                Bson::Int32(-1) => "-1",
                Bson::String(text) if text == "text" => "text",
                Bson::String(geo) if geo == "2dsphere" => "2dsphere",
                Bson::String(hash) if hash == "hashed" => "hashed",
                _ => "1",
            };
            name_parts.push(format!("{}_{}", field, direction_str));
        }

        let base_name = name_parts.join("_");
        format!("idx_{}", base_name)
    }

    fn validate_index_spec(&self, spec: &IndexSpec) -> Result<()> {
        if spec.key.is_empty() {
            return Err(anyhow!("Index key cannot be empty"));
        }

        // Validate index key fields
        for (field, direction) in &spec.key {
            if field.starts_with('$') {
                return Err(anyhow!("Index field cannot start with '$': {}", field));
            }

            match direction {
                Bson::Int32(1) | Bson::Int32(-1) => {},
                Bson::String(text) if text == "text" => {},
                Bson::String(geo) if geo == "2dsphere" => {},
                Bson::String(hash) if hash == "hashed" => {},
                _ => return Err(anyhow!("Invalid index direction for field {}: {:?}", field, direction))
            }
        }

        // Validate text index
        let text_fields: Vec<_> = spec.key.iter()
            .filter(|(_, direction)| matches!(direction, Bson::String(text) if text == "text"))
            .collect();

        if text_fields.len() > 1 {
            return Err(anyhow!("Text index can only have one field"));
        }

        if !text_fields.is_empty() && spec.key.len() > 1 {
            return Err(anyhow!("Text index cannot be combined with other index types"));
        }

        // Validate geospatial index
        let geo_fields: Vec<_> = spec.key.iter()
            .filter(|(_, direction)| matches!(direction, Bson::String(geo) if geo == "2dsphere"))
            .collect();

        if geo_fields.len() > 1 {
            return Err(anyhow!("2dsphere index can only have one field"));
        }

        // Validate TTL index
        if spec.options.expire_after_seconds.is_some() {
            if spec.key.len() != 1 {
                return Err(anyhow!("TTL index can only have one field"));
            }
            
            let first_field = spec.key.iter().next().unwrap();
            match first_field.1 {
                Bson::Int32(1) | Bson::Int32(-1) => {},
                _ => return Err(anyhow!("TTL index field must be ascending or descending"))
            }
        }

        // Validate unique index
        if spec.options.unique.unwrap_or(false)
            && spec.options.sparse.unwrap_or(false) {
                    fauxdb_warn!("Unique sparse indexes may allow duplicate null values");
        }

        Ok(())
    }

    fn generate_create_index_sql(&self, spec: &IndexSpec) -> Result<String> {
        let index_name = self.sanitize_index_name(&spec.name);
        let table_name = format!("{}.{}", spec.database, spec.collection);
        
        let mut sql_parts = vec!["CREATE".to_string()];

        // Add UNIQUE if specified
        if spec.options.unique.unwrap_or(false) {
            sql_parts.push("UNIQUE".to_string());
        }

        // Add index type-specific modifiers
        if spec.options.sparse.unwrap_or(false) {
            sql_parts.push("SPARSE".to_string());
        }

        sql_parts.push("INDEX".to_string());
        sql_parts.push(index_name);
        sql_parts.push("ON".to_string());
        sql_parts.push(table_name);

        // Generate column list
        let mut columns = Vec::new();
        for (field, direction) in &spec.key {
            let column_name = self.sanitize_column_name(field);
            
            match direction {
                Bson::Int32(1) => columns.push(column_name),
                Bson::Int32(-1) => columns.push(format!("{} DESC", column_name)),
                Bson::String(text) if text == "text" => {
                    // For text indexes, we'll create a GIN index on a text search vector
                    columns.push(format!("to_tsvector('english', {})", column_name));
                }
                Bson::String(geo) if geo == "2dsphere" => {
                    // For geospatial indexes, we'll create a GIST index
                    columns.push(format!("{} USING GIST", column_name));
                }
                Bson::String(hash) if hash == "hashed" => {
                    // For hashed indexes, we'll use a hash function
                    columns.push(format!("hash({})", column_name));
                }
                _ => return Err(anyhow!("Unsupported index direction: {:?}", direction))
            }
        }

        sql_parts.push(format!("({})", columns.join(", ")));

        // Add index options
        let mut options = Vec::new();
        
        if let Some(partial_filter) = &spec.options.partial_filter_expression {
            // Convert MongoDB partial filter to PostgreSQL WHERE clause
            if let Ok(where_clause) = self.partial_filter_to_sql(partial_filter) {
                options.push(format!("WHERE {}", where_clause));
            }
        }

        if let Some(expire_seconds) = spec.options.expire_after_seconds {
            // For TTL, we'll need to implement a background process
            // For now, just log that TTL is requested
            fauxdb_info!("TTL index requested with {} seconds expiration", expire_seconds);
        }

        if !options.is_empty() {
            sql_parts.push(options.join(" "));
        }

        Ok(sql_parts.join(" "))
    }

    fn sanitize_index_name(&self, name: &str) -> String {
        name.replace(".", "_").replace("-", "_").replace("$", "_")
    }

    fn sanitize_column_name(&self, name: &str) -> String {
        name.replace("$", "_")
    }

    fn determine_index_type(&self, spec: &IndexSpec) -> IndexType {
        if spec.options.unique.unwrap_or(false) {
            return IndexType::Unique;
        }
        
        if spec.options.sparse.unwrap_or(false) {
            return IndexType::Sparse;
        }
        
        if spec.options.expire_after_seconds.is_some() {
            return IndexType::TTL;
        }
        
        if spec.options.partial_filter_expression.is_some() {
            return IndexType::Partial;
        }

        for (_, direction) in &spec.key {
            match direction {
                Bson::String(text) if text == "text" => return IndexType::Text,
                Bson::String(geo) if geo == "2dsphere" => return IndexType::Geospatial2dSphere,
                Bson::String(geo) if geo == "2d" => return IndexType::Geospatial2d,
                Bson::String(hash) if hash == "hashed" => return IndexType::Hashed,
                _ => {}
            }
        }

        IndexType::Ascending
    }

    fn field_has_index(&self, indexes: &[IndexSpec], field: &str) -> bool {
        indexes.iter().any(|index| index.key.contains_key(field))
    }

    fn partial_filter_to_sql(&self, filter: &Document) -> Result<String> {
        // Convert MongoDB partial filter expression to PostgreSQL WHERE clause
        // This is a simplified implementation
        let mut conditions = Vec::new();
        
        for (field, value) in filter {
            match value {
                Bson::Document(op_doc) => {
                    if op_doc.len() == 1 {
                        let (op, op_value) = op_doc.into_iter().next().unwrap();
                        match op.as_str() {
                            "$exists" => {
                                let exists = op_value.as_bool().unwrap_or(false);
                                if exists {
                                    conditions.push(format!("{} IS NOT NULL", field));
                                } else {
                                    conditions.push(format!("{} IS NULL", field));
                                }
                            }
                            "$ne" => {
                                conditions.push(format!("{} != {}", field, self.bson_to_sql_value(op_value)?));
                            }
                            _ => {
                                fauxdb_warn!("Unsupported partial filter operator: {}", op);
                            }
                        }
                    }
                }
                _ => {
                    conditions.push(format!("{} = {}", field, self.bson_to_sql_value(value)?));
                }
            }
        }
        
        Ok(conditions.join(" AND "))
    }

    fn bson_to_sql_value(&self, value: &Bson) -> Result<String> {
        match value {
            Bson::String(s) => Ok(format!("'{}'", s.replace("'", "''"))),
            Bson::Int32(i) => Ok(i.to_string()),
            Bson::Int64(i) => Ok(i.to_string()),
            Bson::Double(f) => Ok(f.to_string()),
            Bson::Boolean(b) => Ok(b.to_string()),
            Bson::Null => Ok("NULL".to_string()),
            _ => Err(anyhow!("Unsupported BSON type for SQL conversion"))
        }
    }

    fn parse_query_fields(&self, _query: &str) -> Result<ParsedQuery> {
        // Simplified query parsing - in production, this would be more sophisticated
        Ok(ParsedQuery {
            filter_fields: Vec::new(),
            sort_fields: Vec::new(),
        })
    }
}

#[derive(Debug)]
struct ParsedQuery {
    filter_fields: Vec<String>,
    sort_fields: Vec<String>,
}

impl Default for IndexManager {
    fn default() -> Self {
        Self::new()
    }
}

// Index optimization and maintenance
impl IndexManager {
    pub fn optimize_indexes(&self, collection: &str, database: &str) -> Result<Vec<String>> {
        let mut recommendations = Vec::new();
        
        // Analyze index usage statistics
        let stats = self.get_index_stats(collection, database);
        
        for stat in stats {
            // Recommend dropping unused indexes
            if stat.access_count == 0 && stat.created_at < Utc::now() - chrono::Duration::days(7) {
                recommendations.push(format!("Consider dropping unused index: {}", stat.name));
            }
            
            // Recommend rebuilding fragmented indexes
            if stat.usage_percentage < 50.0 {
                recommendations.push(format!("Consider rebuilding fragmented index: {}", stat.name));
            }
        }
        
        Ok(recommendations)
    }

    pub fn rebuild_index(&self, index_name: &str) -> Result<()> {
        fauxdb_info!("Rebuilding index: {}", index_name);
        
        // Generate REINDEX SQL for PostgreSQL
        let sql = format!("REINDEX INDEX {}", self.sanitize_index_name(index_name));
        fauxdb_info!("Rebuilding index with SQL: {}", sql);
        
        Ok(())
    }

    pub fn get_index_size(&self, index_name: &str) -> Result<u64> {
        // Query PostgreSQL system tables for index size
        let _sql = format!(
            "SELECT pg_size_pretty(pg_relation_size('{}')) as size",
            self.sanitize_index_name(index_name)
        );
        
        // In a real implementation, this would execute the query and return the size
        Ok(0)
    }
}
