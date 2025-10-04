/*
 * Copyright (c) 2025 pgElephant. All rights reserved.
 *
 * FauxDB - Production-ready MongoDB-compatible database server
 * Built with Rust for superior performance and reliability
 *
 * @file circuit_breaker.rs
 * @brief Circuit breaker pattern implementation for resilience
 */

use std::sync::Arc;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};
use tokio::sync::RwLock;
use tokio::time::sleep;
use crate::error::{FauxDBError, Result};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CircuitState {
    Closed,
    Open,
    HalfOpen,
}

impl CircuitState {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Closed => "closed",
            Self::Open => "open",
            Self::HalfOpen => "half_open",
        }
    }
}

#[derive(Debug, Clone)]
pub struct CircuitBreakerConfig {
    pub failure_threshold: u32,
    pub success_threshold: u32,
    pub timeout: Duration,
    pub volume_threshold: u32,
    pub sleep_window: Duration,
}

impl Default for CircuitBreakerConfig {
    fn default() -> Self {
        Self {
            failure_threshold: 5,
            success_threshold: 3,
            timeout: Duration::from_secs(30),
            volume_threshold: 10,
            sleep_window: Duration::from_secs(60),
        }
    }
}

#[derive(Debug)]
struct CircuitBreakerState {
    state: CircuitState,
    failure_count: u32,
    success_count: u32,
    request_count: u32,
    last_failure_time: Option<Instant>,
    last_success_time: Option<Instant>,
    next_attempt_time: Option<Instant>,
}

impl CircuitBreakerState {
    fn new() -> Self {
        Self {
            state: CircuitState::Closed,
            failure_count: 0,
            success_count: 0,
            request_count: 0,
            last_failure_time: None,
            last_success_time: None,
            next_attempt_time: None,
        }
    }

    fn reset(&mut self) {
        self.failure_count = 0;
        self.success_count = 0;
        self.request_count = 0;
        self.last_failure_time = None;
        self.last_success_time = None;
        self.next_attempt_time = None;
        self.state = CircuitState::Closed;
    }
}

#[derive(Debug)]
pub struct CircuitBreaker {
    name: String,
    config: CircuitBreakerConfig,
    state: Arc<RwLock<CircuitBreakerState>>,
    metrics: CircuitBreakerMetrics,
}

#[derive(Debug, Clone)]
pub struct CircuitBreakerMetrics {
    pub total_requests: u64,
    pub successful_requests: u64,
    pub failed_requests: u64,
    pub circuit_opens: u64,
    pub circuit_closes: u64,
    pub state_changes: u64,
    pub last_state_change: Option<u64>,
}

impl Default for CircuitBreakerMetrics {
    fn default() -> Self {
        Self {
            total_requests: 0,
            successful_requests: 0,
            failed_requests: 0,
            circuit_opens: 0,
            circuit_closes: 0,
            state_changes: 0,
            last_state_change: None,
        }
    }
}

impl CircuitBreaker {
    pub fn new(name: String, config: CircuitBreakerConfig) -> Self {
        Self {
            name,
            config,
            state: Arc::new(RwLock::new(CircuitBreakerState::new())),
            metrics: CircuitBreakerMetrics::default(),
        }
    }

    pub async fn call<F, T>(&self, operation: F) -> Result<T>
    where
        F: FnOnce() -> Result<T>,
    {
        // Check if we should allow the request
        {
            let state = self.state.read().await;
            if !self.should_allow_request(&state).await {
                let error = FauxDBError::circuit_breaker_error(
                    format!("Circuit breaker '{}' is open", self.name)
                );
                return Err(error);
            }
        }

        // Execute the operation with timeout
        let result = self.execute_with_timeout(operation).await;
        let success = result.is_ok();
        
        // Update circuit state based on result
        {
            let mut state = self.state.write().await;
            self.update_state(&mut state, success).await;
        }
        
        result
    }

