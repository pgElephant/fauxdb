/*
 * Copyright (c) 2025 pgElephant. All rights reserved.
 *
 * FauxDB - Production-ready MongoDB-compatible database server
 * Built with Rust for superior performance and reliability
 *
 * @file config.rs
 * @brief FauxDB configuration management
 */

use anyhow::Result;
use serde::{Deserialize, Serialize};
use std::path::Path;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Config {
    pub server: ServerConfig,
    pub database: DatabaseConfig,
    pub logging: LoggingConfig,
    pub metrics: MetricsConfig,
    pub ssl: SslConfig,
    pub authentication: AuthenticationConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ServerConfig {
    pub host: String,
    pub port: u16,
    pub max_connections: u32,
    pub connection_timeout_ms: u64,
    pub idle_timeout_ms: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DatabaseConfig {
    pub uri: String,
    pub max_connections: u32,
    pub connection_timeout_ms: u64,
    pub idle_timeout_ms: u64,
    pub enable_jsonb_extensions: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LoggingConfig {
    pub level: String,
    pub format: String,
    pub output: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MetricsConfig {
    pub enabled: bool,
    pub port: u16,
    pub path: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SslConfig {
    pub enabled: bool,
    pub cert_file: Option<String>,
    pub key_file: Option<String>,
    pub ca_file: Option<String>,
    pub verify_mode: String,
    pub min_tls_version: String,
    pub max_tls_version: String,
    pub cipher_suites: Vec<String>,
    pub allow_insecure: bool,
    pub client_cert_required: bool,
    pub sni_enabled: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AuthenticationConfig {
    pub enabled: bool,
    pub default_auth_method: String,
    pub session_timeout_minutes: u64,
    pub max_sessions_per_user: usize,
    pub password_policy: PasswordPolicyConfig,
    pub ldap: Option<LdapConfig>,
    pub kerberos: Option<KerberosConfig>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PasswordPolicyConfig {
    pub min_length: usize,
    pub require_uppercase: bool,
    pub require_lowercase: bool,
    pub require_numbers: bool,
    pub require_special_chars: bool,
    pub max_age_days: Option<u64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LdapConfig {
    pub server_url: String,
    pub base_dn: String,
    pub bind_dn: String,
    pub bind_password: String,
    pub user_search_filter: String,
    pub group_search_filter: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerberosConfig {
    pub realm: String,
    pub keytab_file: String,
    pub service_principal: String,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            server: ServerConfig {
                host: "127.0.0.1".to_string(),
                port: 27018,
                max_connections: 1000,
                connection_timeout_ms: 5000,
                idle_timeout_ms: 60000,
            },
            database: DatabaseConfig {
                uri: "postgresql://localhost:5432/fauxdb".to_string(),
                max_connections: 10,
                connection_timeout_ms: 5000,
                idle_timeout_ms: 60000,
                enable_jsonb_extensions: true,
            },
            logging: LoggingConfig {
                level: "info".to_string(),
                format: "json".to_string(),
                output: "stdout".to_string(),
            },
            metrics: MetricsConfig {
                enabled: true,
                port: 9090,
                path: "/metrics".to_string(),
            },
            ssl: SslConfig {
                enabled: false,
                cert_file: None,
                key_file: None,
                ca_file: None,
                verify_mode: "peer".to_string(),
                min_tls_version: "1.2".to_string(),
                max_tls_version: "1.3".to_string(),
                cipher_suites: vec![
                    "TLS_AES_256_GCM_SHA384".to_string(),
                    "TLS_CHACHA20_POLY1305_SHA256".to_string(),
                    "TLS_AES_128_GCM_SHA256".to_string(),
                ],
                allow_insecure: false,
                client_cert_required: false,
                sni_enabled: true,
            },
            authentication: AuthenticationConfig {
                enabled: true,
                default_auth_method: "SCRAM-SHA-256".to_string(),
                session_timeout_minutes: 30,
                max_sessions_per_user: 10,
                password_policy: PasswordPolicyConfig {
                    min_length: 8,
                    require_uppercase: true,
                    require_lowercase: true,
                    require_numbers: true,
                    require_special_chars: true,
                    max_age_days: Some(90),
                },
                ldap: None,
                kerberos: None,
            },
        }
    }
}

impl Config {
    pub async fn load<P: AsRef<Path>>(path: P) -> Result<Self> {
        let path = path.as_ref();
        
        if !path.exists() {
            let config = Self::default();
            config.save(path).await?;
            return Ok(config);
        }

        let content = tokio::fs::read_to_string(path).await?;
        
        let config: Config = match path.extension().and_then(|s| s.to_str()) {
            Some("json") => serde_json::from_str(&content)?,
            Some("yaml") | Some("yml") => serde_yaml::from_str(&content)?,
            Some("toml") => toml::from_str(&content)?,
            _ => toml::from_str(&content)?,
        };

        Ok(config)
    }

    pub async fn save<P: AsRef<Path>>(&self, path: P) -> Result<()> {
        let path = path.as_ref();
        let content = match path.extension().and_then(|s| s.to_str()) {
            Some("json") => serde_json::to_string_pretty(self)?,
            Some("yaml") | Some("yml") => serde_yaml::to_string(self)?,
            Some("toml") => toml::to_string_pretty(self)?,
            _ => toml::to_string_pretty(self)?,
        };

        tokio::fs::write(path, content).await?;
        Ok(())
    }
}
