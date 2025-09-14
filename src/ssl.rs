/*!
 * @file ssl.rs
 * @brief SSL/TLS support for FauxDB
 */

use crate::error::{FauxDBError, Result};
use crate::config::SslConfig;

pub struct SslManager {
    config: SslConfig,
}

impl SslManager {
    pub fn new(config: SslConfig) -> Self {
        Self { config }
    }

    pub fn is_enabled(&self) -> bool {
        self.config.enabled
    }

    pub fn get_cert_file(&self) -> Option<&str> {
        self.config.cert_file.as_deref()
    }

    pub fn get_key_file(&self) -> Option<&str> {
        self.config.key_file.as_deref()
    }

    pub fn get_ca_file(&self) -> Option<&str> {
        self.config.ca_file.as_deref()
    }

    pub fn get_verify_mode(&self) -> &str {
        &self.config.verify_mode
    }

    pub fn get_min_tls_version(&self) -> &str {
        &self.config.min_tls_version
    }

    pub fn get_max_tls_version(&self) -> &str {
        &self.config.max_tls_version
    }

    pub fn get_cipher_suites(&self) -> &[String] {
        &self.config.cipher_suites
    }

    pub fn is_allow_insecure(&self) -> bool {
        self.config.allow_insecure
    }

    pub fn is_client_cert_required(&self) -> bool {
        self.config.client_cert_required
    }

    pub fn is_sni_enabled(&self) -> bool {
        self.config.sni_enabled
    }

    pub async fn validate_config(&self) -> Result<()> {
        if !self.config.enabled {
            return Ok(());
        }

        if let Some(cert_file) = &self.config.cert_file {
            if !std::path::Path::new(cert_file).exists() {
                return Err(FauxDBError::Config(format!("Certificate file not found: {}", cert_file)));
            }
        }

        if let Some(key_file) = &self.config.key_file {
            if !std::path::Path::new(key_file).exists() {
                return Err(FauxDBError::Config(format!("Private key file not found: {}", key_file)));
            }
        }

        if let Some(ca_file) = &self.config.ca_file {
            if !std::path::Path::new(ca_file).exists() {
                return Err(FauxDBError::Config(format!("CA file not found: {}", ca_file)));
            }
        }

        Ok(())
    }

    pub fn get_tls_config(&self) -> Result<TlsConfig> {
        if !self.config.enabled {
            return Err(FauxDBError::Config("SSL/TLS is not enabled".to_string()));
        }

        Ok(TlsConfig {
            cert_file: self.config.cert_file.clone(),
            key_file: self.config.key_file.clone(),
            ca_file: self.config.ca_file.clone(),
            verify_mode: self.config.verify_mode.clone(),
            min_tls_version: self.config.min_tls_version.clone(),
            max_tls_version: self.config.max_tls_version.clone(),
            cipher_suites: self.config.cipher_suites.clone(),
            allow_insecure: self.config.allow_insecure,
            client_cert_required: self.config.client_cert_required,
            sni_enabled: self.config.sni_enabled,
        })
    }
}

#[derive(Debug, Clone)]
pub struct TlsConfig {
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

impl Default for TlsConfig {
    fn default() -> Self {
        Self {
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
        }
    }
}