    async fn should_allow_request(&self, state: &CircuitBreakerState) -> bool {
        match state.state {
            CircuitState::Closed => true,
            CircuitState::Open => {
                if let Some(next_attempt) = state.next_attempt_time {
                    Instant::now() >= next_attempt
                } else {
                    true
                }
            }
            CircuitState::HalfOpen => true,
        }
    }

    async fn execute_with_timeout<F, T>(&self, operation: F) -> Result<T>
    where
        F: FnOnce() -> Result<T>,
    {
        let timeout_future = sleep(self.config.timeout);
        let operation_future = async { operation() };
        
        tokio::select! {
            result = operation_future => result,
            _ = timeout_future => {
                Err(FauxDBError::timeout_error(
                    format!("Circuit breaker operation timeout for {}", self.name)
                ))
            }
        }
    }

    async fn update_state(&self, state: &mut CircuitBreakerState, success: bool) {
        state.request_count += 1;

        match state.state {
            CircuitState::Closed => {
                if success {
                    self.on_success_simple(state);
                } else {
                    self.on_failure_simple(state);
                    if self.should_open_circuit(state) {
                        self.open_circuit_simple(state).await;
                    }
                }
            }
            CircuitState::Open => {
                if Instant::now() >= state.next_attempt_time.unwrap_or(Instant::now()) {
                    state.state = CircuitState::HalfOpen;
                    state.success_count = 0;
                    state.failure_count = 0;
                }
            }
            CircuitState::HalfOpen => {
                if success {
                    self.on_success_simple(state);
                    if self.should_close_circuit(state) {
                        self.close_circuit_simple(state).await;
                    }
                } else {
                    self.on_failure_simple(state);
                    self.open_circuit_simple(state).await;
                }
            }
        }
    }

    fn on_success_simple(&self, state: &mut CircuitBreakerState) {
        state.success_count += 1;
        state.last_success_time = Some(Instant::now());
        
        if state.state == CircuitState::HalfOpen {
            state.failure_count = 0;
        }
    }

    fn on_failure_simple(&self, state: &mut CircuitBreakerState) {
        state.failure_count += 1;
        state.last_failure_time = Some(Instant::now());
        
        if state.state == CircuitState::HalfOpen {
            state.success_count = 0;
        }
    }

    fn should_open_circuit(&self, state: &CircuitBreakerState) -> bool {
        state.request_count >= self.config.volume_threshold &&
        state.failure_count >= self.config.failure_threshold
    }

    fn should_close_circuit(&self, state: &CircuitBreakerState) -> bool {
        state.success_count >= self.config.success_threshold
    }

    async fn open_circuit_simple(&self, state: &mut CircuitBreakerState) {
        state.state = CircuitState::Open;
        state.next_attempt_time = Some(Instant::now() + self.config.sleep_window);
    }

    async fn close_circuit_simple(&self, state: &mut CircuitBreakerState) {
        state.reset();
    }

    pub async fn get_state(&self) -> CircuitState {
        let state = self.state.read().await;
        state.state
    }

    pub async fn get_metrics(&self) -> CircuitBreakerMetrics {
        let state = self.state.read().await;
        CircuitBreakerMetrics {
            total_requests: self.metrics.total_requests,
            successful_requests: self.metrics.successful_requests,
            failed_requests: self.metrics.failed_requests,
            circuit_opens: self.metrics.circuit_opens,
            circuit_closes: self.metrics.circuit_closes,
            state_changes: self.metrics.state_changes,
            last_state_change: self.metrics.last_state_change,
        }
    }

    pub async fn reset(&mut self) {
        let mut state = self.state.write().await;
        state.reset();
        self.metrics = CircuitBreakerMetrics::default();
    }

    fn get_retry_after_ms(&self, state: &CircuitBreakerState) -> Option<u64> {
        match state.state {
            CircuitState::Open => {
                if let Some(next_attempt) = state.next_attempt_time {
                    let duration = next_attempt.duration_since(Instant::now());
                    if duration.as_millis() > 0 {
                        Some(duration.as_millis() as u64)
                    } else {
                        None
                    }
                } else {
                    Some(self.config.sleep_window.as_millis() as u64)
                }
            }
            _ => None,
        }
    }
}

