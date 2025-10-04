/*
 * Copyright (c) 2025 pgElephant. All rights reserved.
 *
 * FauxDB - Production-ready MongoDB-compatible database server
 * Built with Rust for superior performance and reliability
 *
 * @file rate_limiter.rs
 * @brief Rate limiting and request throttling
 */

use std::collections::HashMap;
use std::sync::Arc;
use std::time::{Duration, Instant};
use tokio::sync::RwLock;
use crate::error::{FauxDBError, Result};

#[derive(Debug, Clone)]
pub struct RateLimitConfig {
    pub requests_per_second: u32,
    pub burst_size: u32,
    pub window_size_seconds: u64,
    pub enabled: bool,
}

impl Default for RateLimitConfig {
    fn default() -> Self {
        Self {
            requests_per_second: 1000,
            burst_size: 2000,
            window_size_seconds: 60,
            enabled: true,
        }
    }
}

#[derive(Debug, Clone)]
pub struct RateLimitRule {
    pub name: String,
    pub config: RateLimitConfig,
    pub scope: RateLimitScope,
}

#[derive(Debug, Clone)]
pub enum RateLimitScope {
    Global,
    PerUser(String),
    PerIP(String),
    PerDatabase(String),
    PerCollection(String, String), // (database, collection)
}

#[derive(Debug)]
struct TokenBucket {
    capacity: u32,
    tokens: u32,
    last_refill: Instant,
    refill_rate: f64, // tokens per second
}

impl TokenBucket {
    fn new(capacity: u32, refill_rate: f64) -> Self {
        Self {
            capacity,
            tokens: capacity,
            last_refill: Instant::now(),
            refill_rate,
        }
    }

    fn try_consume(&mut self, tokens: u32) -> bool {
        self.refill();
        
        if self.tokens >= tokens {
            self.tokens -= tokens;
            true
        } else {
            false
        }
    }

    fn refill(&mut self) {
        let now = Instant::now();
        let elapsed = now.duration_since(self.last_refill);
        let tokens_to_add = (elapsed.as_secs_f64() * self.refill_rate) as u32;
        
        if tokens_to_add > 0 {
            self.tokens = (self.tokens + tokens_to_add).min(self.capacity);
            self.last_refill = now;
        }
    }

    fn tokens_available(&mut self) -> u32 {
        self.refill();
        self.tokens
    }

    fn time_until_next_token(&self) -> Duration {
        if self.tokens > 0 {
            Duration::from_secs(0)
        } else {
            Duration::from_secs_f64(1.0 / self.refill_rate)
        }
    }
}

#[derive(Debug)]
struct RateLimitEntry {
    bucket: TokenBucket,
    last_request: Instant,
    request_count: u64,
    window_start: Instant,
}

impl RateLimitEntry {
    fn new(config: &RateLimitConfig) -> Self {
        Self {
            bucket: TokenBucket::new(config.burst_size, config.requests_per_second as f64),
            last_request: Instant::now(),
            request_count: 0,
            window_start: Instant::now(),
        }
    }

    fn reset_window_if_needed(&mut self, window_size: Duration) {
        if self.window_start.elapsed() >= window_size {
            self.window_start = Instant::now();
            self.request_count = 0;
        }
    }
}

pub struct RateLimiter {
    rules: HashMap<String, RateLimitRule>,
    buckets: Arc<RwLock<HashMap<String, RateLimitEntry>>>,
    cleanup_interval: Duration,
}

impl RateLimiter {
    pub fn new() -> Self {
        Self {
            rules: HashMap::new(),
            buckets: Arc::new(RwLock::new(HashMap::new())),
            cleanup_interval: Duration::from_secs(300), // 5 minutes
        }
    }

    pub fn add_rule(&mut self, rule: RateLimitRule) {
        self.rules.insert(rule.name.clone(), rule);
    }

    pub async fn check_rate_limit(
        &self,
        rule_name: &str,
        scope: &RateLimitScope,
    ) -> Result<RateLimitResult> {
        let rule = self.rules.get(rule_name)
            .ok_or_else(|| FauxDBError::configuration_error(
                format!("Rate limit rule '{}' not found", rule_name)
            ))?;

        if !rule.config.enabled {
            return Ok(RateLimitResult::Allowed);
        }

        let bucket_key = self.get_bucket_key(rule_name, scope);
        let mut buckets = self.buckets.write().await;
        
        let entry = buckets.entry(bucket_key.clone()).or_insert_with(|| {
            RateLimitEntry::new(&rule.config)
        });

        entry.reset_window_if_needed(Duration::from_secs(rule.config.window_size_seconds));

        // Check if we can consume tokens
        if entry.bucket.try_consume(1) {
            entry.last_request = Instant::now();
            entry.request_count += 1;
            
            Ok(RateLimitResult::Allowed)
        } else {
            let time_until_next = entry.bucket.time_until_next_token();
            let retry_after = time_until_next.as_secs().max(1);
            
            Ok(RateLimitResult::RateLimited {
                retry_after_seconds: retry_after,
                limit: rule.config.requests_per_second,
                window_seconds: rule.config.window_size_seconds,
            })
        }
    }

