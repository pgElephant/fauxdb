/*!
 * @file aggregation.rs
 * @brief Aggregation pipeline support for FauxDB
 */

use crate::error::{FauxDBError, Result};
use bson::Document;
use serde::{Deserialize, Serialize};

pub struct AggregationEngine {
    // Aggregation pipeline stages
}

impl AggregationEngine {
    pub fn new() -> Self {
        Self {}
    }

    /// Test-friendly method that accepts input documents directly
    pub async fn process_pipeline_with_input(&self, input_docs: &[Document], pipeline: &[Document]) -> Result<Vec<Document>> {
        println!("📊 Processing aggregation pipeline with {} input documents", input_docs.len());
        
        let mut results = input_docs.to_vec();
        
        // Process each stage in the pipeline
        for (i, stage) in pipeline.iter().enumerate() {
            println!("🔧 Processing stage {}: {:?}", i, stage);
            
            if let Some(key) = stage.keys().next() {
                match key.as_str() {
                    "$match" => {
                        results = self.process_match_stage(&results, stage).await?;
                    }
                    "$group" => {
                        results = self.process_group_stage(&results, stage).await?;
                    }
                    "$sort" => {
                        results = self.process_sort_stage(&results, stage).await?;
                    }
                    "$limit" => {
                        results = self.process_limit_stage(&results, stage).await?;
                    }
                    "$skip" => {
                        results = self.process_skip_stage(&results, stage).await?;
                    }
                    "$project" => {
                        results = self.process_project_stage(&results, stage).await?;
                    }
                    "$count" => {
                        results = self.process_count_stage(&results, stage).await?;
                    }
                    _ => {
                        println!("⚠️ Unknown aggregation stage: {}", key);
                        // Continue processing with current results
                    }
                }
            }
        }
        
        println!("✅ Pipeline completed with {} result documents", results.len());
        Ok(results)
    }

    pub async fn process_pipeline(&self, collection: &str, pipeline: &[Document]) -> Result<Vec<Document>> {
        println!("📊 Processing aggregation pipeline for collection: {}", collection);
        
        let mut results = Vec::new();
        
        // Process each stage in the pipeline
        for (i, stage) in pipeline.iter().enumerate() {
            println!("🔧 Processing stage {}: {:?}", i, stage);
            
            if let Some(key) = stage.keys().next() {
                match key.as_str() {
                    "$match" => {
                        results = self.process_match_stage(&results, stage).await?;
                    }
                    "$group" => {
                        results = self.process_group_stage(&results, stage).await?;
                    }
                    "$sort" => {
                        results = self.process_sort_stage(&results, stage).await?;
                    }
                    "$limit" => {
                        results = self.process_limit_stage(&results, stage).await?;
                    }
                    "$skip" => {
                        results = self.process_skip_stage(&results, stage).await?;
                    }
                    "$project" => {
                        results = self.process_project_stage(&results, stage).await?;
                    }
                    "$count" => {
                        results = self.process_count_stage(&results, stage).await?;
                    }
                    "$unwind" => {
                        results = self.process_unwind_stage(&results, stage).await?;
                    }
                    "$lookup" => {
                        results = self.process_lookup_stage(&results, stage).await?;
                    }
                    _ => {
                        return Err(FauxDBError::Database(format!("Unknown aggregation stage: {:?}", stage)));
                    }
                }
            }
        }
        
        Ok(results)
    }

    async fn process_match_stage(&self, input: &[Document], stage: &Document) -> Result<Vec<Document>> {
        println!("🔍 Processing $match stage");
        
        let mut filtered_docs = Vec::new();
        
        // Get the match criteria from the stage
        if let Some(match_criteria) = stage.get("$match").and_then(|v| v.as_document()) {
            for doc in input {
                if self.document_matches_criteria(doc, match_criteria)? {
                    filtered_docs.push(doc.clone());
                }
            }
        } else {
            // If no match criteria, return all documents
            filtered_docs = input.to_vec();
        }
        
        println!("✅ $match stage filtered {} documents to {}", input.len(), filtered_docs.len());
        Ok(filtered_docs)
    }

    fn document_matches_criteria(&self, doc: &Document, criteria: &Document) -> Result<bool> {
        for (field, condition) in criteria {
            if let Some(field_value) = doc.get(field) {
                match condition.as_document() {
                    Some(condition_doc) => {
                        // Handle operators like $gt, $lt, $eq, etc.
                        if !self.evaluate_field_condition(field_value, condition_doc)? {
                            return Ok(false);
                        }
                    }
                    None => {
                        // Direct value comparison
                        if field_value != condition {
                            return Ok(false);
                        }
                    }
                }
            } else {
                // Field doesn't exist in document
                return Ok(false);
            }
        }
        Ok(true)
    }

