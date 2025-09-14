/*!
 * Production-ready connection pool management for FauxDB
 */

use deadpool_postgres::{Config, Pool, PoolConfig, Runtime};
use tokio_postgres::{NoTls, Row};
use std::time::Duration;
use std::sync::Arc;
use parking_lot::RwLock;
use metrics::{counter, histogram, gauge};
use anyhow::Result;
use crate::fauxdb_info;

pub struct ProductionConnectionPool {
    pub pool: Pool,
    config: Arc<ProductionPoolConfig>,
    stats: Arc<PoolStats>,
}

#[derive(Debug, Clone)]
pub struct ProductionPoolConfig {
    pub max_size: u32,
    pub min_idle: u32,
    pub max_lifetime: Duration,
    pub idle_timeout: Duration,
    pub connection_timeout: Duration,
    pub statement_cache_size: usize,
}

#[derive(Debug, Default)]
pub struct PoolStats {
    pub total_connections: RwLock<u64>,
    pub active_connections: RwLock<u64>,
    pub idle_connections: RwLock<u64>,
    pub connection_errors: RwLock<u64>,
    pub query_count: RwLock<u64>,
    pub avg_query_time: RwLock<f64>,
}

impl Default for ProductionPoolConfig {
    fn default() -> Self {
        Self {
            max_size: 20,
            min_idle: 5,
            max_lifetime: Duration::from_secs(1800),
            idle_timeout: Duration::from_secs(600),
            connection_timeout: Duration::from_secs(10),
            statement_cache_size: 1000,
        }
    }
}

impl ProductionConnectionPool {
    pub async fn new(
        connection_string: &str,
        config: ProductionPoolConfig,
    ) -> Result<Self> {
        fauxdb_info!("Initializing production connection pool with config: {:?}", config);

        let mut pg_config = Config::new();
        pg_config.url = Some(connection_string.to_string());
        
        let pool_config = PoolConfig::new(config.max_size as usize);
        
        pg_config.pool = Some(pool_config);

        let pool = pg_config
            .create_pool(Some(Runtime::Tokio1), NoTls)
            .map_err(|e| anyhow::anyhow!("Failed to create connection pool: {}", e))?;

        // Test the connection
        let test_client = pool.get().await
            .map_err(|e| anyhow::anyhow!("Failed to get test connection: {}", e))?;
        
        test_client.execute("SELECT 1", &[]).await
            .map_err(|e| anyhow::anyhow!("Failed to execute test query: {}", e))?;

        fauxdb_info!("Connection pool initialized successfully with {} max connections", config.max_size);

        Ok(Self {
            pool,
            config: Arc::new(config),
            stats: Arc::new(PoolStats::default()),
        })
    }

    pub async fn get_connection(&self) -> Result<PooledConnection> {
        let start_time = std::time::Instant::now();
        
        let client = self.pool.get().await
            .map_err(|e| {
                *self.stats.connection_errors.write() += 1;
                counter!("fauxdb_pool_connection_errors_total").increment(1);
                anyhow::anyhow!("Failed to get connection from pool: {}", e)
            })?;

        let connection_time = start_time.duration_since(start_time);
        histogram!("fauxdb_pool_connection_time_seconds").record(connection_time.as_secs_f64());

        *self.stats.active_connections.write() += 1;
        gauge!("fauxdb_pool_active_connections").set(*self.stats.active_connections.read() as f64);

        Ok(PooledConnection {
            client,
            stats: Arc::clone(&self.stats),
        })
    }

    pub fn get_stats(&self) -> PoolStatsSnapshot {
        PoolStatsSnapshot {
            total_connections: *self.stats.total_connections.read(),
            active_connections: *self.stats.active_connections.read(),
            idle_connections: *self.stats.idle_connections.read(),
            connection_errors: *self.stats.connection_errors.read(),
            query_count: *self.stats.query_count.read(),
            avg_query_time: *self.stats.avg_query_time.read(),
            pool_size: self.config.max_size,
            pool_idle: self.config.min_idle,
        }
    }

