/*!
 * @file benchmarks.rs
 * @brief Benchmarking utilities for FauxDB
 */

use crate::error::Result;
use std::time::{Duration, Instant};
use serde::{Serialize, Deserialize};

pub struct BenchmarkRunner {
    results: Vec<BenchmarkResult>,
}

impl BenchmarkRunner {
    pub fn new() -> Self {
        Self {
            results: Vec::new(),
        }
    }

    pub async fn run_benchmark<F, Fut>(&mut self, name: &str, benchmark_fn: F) -> Result<Duration>
    where
        F: FnOnce() -> Fut,
        Fut: std::future::Future<Output = Result<()>>,
    {
        println!("🏃 Running benchmark: {}", name);
        
        let start = Instant::now();
        benchmark_fn().await?;
        let duration = start.elapsed();
        
        let result = BenchmarkResult {
            name: name.to_string(),
            duration,
            timestamp: chrono::Utc::now(),
        };
        
        self.results.push(result);
        println!("✅ Benchmark '{}' completed in {:?}", name, duration);
        
        Ok(duration)
    }

    pub async fn run_insert_benchmark(&mut self, num_documents: usize) -> Result<Duration> {
        self.run_benchmark(&format!("insert_{}_documents", num_documents), || async {
            for i in 0..num_documents {
                let _doc = bson::doc! {
                    "_id": i as i32,
                    "name": format!("Document {}", i),
                    "value": i as f64,
                    "created_at": chrono::Utc::now().to_rfc3339(),
                };
                // Simulate document insertion
                tokio::time::sleep(Duration::from_millis(1)).await;
            }
            Ok(())
        }).await
    }

    pub async fn run_find_benchmark(&mut self, num_queries: usize) -> Result<Duration> {
        self.run_benchmark(&format!("find_{}_queries", num_queries), || async {
            for i in 0..num_queries {
                let _filter = bson::doc! {
                    "value": { "$gte": i as f64 }
                };
                // Simulate document finding
                tokio::time::sleep(Duration::from_millis(1)).await;
            }
            Ok(())
        }).await
    }

    pub async fn run_aggregation_benchmark(&mut self, num_pipelines: usize) -> Result<Duration> {
        self.run_benchmark(&format!("aggregation_{}_pipelines", num_pipelines), || async {
            for i in 0..num_pipelines {
                let _pipeline = vec![
                    bson::doc! { "$match": { "value": { "$gte": i as f64 } } },
                    bson::doc! { "$group": { "_id": null, "count": { "$sum": 1 } } },
                    bson::doc! { "$sort": { "count": -1 } },
                ];
                // Simulate aggregation pipeline execution
                tokio::time::sleep(Duration::from_millis(2)).await;
            }
            Ok(())
        }).await
    }

    pub async fn run_concurrent_benchmark(&mut self, num_tasks: usize, operations_per_task: usize) -> Result<Duration> {
        self.run_benchmark(&format!("concurrent_{}_tasks_{}_ops", num_tasks, operations_per_task), || async {
            let mut handles = Vec::new();
            
            for _task_id in 0..num_tasks {
                let handle = tokio::spawn(async move {
                    for _op_id in 0..operations_per_task {
                        // Simulate concurrent operation
                        tokio::time::sleep(Duration::from_millis(1)).await;
                    }
                });
                handles.push(handle);
            }
            
            for handle in handles {
                handle.await.unwrap();
            }
            
            Ok(())
        }).await
    }

    pub fn get_results(&self) -> &[BenchmarkResult] {
        &self.results
    }

    pub fn generate_report(&self) -> BenchmarkReport {
        let total_benchmarks = self.results.len();
        let total_duration: Duration = self.results.iter().map(|r| r.duration).sum();
        let avg_duration = if total_benchmarks > 0 {
            total_duration / total_benchmarks as u32
        } else {
            Duration::ZERO
        };

        BenchmarkReport {
            total_benchmarks,
            total_duration,
            average_duration: avg_duration,
            results: self.results.clone(),
            generated_at: chrono::Utc::now(),
        }
    }

    pub fn save_results_to_file(&self, filename: &str) -> Result<()> {
        let report = self.generate_report();
        let json = serde_json::to_string_pretty(&report)?;
        std::fs::write(filename, json)?;
        println!("📊 Benchmark results saved to: {}", filename);
        Ok(())
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BenchmarkResult {
    pub name: String,
    pub duration: Duration,
    pub timestamp: chrono::DateTime<chrono::Utc>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BenchmarkReport {
    pub total_benchmarks: usize,
    pub total_duration: Duration,
    pub average_duration: Duration,
    pub results: Vec<BenchmarkResult>,
    pub generated_at: chrono::DateTime<chrono::Utc>,
}

#[derive(Debug, Clone)]
pub struct PerformanceMetrics {
    pub operations_per_second: f64,
    pub average_latency_ms: f64,
    pub p95_latency_ms: f64,
    pub p99_latency_ms: f64,
    pub throughput_mbps: f64,
}

impl PerformanceMetrics {
    pub fn calculate(&self, results: &[BenchmarkResult]) -> Self {
        if results.is_empty() {
            return Self {
                operations_per_second: 0.0,
                average_latency_ms: 0.0,
                p95_latency_ms: 0.0,
                p99_latency_ms: 0.0,
                throughput_mbps: 0.0,
            };
        }

        let total_duration = results.iter().map(|r| r.duration).sum::<Duration>();
        let total_operations = results.len() as f64;
        let duration_seconds = total_duration.as_secs_f64();
        
        let operations_per_second = total_operations / duration_seconds.max(0.001);
        
        let mut latencies: Vec<f64> = results.iter()
            .map(|r| r.duration.as_secs_f64() * 1000.0)
            .collect();
        latencies.sort_by(|a, b| a.partial_cmp(b).unwrap());
        
        let average_latency_ms = latencies.iter().sum::<f64>() / latencies.len() as f64;
        
        let p95_index = (latencies.len() as f64 * 0.95) as usize;
        let p99_index = (latencies.len() as f64 * 0.99) as usize;
        
        let p95_latency_ms = latencies.get(p95_index.min(latencies.len() - 1)).copied().unwrap_or(0.0);
        let p99_latency_ms = latencies.get(p99_index.min(latencies.len() - 1)).copied().unwrap_or(0.0);
        
        // Estimate throughput (simplified)
        let throughput_mbps = (total_operations * 1024.0) / (duration_seconds * 1024.0 * 1024.0);
        
        Self {
            operations_per_second,
            average_latency_ms,
            p95_latency_ms,
            p99_latency_ms,
            throughput_mbps,
        }
    }
}
