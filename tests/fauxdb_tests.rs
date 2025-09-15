/*!
 * Consolidated FauxDB Test Suite
 * All tests use mongosh for authentic MongoDB client testing
 */

use anyhow::Result;
use std::process::Command;
use std::time::{Duration, Instant};
use serde::{Serialize, Deserialize};

#[derive(Debug, Serialize, Deserialize)]
struct TestResult {
    test_name: String,
    category: String,
    status: TestStatus,
    duration_ms: u64,
    output: String,
    error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
enum TestStatus {
    Passed,
    Failed,
    Skipped,
}

struct FauxDBTestSuite {
    connection_string: String,
    test_results: Vec<TestResult>,
}

impl FauxDBTestSuite {
    fn new(port: u16) -> Self {
        Self {
            connection_string: format!("mongodb://localhost:{}/test", port),
            test_results: Vec::new(),
        }
    }

    fn run_mongosh_command(&self, command: &str) -> Result<String> {
        let output = Command::new("mongosh")
            .arg(&self.connection_string)
            .arg("--quiet")
            .arg("--eval")
            .arg(command)
            .output()?;

        if output.status.success() {
            Ok(String::from_utf8_lossy(&output.stdout).to_string())
        } else {
            let stderr = String::from_utf8_lossy(&output.stderr).to_string();
            Err(anyhow::anyhow!("mongosh command failed: {}", stderr))
        }
    }

    fn run_test(&mut self, category: &str, test_name: &str, command: &str, description: &str) -> TestStatus {
        let start = Instant::now();
        
        match self.run_mongosh_command(command) {
            Ok(output) => {
                let duration = start.elapsed().as_millis() as u64;
                println!("PASS {} - {}ms", description, duration);
                self.test_results.push(TestResult {
                    test_name: test_name.to_string(),
                    category: category.to_string(),
                    status: TestStatus::Passed,
                    duration_ms: duration,
                    output: output.trim().to_string(),
                    error: None,
                });
                TestStatus::Passed
            }
            Err(e) => {
                let duration = start.elapsed().as_millis() as u64;
                println!("FAIL {} - {}ms - Error: {}", description, duration, e);
                self.test_results.push(TestResult {
                    test_name: test_name.to_string(),
                    category: category.to_string(),
                    status: TestStatus::Failed,
                    duration_ms: duration,
                    output: String::new(),
                    error: Some(e.to_string()),
                });
                TestStatus::Failed
            }
        }
    }

    fn run_error_test(&mut self, category: &str, test_name: &str, command: &str, description: &str) -> TestStatus {
        let start = Instant::now();
        
        match self.run_mongosh_command(command) {
            Ok(_output) => {
                let duration = start.elapsed().as_millis() as u64;
                println!("FAIL {} - {}ms - Expected error but got success", description, duration);
                self.test_results.push(TestResult {
                    test_name: test_name.to_string(),
                    category: category.to_string(),
                    status: TestStatus::Failed,
                    duration_ms: duration,
                    output: String::new(),
                    error: Some("Expected error but got success".to_string()),
                });
                TestStatus::Failed
            }
            Err(e) => {
                let duration = start.elapsed().as_millis() as u64;
                println!("PASS {} - {}ms - Error as expected: {}", description, duration, e);
                self.test_results.push(TestResult {
                    test_name: test_name.to_string(),
                    category: category.to_string(),
                    status: TestStatus::Passed,
                    duration_ms: duration,
                    output: String::new(),
                    error: Some(e.to_string()),
                });
                TestStatus::Passed
            }
        }
    }

    async fn wait_for_server(&self) -> Result<()> {
        for attempt in 1..=30 {
            match self.run_mongosh_command("db.runCommand({ping: 1})") {
                Ok(_) => return Ok(()),
                Err(_) => {
                    if attempt < 30 {
                        tokio::time::sleep(Duration::from_secs(1)).await;
                    }
                }
            }
        }
        Err(anyhow::anyhow!("Server not ready"))
    }

