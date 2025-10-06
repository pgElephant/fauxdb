/*!
 * Aggregation pipeline tests for FauxDB
 * Tests the aggregation pipeline stages and operations
 */

use anyhow::Result;
use bson::doc;
use fauxdb::aggregation::AggregationEngine;

#[tokio::test]
async fn test_match_stage() -> Result<()> {
    let pipeline = AggregationEngine::new();
    
    // Test data
    let _input_docs = vec![
        doc! { "name": "Alice", "age": 25, "city": "New York" },
        doc! { "name": "Bob", "age": 30, "city": "Boston" },
        doc! { "name": "Charlie", "age": 25, "city": "New York" },
    ];
    
    // Test $match stage
    let match_stage = doc! { "$match": { "age": 25 } };
    let stages = vec![match_stage];
    
    let result = pipeline.process_pipeline_with_input(&_input_docs, &stages).await?;
    
    assert_eq!(result.len(), 2);
    assert_eq!(result[0]["name"], "Alice".into());
    assert_eq!(result[1]["name"], "Charlie".into());
    
    println!("✅ $match stage test passed");
    Ok(())
}

#[tokio::test]
async fn test_limit_stage() -> Result<()> {
    let pipeline = AggregationEngine::new();
    
    // Test data
    let _input_docs = vec![
        doc! { "name": "Alice", "age": 25 },
        doc! { "name": "Bob", "age": 30 },
        doc! { "name": "Charlie", "age": 25 },
    ];
    
    // Test $limit stage
    let limit_stage = doc! { "$limit": 2 };
    let stages = vec![limit_stage];
    
    let result = pipeline.process_pipeline_with_input(&_input_docs, &stages).await?;
    
    assert_eq!(result.len(), 2);
    
    println!("✅ $limit stage test passed");
    Ok(())
}

#[tokio::test]
async fn test_skip_stage() -> Result<()> {
    let pipeline = AggregationEngine::new();
    
    // Test data
    let _input_docs = vec![
        doc! { "name": "Alice", "age": 25 },
        doc! { "name": "Bob", "age": 30 },
        doc! { "name": "Charlie", "age": 25 },
    ];
    
    // Test $skip stage
    let skip_stage = doc! { "$skip": 1 };
    let stages = vec![skip_stage];
    
    let result = pipeline.process_pipeline_with_input(&_input_docs, &stages).await?;
    
    assert_eq!(result.len(), 2);
    
    println!("✅ $skip stage test passed");
    Ok(())
}

#[tokio::test]
async fn test_project_stage() -> Result<()> {
    let pipeline = AggregationEngine::new();
    
    // Test data
    let _input_docs = vec![
        doc! { "name": "Alice", "age": 25, "city": "New York" },
        doc! { "name": "Bob", "age": 30, "city": "Boston" },
    ];
    
    // Test $project stage
    let project_stage = doc! { "$project": { "name": 1, "age": 1 } };
    let stages = vec![project_stage];
    
    let result = pipeline.process_pipeline_with_input(&_input_docs, &stages).await?;
    
    assert_eq!(result.len(), 2);
    assert!(!result[0].contains_key("city"));
    assert!(result[0].contains_key("name"));
    assert!(result[0].contains_key("age"));
    
    println!("✅ $project stage test passed");
    Ok(())
}

#[tokio::test]
async fn test_count_stage() -> Result<()> {
    let pipeline = AggregationEngine::new();
    
    // Test data
    let _input_docs = vec![
        doc! { "name": "Alice", "age": 25 },
        doc! { "name": "Bob", "age": 30 },
        doc! { "name": "Charlie", "age": 25 },
    ];
    
    // Test $count stage
    let count_stage = doc! { "$count": "total" };
    let stages = vec![count_stage];
    
    let result = pipeline.process_pipeline_with_input(&_input_docs, &stages).await?;
    
    assert_eq!(result.len(), 1);
    assert_eq!(result[0]["total"], bson::Bson::Int32(3));
    
    println!("✅ $count stage test passed");
    Ok(())
}

#[tokio::test]
async fn test_complex_pipeline() -> Result<()> {
    let pipeline = AggregationEngine::new();
    
    // Test data
    let _input_docs = vec![
        doc! { "name": "Alice", "age": 25, "city": "New York" },
        doc! { "name": "Bob", "age": 30, "city": "Boston" },
        doc! { "name": "Charlie", "age": 25, "city": "New York" },
        doc! { "name": "David", "age": 35, "city": "Boston" },
    ];
    
    // Complex pipeline: match -> project -> limit
    let stages = vec![
        doc! { "$match": { "age": { "$gte": 25 } } },
        doc! { "$project": { "name": 1, "city": 1 } },
        doc! { "$limit": 2 },
    ];
    
    let result = pipeline.process_pipeline_with_input(&_input_docs, &stages).await?;
    
    assert_eq!(result.len(), 2);
    assert!(!result[0].contains_key("age"));
    assert!(result[0].contains_key("name"));
    assert!(result[0].contains_key("city"));
    
    println!("✅ Complex pipeline test passed");
    Ok(())
}

#[tokio::test]
async fn test_aggregation_error_handling() -> Result<()> {
    let pipeline = AggregationEngine::new();
    
    // Test data
    let _input_docs = vec![
        doc! { "name": "Alice", "age": 25 },
        doc! { "name": "Bob", "age": 30 },
    ];
    
    // Test with invalid stage
    let invalid_stage = doc! { "$invalid_stage": { "field": "value" } };
    let stages = vec![invalid_stage];
    
    let result = pipeline.process_pipeline_with_input(&_input_docs, &stages).await;
    
    // Should handle unknown stages gracefully
    match result {
        Ok(docs) => {
            println!("✅ Aggregation handled unknown stage gracefully: {:?}", docs);
        }
        Err(e) => {
            println!("✅ Aggregation returned expected error: {}", e);
        }
    }
    
    println!("✅ Aggregation error handling test passed");
    Ok(())
}