    pub async fn health_check(&self) -> Result<()> {
        let mut client = self.get_connection().await?;
        client.execute("SELECT 1", &[]).await?;
        Ok(())
    }
}

pub struct PooledConnection {
    client: deadpool_postgres::Object,
    stats: Arc<PoolStats>,
}

impl PooledConnection {
    pub async fn execute(&mut self, query: &str, params: &[&(dyn tokio_postgres::types::ToSql + Sync)]) -> Result<u64> {
        let start_time = std::time::Instant::now();
        
        let result = self.client.execute(query, params).await
            .map_err(|e| anyhow::anyhow!("Query execution failed: {}", e))?;

        let query_time = start_time.duration_since(start_time);
        histogram!("fauxdb_query_duration_seconds").record(query_time.as_secs_f64());

        *self.stats.query_count.write() += 1;
        counter!("fauxdb_queries_total").increment(1);

        // Update average query time
        let mut avg_time = self.stats.avg_query_time.write();
        let count = *self.stats.query_count.read();
        *avg_time = (*avg_time * (count - 1) as f64 + query_time.as_secs_f64()) / count as f64;

        Ok(result)
    }

    pub async fn query(&mut self, query: &str, params: &[&(dyn tokio_postgres::types::ToSql + Sync)]) -> Result<Vec<Row>> {
        let start_time = std::time::Instant::now();
        
        let result = self.client.query(query, params).await
            .map_err(|e| anyhow::anyhow!("Query execution failed: {}", e))?;

        let query_time = start_time.duration_since(start_time);
        histogram!("fauxdb_query_duration_seconds").record(query_time.as_secs_f64());

        *self.stats.query_count.write() += 1;
        counter!("fauxdb_queries_total").increment(1);

        // Update average query time
        let mut avg_time = self.stats.avg_query_time.write();
        let count = *self.stats.query_count.read();
        *avg_time = (*avg_time * (count - 1) as f64 + query_time.as_secs_f64()) / count as f64;

        Ok(result)
    }

    pub async fn query_one(&mut self, query: &str, params: &[&(dyn tokio_postgres::types::ToSql + Sync)]) -> Result<Row> {
        let start_time = std::time::Instant::now();
        
        let result = self.client.query_one(query, params).await
            .map_err(|e| anyhow::anyhow!("Query execution failed: {}", e))?;

        let query_time = start_time.duration_since(start_time);
        histogram!("fauxdb_query_duration_seconds").record(query_time.as_secs_f64());

        *self.stats.query_count.write() += 1;
        counter!("fauxdb_queries_total").increment(1);

        // Update average query time
        let mut avg_time = self.stats.avg_query_time.write();
        let count = *self.stats.query_count.read();
        *avg_time = (*avg_time * (count - 1) as f64 + query_time.as_secs_f64()) / count as f64;

        Ok(result)
    }
}

impl Drop for PooledConnection {
    fn drop(&mut self) {
        *self.stats.active_connections.write() -= 1;
        gauge!("fauxdb_pool_active_connections").set(*self.stats.active_connections.read() as f64);
    }
}

#[derive(Debug, Clone)]
pub struct PoolStatsSnapshot {
    pub total_connections: u64,
    pub active_connections: u64,
    pub idle_connections: u64,
    pub connection_errors: u64,
    pub query_count: u64,
    pub avg_query_time: f64,
    pub pool_size: u32,
    pub pool_idle: u32,
}

impl std::fmt::Display for PoolStatsSnapshot {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, 
            "Pool Stats: active={}, idle={}, total={}, errors={}, queries={}, avg_time={:.3}s, pool_size={}, pool_idle={}",
            self.active_connections, self.idle_connections, self.total_connections,
            self.connection_errors, self.query_count, self.avg_query_time,
            self.pool_size, self.pool_idle
        )
    }
}