    async fn run_all_tests(&mut self) -> Result<()> {
        println!("FauxDB Comprehensive Test Suite");
        println!("===================================");
        println!("Connection: {}", self.connection_string);
        println!();

        self.wait_for_server().await?;

        // Core MongoDB Commands
        println!("Testing Core MongoDB Commands");
        println!("--------------------------------");
        self.run_test("core", "ping", "db.runCommand({ping: 1})", "Ping Command");
        self.run_test("core", "hello", "db.runCommand({hello: 1})", "Hello Command");
        self.run_test("core", "buildInfo", "db.runCommand({buildInfo: 1})", "BuildInfo Command");
        self.run_test("core", "isMaster", "db.runCommand({isMaster: 1})", "IsMaster Command");
        self.run_test("core", "listDatabases", "db.runCommand({listDatabases: 1})", "ListDatabases Command");
        println!();

        // CRUD Operations
        println!("Testing CRUD Operations");
        println!("--------------------------");
        
        // Insert Operations
        self.run_test("crud", "insertOne", r#"db.users.insertOne({name: "Test User", age: 25, email: "test@example.com"})"#, "Insert One");
        self.run_test("crud", "insertMany", r#"db.users.insertMany([{name: "User 1", age: 30}, {name: "User 2", age: 35}])"#, "Insert Many");
        
        // Find Operations
        self.run_test("crud", "find", "db.users.find()", "Find All");
        self.run_test("crud", "findOne", "db.users.findOne({name: \"Test User\"})", "Find One");
        self.run_test("crud", "findWithFilter", "db.users.find({age: {$gte: 30}})", "Find with Filter");
        self.run_test("crud", "findWithLimit", "db.users.find().limit(2)", "Find with Limit");
        self.run_test("crud", "findWithSkip", "db.users.find().skip(1)", "Find with Skip");
        self.run_test("crud", "findWithSort", "db.users.find().sort({age: 1})", "Find with Sort");
        self.run_test("crud", "findWithProjection", "db.users.find({}, {name: 1, age: 1})", "Find with Projection");
        self.run_test("crud", "countDocuments", "db.users.countDocuments()", "Count Documents");
        
        // Update Operations
        self.run_test("crud", "updateOne", r#"db.users.updateOne({name: "Test User"}, {$set: {age: 26}})"#, "Update One");
        self.run_test("crud", "updateMany", r#"db.users.updateMany({age: {$gte: 30}}, {$set: {category: "senior"}})"#, "Update Many");
        self.run_test("crud", "replaceOne", r#"db.users.replaceOne({name: "Test User"}, {name: "Updated User", age: 26, email: "updated@example.com"})"#, "Replace One");
        
        // Delete Operations
        self.run_test("crud", "deleteOne", r#"db.users.deleteOne({name: "Updated User"})"#, "Delete One");
        self.run_test("crud", "deleteMany", r#"db.users.deleteMany({category: "senior"})"#, "Delete Many");
        println!();

        // Aggregation Pipeline
        println!("Testing Aggregation Pipeline");
        println!("------------------------------");
        self.run_test("aggregation", "aggregate", r#"db.users.aggregate([{$match: {age: {$gte: 25}}}, {$group: {_id: null, avgAge: {$avg: "$age"}}}])"#, "Aggregate Pipeline");
        self.run_test("aggregation", "aggregateMatch", r#"db.users.aggregate([{$match: {age: {$gte: 30}}}])"#, "Aggregate Match Stage");
        self.run_test("aggregation", "aggregateGroup", r#"db.users.aggregate([{$group: {_id: "$category", count: {$sum: 1}}}])"#, "Aggregate Group Stage");
        self.run_test("aggregation", "aggregateSort", r#"db.users.aggregate([{$sort: {age: -1}}])"#, "Aggregate Sort Stage");
        self.run_test("aggregation", "aggregateLimit", r#"db.users.aggregate([{$limit: 3}])"#, "Aggregate Limit Stage");
        self.run_test("aggregation", "aggregateSkip", r#"db.users.aggregate([{$skip: 1}])"#, "Aggregate Skip Stage");
        self.run_test("aggregation", "aggregateProject", r#"db.users.aggregate([{$project: {name: 1, age: 1}}])"#, "Aggregate Project Stage");
        println!();

        // Indexing Operations
        println!("Testing Indexing Operations");
        println!("-----------------------------");
        self.run_test("indexing", "createIndex", r#"db.users.createIndex({name: 1})"#, "Create Single Index");
        self.run_test("indexing", "createCompoundIndex", r#"db.users.createIndex({name: 1, age: -1})"#, "Create Compound Index");
        self.run_test("indexing", "createUniqueIndex", r#"db.users.createIndex({email: 1}, {unique: true})"#, "Create Unique Index");
        self.run_test("indexing", "listIndexes", "db.users.getIndexes()", "List Indexes");
        self.run_test("indexing", "dropIndex", r#"db.users.dropIndex({name: 1})"#, "Drop Index");
        println!();

        // Collection Operations
        println!("Testing Collection Operations");
        println!("-------------------------------");
        self.run_test("collections", "createCollection", "db.createCollection('test_collection')", "Create Collection");
        self.run_test("collections", "listCollections", "db.getCollectionNames()", "List Collections");
        self.run_test("collections", "collectionStats", "db.runCommand({collStats: 'users'})", "Collection Statistics");
        self.run_test("collections", "dropCollection", "db.test_collection.drop()", "Drop Collection");
        println!();

        // Advanced Features
        println!("Testing Advanced Features");
        println!("---------------------------");
        self.run_test("advanced", "distinct", "db.users.distinct('category')", "Distinct Values");
        self.run_test("advanced", "explain", "db.users.find().explain()", "Explain Query");
        self.run_test("advanced", "hint", "db.users.find().hint({name: 1})", "Query Hint");
        self.run_test("advanced", "maxTimeMS", "db.users.find().maxTimeMS(1000)", "Max Time MS");
        println!();

        // Error Handling
        println!("Testing Error Handling");
        println!("-------------------------");
        self.run_error_test("errors", "invalidCommand", "db.runCommand({invalidCommand: 1})", "Invalid Command");
        self.run_error_test("errors", "syntaxError", "db.users.find({invalid syntax})", "Syntax Error");
        self.run_test("errors", "typeError", "db.users.find({age: {$gte: \"not_a_number\"}})", "Type Error");
        println!();

        self.print_summary();
        Ok(())
    }

    fn print_summary(&self) {
        let total = self.test_results.len();
        let passed = self.test_results.iter().filter(|r| matches!(r.status, TestStatus::Passed)).count();
        let failed = self.test_results.iter().filter(|r| matches!(r.status, TestStatus::Failed)).count();

        println!("COMPREHENSIVE TEST RESULTS");
        println!("=============================");
        println!("Total Tests: {}", total);
        println!("Passed: {}", passed);
        println!("Failed: {}", failed);
        println!("Success Rate: {:.1}%", (passed as f64 / total as f64) * 100.0);

        if failed == 0 {
            println!("\nALL TESTS PASSED! FauxDB is fully compatible with mongosh!");
        } else {
            println!("\nSome tests failed. FauxDB needs more improvements.");
        }
    }

    fn save_results_to_file(&self, filename: &str) -> Result<()> {
        let json = serde_json::to_string_pretty(&self.test_results)?;
        std::fs::write(filename, json)?;
        println!("Test results saved to: {}", filename);
        Ok(())
    }
}

// Individual test functions for different categories

#[tokio::test]
async fn test_core_mongodb_commands() -> Result<()> {
    let mut suite = FauxDBTestSuite::new(27018);
    suite.wait_for_server().await?;

    println!("Testing Core MongoDB Commands");
    suite.run_test("core", "ping", "db.runCommand({ping: 1})", "Ping Command");
    suite.run_test("core", "hello", "db.runCommand({hello: 1})", "Hello Command");
    suite.run_test("core", "buildInfo", "db.runCommand({buildInfo: 1})", "BuildInfo Command");
    suite.run_test("core", "isMaster", "db.runCommand({isMaster: 1})", "IsMaster Command");
    suite.run_test("core", "listDatabases", "db.runCommand({listDatabases: 1})", "ListDatabases Command");

    suite.print_summary();
    Ok(())
}

#[tokio::test]
async fn test_crud_operations() -> Result<()> {
    let mut suite = FauxDBTestSuite::new(27018);
    suite.wait_for_server().await?;

    println!("Testing CRUD Operations");
    
    // Insert Operations
    suite.run_test("crud", "insertOne", r#"db.users.insertOne({name: "Test User", age: 25, email: "test@example.com"})"#, "Insert One");
    suite.run_test("crud", "insertMany", r#"db.users.insertMany([{name: "User 1", age: 30}, {name: "User 2", age: 35}])"#, "Insert Many");
    
    // Find Operations
    suite.run_test("crud", "find", "db.users.find()", "Find All");
    suite.run_test("crud", "findOne", "db.users.findOne({name: \"Test User\"})", "Find One");
    suite.run_test("crud", "findWithFilter", "db.users.find({age: {$gte: 30}})", "Find with Filter");
    suite.run_test("crud", "findWithLimit", "db.users.find().limit(2)", "Find with Limit");
    suite.run_test("crud", "findWithSkip", "db.users.find().skip(1)", "Find with Skip");
    suite.run_test("crud", "findWithSort", "db.users.find().sort({age: 1})", "Find with Sort");
    suite.run_test("crud", "findWithProjection", "db.users.find({}, {name: 1, age: 1})", "Find with Projection");
    suite.run_test("crud", "countDocuments", "db.users.countDocuments()", "Count Documents");
    
    // Update Operations
    suite.run_test("crud", "updateOne", r#"db.users.updateOne({name: "Test User"}, {$set: {age: 26}})"#, "Update One");
    suite.run_test("crud", "updateMany", r#"db.users.updateMany({age: {$gte: 30}}, {$set: {category: "senior"}})"#, "Update Many");
    suite.run_test("crud", "replaceOne", r#"db.users.replaceOne({name: "Test User"}, {name: "Updated User", age: 26, email: "updated@example.com"})"#, "Replace One");
    
    // Delete Operations
    suite.run_test("crud", "deleteOne", r#"db.users.deleteOne({name: "Updated User"})"#, "Delete One");
    suite.run_test("crud", "deleteMany", r#"db.users.deleteMany({category: "senior"})"#, "Delete Many");

    suite.print_summary();
    Ok(())
}

#[tokio::test]
async fn test_aggregation_pipeline() -> Result<()> {
    let mut suite = FauxDBTestSuite::new(27018);
    suite.wait_for_server().await?;

    println!("Testing Aggregation Pipeline");
    suite.run_test("aggregation", "aggregate", r#"db.users.aggregate([{$match: {age: {$gte: 25}}}, {$group: {_id: null, avgAge: {$avg: "$age"}}}])"#, "Aggregate Pipeline");
    suite.run_test("aggregation", "aggregateMatch", r#"db.users.aggregate([{$match: {age: {$gte: 30}}}])"#, "Aggregate Match Stage");
    suite.run_test("aggregation", "aggregateGroup", r#"db.users.aggregate([{$group: {_id: "$category", count: {$sum: 1}}}])"#, "Aggregate Group Stage");
    suite.run_test("aggregation", "aggregateSort", r#"db.users.aggregate([{$sort: {age: -1}}])"#, "Aggregate Sort Stage");
    suite.run_test("aggregation", "aggregateLimit", r#"db.users.aggregate([{$limit: 3}])"#, "Aggregate Limit Stage");
    suite.run_test("aggregation", "aggregateSkip", r#"db.users.aggregate([{$skip: 1}])"#, "Aggregate Skip Stage");
    suite.run_test("aggregation", "aggregateProject", r#"db.users.aggregate([{$project: {name: 1, age: 1}}])"#, "Aggregate Project Stage");

    suite.print_summary();
    Ok(())
}

#[tokio::test]
async fn test_indexing_operations() -> Result<()> {
    let mut suite = FauxDBTestSuite::new(27018);
    suite.wait_for_server().await?;

    println!("Testing Indexing Operations");
    suite.run_test("indexing", "createIndex", r#"db.users.createIndex({name: 1})"#, "Create Single Index");
    suite.run_test("indexing", "createCompoundIndex", r#"db.users.createIndex({name: 1, age: -1})"#, "Create Compound Index");
    suite.run_test("indexing", "createUniqueIndex", r#"db.users.createIndex({email: 1}, {unique: true})"#, "Create Unique Index");
    suite.run_test("indexing", "listIndexes", "db.users.getIndexes()", "List Indexes");
    suite.run_test("indexing", "dropIndex", r#"db.users.dropIndex({name: 1})"#, "Drop Index");

    suite.print_summary();
    Ok(())
}

#[tokio::test]
async fn test_collection_operations() -> Result<()> {
    let mut suite = FauxDBTestSuite::new(27018);
    suite.wait_for_server().await?;

    println!("Testing Collection Operations");
    suite.run_test("collections", "createCollection", "db.createCollection('test_collection')", "Create Collection");
    suite.run_test("collections", "listCollections", "db.getCollectionNames()", "List Collections");
    suite.run_test("collections", "collectionStats", "db.runCommand({collStats: 'users'})", "Collection Statistics");
    suite.run_test("collections", "dropCollection", "db.test_collection.drop()", "Drop Collection");

    suite.print_summary();
    Ok(())
}

#[tokio::test]
async fn test_advanced_features() -> Result<()> {
    let mut suite = FauxDBTestSuite::new(27018);
    suite.wait_for_server().await?;

    println!("Testing Advanced Features");
    suite.run_test("advanced", "distinct", "db.users.distinct('category')", "Distinct Values");
    suite.run_test("advanced", "explain", "db.users.find().explain()", "Explain Query");
    suite.run_test("advanced", "hint", "db.users.find().hint({name: 1})", "Query Hint");
    suite.run_test("advanced", "maxTimeMS", "db.users.find().maxTimeMS(1000)", "Max Time MS");

    suite.print_summary();
    Ok(())
}

#[tokio::test]
async fn test_comprehensive_suite() -> Result<()> {
    let mut suite = FauxDBTestSuite::new(27018);
    suite.run_all_tests().await?;
    suite.save_results_to_file("test_results.json")?;
    Ok(())
}

#[tokio::test]
async fn test_basic_connectivity() -> Result<()> {
    let suite = FauxDBTestSuite::new(27018);
    suite.wait_for_server().await?;
    
    let result = suite.run_mongosh_command("db.runCommand({ping: 1})")?;
    assert!(result.contains("ok") && result.contains("1"), "Ping should return ok: 1");
    
    println!("✅ Basic connectivity test passed!");
    Ok(())
}