pub struct CircuitBreakerManager {
    breakers: Arc<RwLock<std::collections::HashMap<String, Arc<CircuitBreaker>>>>,
}

impl CircuitBreakerManager {
    pub fn new() -> Self {
        Self {
            breakers: Arc::new(RwLock::new(std::collections::HashMap::new())),
        }
    }

    pub async fn get_or_create(
        &self,
        name: String,
        config: CircuitBreakerConfig,
    ) -> Arc<CircuitBreaker> {
        let mut breakers = self.breakers.write().await;
        
        if let Some(breaker) = breakers.get(&name) {
            Arc::clone(breaker)
        } else {
            let breaker = Arc::new(CircuitBreaker::new(name.clone(), config));
            breakers.insert(name, Arc::clone(&breaker));
            breaker
        }
    }

    pub async fn get_breaker(&self, name: &str) -> Option<Arc<CircuitBreaker>> {
        let breakers = self.breakers.read().await;
        breakers.get(name).map(Arc::clone)
    }

    pub async fn list_breakers(&self) -> Vec<String> {
        let breakers = self.breakers.read().await;
        breakers.keys().cloned().collect()
    }

    pub async fn reset_all(&self) {
        // For now, just log that reset was requested
        // TODO: Implement proper reset functionality
        println!("Circuit breaker reset requested");
    }

    pub async fn get_all_metrics(&self) -> std::collections::HashMap<String, CircuitBreakerMetrics> {
        let breakers = self.breakers.read().await;
        let mut metrics = std::collections::HashMap::new();
        
        for (name, breaker) in breakers.iter() {
            metrics.insert(name.clone(), breaker.get_metrics().await);
        }
        
        metrics
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::atomic::{AtomicU32, Ordering};

    #[tokio::test]
    async fn test_circuit_breaker_success() {
        let breaker = CircuitBreaker::new("test".to_string(), CircuitBreakerConfig::default());
        
        let result = breaker.call(|| Ok("success")).await;
        assert!(result.is_ok());
        assert_eq!(breaker.get_state().await, CircuitState::Closed);
    }

    #[tokio::test]
    async fn test_circuit_breaker_failure() {
        let config = CircuitBreakerConfig {
            failure_threshold: 2,
            volume_threshold: 2,
            ..Default::default()
        };
        let breaker = CircuitBreaker::new("test".to_string(), config);
        
        // First failure
        let result = breaker.call(|| Err(FauxDBError::internal_error("test error"))).await;
        assert!(result.is_err());
        assert_eq!(breaker.get_state().await, CircuitState::Closed);
        
        // Second failure should open circuit
        let result = breaker.call(|| Err(FauxDBError::internal_error("test error"))).await;
        assert!(result.is_err());
        assert_eq!(breaker.get_state().await, CircuitState::Open);
        
        // Third call should be rejected immediately
        let result = breaker.call(|| Ok("should not execute")).await;
        assert!(result.is_err());
        assert_eq!(breaker.get_state().await, CircuitState::Open);
    }

    #[tokio::test]
    async fn test_circuit_breaker_recovery() {
        let config = CircuitBreakerConfig {
            failure_threshold: 1,
            success_threshold: 2,
            volume_threshold: 1,
            sleep_window: Duration::from_millis(100),
            ..Default::default()
        };
        let breaker = CircuitBreaker::new("test".to_string(), config);
        
        // Open the circuit
        let _ = breaker.call(|| Err(FauxDBError::internal_error("test error"))).await;
        assert_eq!(breaker.get_state().await, CircuitState::Open);
        
        // Wait for sleep window
        sleep(Duration::from_millis(150)).await;
        
        // Circuit should be half-open
        assert_eq!(breaker.get_state().await, CircuitState::HalfOpen);
        
        // Successful calls should close the circuit
        let _ = breaker.call(|| Ok("success1")).await;
        let _ = breaker.call(|| Ok("success2")).await;
        assert_eq!(breaker.get_state().await, CircuitState::Closed);
    }
}