    fn evaluate_field_condition(&self, field_value: &bson::Bson, condition: &Document) -> Result<bool> {
        for (operator, value) in condition {
            match operator.as_str() {
                "$eq" => {
                    if field_value != value {
                        return Ok(false);
                    }
                }
                "$ne" => {
                    if field_value == value {
                        return Ok(false);
                    }
                }
                "$gt" => {
                    if !self.compare_values(field_value, value, |a, b| a > b)? {
                        return Ok(false);
                    }
                }
                "$gte" => {
                    if !self.compare_values(field_value, value, |a, b| a >= b)? {
                        return Ok(false);
                    }
                }
                "$lt" => {
                    if !self.compare_values(field_value, value, |a, b| a < b)? {
                        return Ok(false);
                    }
                }
                "$lte" => {
                    if !self.compare_values(field_value, value, |a, b| a <= b)? {
                        return Ok(false);
                    }
                }
                "$in" => {
                    if let Some(values_array) = value.as_array() {
                        if !values_array.contains(field_value) {
                            return Ok(false);
                        }
                    }
                }
                "$nin" => {
                    if let Some(values_array) = value.as_array() {
                        if values_array.contains(field_value) {
                            return Ok(false);
                        }
                    }
                }
                _ => {
                    // Unknown operator, skip for now
                    continue;
                }
            }
        }
        Ok(true)
    }

    fn compare_values<F>(&self, a: &bson::Bson, b: &bson::Bson, compare_fn: F) -> Result<bool> 
    where
        F: FnOnce(f64, f64) -> bool,
    {
        let a_num = match a {
            bson::Bson::Int32(n) => *n as f64,
            bson::Bson::Int64(n) => *n as f64,
            bson::Bson::Double(n) => *n,
            _ => return Ok(false),
        };
        
        let b_num = match b {
            bson::Bson::Int32(n) => *n as f64,
            bson::Bson::Int64(n) => *n as f64,
            bson::Bson::Double(n) => *n,
            _ => return Ok(false),
        };
        
        Ok(compare_fn(a_num, b_num))
    }

    async fn process_group_stage(&self, input: &[Document], _stage: &Document) -> Result<Vec<Document>> {
        println!("👥 Processing $group stage");
        // Placeholder implementation
        Ok(input.to_vec())
    }

    async fn process_sort_stage(&self, input: &[Document], _stage: &Document) -> Result<Vec<Document>> {
        println!("📈 Processing $sort stage");
        // Placeholder implementation
        Ok(input.to_vec())
    }

    async fn process_limit_stage(&self, input: &[Document], stage: &Document) -> Result<Vec<Document>> {
        println!("📏 Processing $limit stage");
        
        let limit_value = stage.get("$limit")
            .and_then(|v| v.as_i32())
            .unwrap_or(input.len() as i32) as usize;
        
        let limited_docs: Vec<Document> = input.iter()
            .take(limit_value)
            .cloned()
            .collect();
        
        println!("✅ $limit stage limited {} documents to {}", input.len(), limited_docs.len());
        Ok(limited_docs)
    }

    async fn process_skip_stage(&self, input: &[Document], stage: &Document) -> Result<Vec<Document>> {
        println!("⏭️ Processing $skip stage");
        
        let skip_value = stage.get("$skip")
            .and_then(|v| v.as_i32())
            .unwrap_or(0) as usize;
        
        let skipped_docs: Vec<Document> = input.iter()
            .skip(skip_value)
            .cloned()
            .collect();
        
        println!("✅ $skip stage skipped {} documents, {} remaining", skip_value, skipped_docs.len());
        Ok(skipped_docs)
    }

    async fn process_project_stage(&self, input: &[Document], stage: &Document) -> Result<Vec<Document>> {
        println!("🎯 Processing $project stage");
        
        let mut projected_docs = Vec::new();
        
        // Get the projection specification
        if let Some(projection) = stage.get("$project").and_then(|v| v.as_document()) {
            for doc in input {
                let mut projected_doc = Document::new();
                
                for (field, include) in projection {
                    match include.as_i32() {
                        Some(1) => {
                            // Include field
                            if let Some(value) = doc.get(field) {
                                projected_doc.insert(field, value.clone());
                            }
                        }
                        Some(0) => {
                            // Exclude field (don't add it to projected_doc)
                            continue;
                        }
                        _ => {
                            // Field expression or other value
                            if let Some(value) = doc.get(field) {
                                projected_doc.insert(field, value.clone());
                            }
                        }
                    }
                }
                
                projected_docs.push(projected_doc);
            }
        } else {
            // If no projection specified, return all documents as-is
            projected_docs = input.to_vec();
        }
        
        println!("✅ $project stage processed {} documents", projected_docs.len());
        Ok(projected_docs)
    }

    async fn process_count_stage(&self, input: &[Document], stage: &Document) -> Result<Vec<Document>> {
        println!("🔢 Processing $count stage");
        let count = input.len() as i32;
        
        // Get the field name from the stage (e.g., {"$count": "total"})
        let field_name = if let Some(count_value) = stage.get("$count") {
            if let Some(name) = count_value.as_str() {
                name
            } else {
                "count"
            }
        } else {
            "count"
        };
        
        let result = bson::doc! {
            field_name: count
        };
        Ok(vec![result])
    }

    async fn process_unwind_stage(&self, input: &[Document], _stage: &Document) -> Result<Vec<Document>> {
        println!("🌀 Processing $unwind stage");
        // Placeholder implementation
        Ok(input.to_vec())
    }

    async fn process_lookup_stage(&self, input: &[Document], _stage: &Document) -> Result<Vec<Document>> {
        println!("🔗 Processing $lookup stage");
        // Placeholder implementation
        Ok(input.to_vec())
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AggregationStage {
    pub stage_type: String,
    pub parameters: Document,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AggregationResult {
    pub documents: Vec<Document>,
    pub cursor_id: Option<i64>,
    pub total_docs: i64,
    pub processing_time_ms: u64,
}