    pub async fn get_rate_limit_status(
        &self,
        rule_name: &str,
        scope: &RateLimitScope,
    ) -> Result<RateLimitStatus> {
        let rule = self.rules.get(rule_name)
            .ok_or_else(|| FauxDBError::configuration_error(
                format!("Rate limit rule '{}' not found", rule_name)
            ))?;

        let bucket_key = self.get_bucket_key(rule_name, scope);
        let buckets = self.buckets.read().await;
        
        if let Some(_entry) = buckets.get(&bucket_key) {
            // For now, return basic status without mutable access
            Ok(RateLimitStatus {
                rule_name: rule_name.to_string(),
                scope: scope.clone(),
                tokens_available: rule.config.burst_size, // Placeholder
                tokens_limit: rule.config.burst_size,
                requests_in_window: 0, // Placeholder
                window_seconds: rule.config.window_size_seconds,
                time_until_next_token_ms: 0, // Placeholder
                last_request_time: Instant::now(), // Placeholder
            })
        } else {
            Ok(RateLimitStatus {
                rule_name: rule_name.to_string(),
                scope: scope.clone(),
                tokens_available: rule.config.burst_size,
                tokens_limit: rule.config.burst_size,
                requests_in_window: 0,
                window_seconds: rule.config.window_size_seconds,
                time_until_next_token_ms: 0,
                last_request_time: Instant::now(),
            })
        }
    }

    fn get_bucket_key(&self, rule_name: &str, scope: &RateLimitScope) -> String {
        match scope {
            RateLimitScope::Global => format!("{}:global", rule_name),
            RateLimitScope::PerUser(user) => format!("{}:user:{}", rule_name, user),
            RateLimitScope::PerIP(ip) => format!("{}:ip:{}", rule_name, ip),
            RateLimitScope::PerDatabase(db) => format!("{}:db:{}", rule_name, db),
            RateLimitScope::PerCollection(db, collection) => {
                format!("{}:coll:{}:{}", rule_name, db, collection)
            }
        }
    }

    pub async fn start_cleanup_task(&self) {
        let buckets = Arc::clone(&self.buckets);
        let cleanup_interval = self.cleanup_interval;
        
        tokio::spawn(async move {
            let mut interval = tokio::time::interval(cleanup_interval);
            loop {
                interval.tick().await;
                
                let mut buckets = buckets.write().await;
                let now = Instant::now();
                
                // Remove buckets that haven't been used in the last hour
                buckets.retain(|_, entry| {
                    now.duration_since(entry.last_request) < Duration::from_secs(3600)
                });
            }
        });
    }

    pub fn create_default_rules(&mut self) {
        // Global rate limit
        self.add_rule(RateLimitRule {
            name: "global".to_string(),
            config: RateLimitConfig {
                requests_per_second: 10000,
                burst_size: 20000,
                window_size_seconds: 60,
                enabled: true,
            },
            scope: RateLimitScope::Global,
        });

        // Per-user rate limit
        self.add_rule(RateLimitRule {
            name: "per_user".to_string(),
            config: RateLimitConfig {
                requests_per_second: 1000,
                burst_size: 2000,
                window_size_seconds: 60,
                enabled: true,
            },
            scope: RateLimitScope::Global, // Will be overridden per user
        });

        // Per-IP rate limit
        self.add_rule(RateLimitRule {
            name: "per_ip".to_string(),
            config: RateLimitConfig {
                requests_per_second: 500,
                burst_size: 1000,
                window_size_seconds: 60,
                enabled: true,
            },
            scope: RateLimitScope::Global, // Will be overridden per IP
        });

        // Database operation rate limit
        self.add_rule(RateLimitRule {
            name: "database_ops".to_string(),
            config: RateLimitConfig {
                requests_per_second: 2000,
                burst_size: 4000,
                window_size_seconds: 60,
                enabled: true,
            },
            scope: RateLimitScope::Global,
        });

        // Write operation rate limit (more restrictive)
        self.add_rule(RateLimitRule {
            name: "write_ops".to_string(),
            config: RateLimitConfig {
                requests_per_second: 500,
                burst_size: 1000,
                window_size_seconds: 60,
                enabled: true,
            },
            scope: RateLimitScope::Global,
        });
    }
}

#[derive(Debug, Clone)]
pub enum RateLimitResult {
    Allowed,
    RateLimited {
        retry_after_seconds: u64,
        limit: u32,
        window_seconds: u64,
    },
}

#[derive(Debug, Clone)]
pub struct RateLimitStatus {
    pub rule_name: String,
    pub scope: RateLimitScope,
    pub tokens_available: u32,
    pub tokens_limit: u32,
    pub requests_in_window: u64,
    pub window_seconds: u64,
    pub time_until_next_token_ms: u64,
    pub last_request_time: Instant,
}

impl RateLimitResult {
    pub fn is_allowed(&self) -> bool {
        matches!(self, Self::Allowed)
    }

    pub fn retry_after_seconds(&self) -> Option<u64> {
        match self {
            Self::Allowed => None,
            Self::RateLimited { retry_after_seconds, .. } => Some(*retry_after_seconds),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_rate_limiter_basic() {
        let mut limiter = RateLimiter::new();
        limiter.create_default_rules();

        let scope = RateLimitScope::Global;
        
        // First request should be allowed
        let result = limiter.check_rate_limit("global", &scope).await.unwrap();
        assert!(result.is_allowed());

        // Get status
        let status = limiter.get_rate_limit_status("global", &scope).await.unwrap();
        assert_eq!(status.rule_name, "global");
        assert!(status.tokens_available < status.tokens_limit);
    }

    #[tokio::test]
    async fn test_rate_limiter_per_user() {
        let mut limiter = RateLimiter::new();
        limiter.create_default_rules();

        let user1_scope = RateLimitScope::PerUser("user1".to_string());
        let user2_scope = RateLimitScope::PerUser("user2".to_string());
        
        // Both users should have separate buckets
        let result1 = limiter.check_rate_limit("per_user", &user1_scope).await.unwrap();
        let result2 = limiter.check_rate_limit("per_user", &user2_scope).await.unwrap();
        
        assert!(result1.is_allowed());
        assert!(result2.is_allowed());
    }
}
