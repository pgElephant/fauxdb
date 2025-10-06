/*!
 * Authentication tests for FauxDB
 * Tests the authentication methods and session management
 */

use fauxdb::*;
use anyhow::Result;
use fauxdb::authentication::AuthenticationManager;
use fauxdb::config::PasswordPolicyConfig;
use fauxdb::config::{AuthenticationConfig, LdapConfig, KerberosConfig};

#[tokio::test]
async fn test_scram_sha1_authentication() -> Result<()> {
    
    let config = AuthenticationConfig {
        enabled: true,
        default_auth_method: "SCRAM-SHA-1".to_string(),
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
    };
    
    let auth_manager = AuthenticationManager::new(config);
    
    // Test successful authentication
    let result = auth_manager.authenticate_scram_sha1("testuser", "testpassword").await?;
    assert!(result);
    
    // Test failed authentication with empty credentials
    let result = auth_manager.authenticate_scram_sha1("", "testpassword").await?;
    assert!(!result);
    
    let result = auth_manager.authenticate_scram_sha1("testuser", "").await?;
    assert!(!result);
    
    Ok(())
}

#[tokio::test]
async fn test_scram_sha256_authentication() -> Result<()> {
    
    let config = AuthenticationConfig {
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
    };
    
    let auth_manager = AuthenticationManager::new(config);
    
    // Test successful authentication
    let result = auth_manager.authenticate_scram_sha256("testuser", "testpassword").await?;
    assert!(result);
    
    // Test failed authentication with empty credentials
    let result = auth_manager.authenticate_scram_sha256("", "testpassword").await?;
    assert!(!result);
    
    let result = auth_manager.authenticate_scram_sha256("testuser", "").await?;
    assert!(!result);
    
    Ok(())
}

#[tokio::test]
async fn test_ldap_authentication() -> Result<()> {
    
    let ldap_config = LdapConfig {
        server_url: "ldap://localhost:389".to_string(),
        base_dn: "dc=example,dc=com".to_string(),
        bind_dn: "cn=admin,dc=example,dc=com".to_string(),
        bind_password: "adminpassword".to_string(),
        user_search_filter: "(uid={})".to_string(),
        group_search_filter: "(member={})".to_string(),
    };
    
    let config = AuthenticationConfig {
        enabled: true,
        default_auth_method: "LDAP".to_string(),
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
        ldap: Some(ldap_config),
        kerberos: None,
    };
    
    let auth_manager = AuthenticationManager::new(config);
    
    // Test successful authentication
    let result = auth_manager.authenticate_ldap("testuser", "testpassword").await?;
    assert!(result);
    
    // Test failed authentication with empty credentials
    let result = auth_manager.authenticate_ldap("", "testpassword").await?;
    assert!(!result);
    
    let result = auth_manager.authenticate_ldap("testuser", "").await?;
    assert!(!result);
    
    Ok(())
}

#[tokio::test]
async fn test_kerberos_authentication() -> Result<()> {
    
    let kerberos_config = KerberosConfig {
        realm: "EXAMPLE.COM".to_string(),
        keytab_file: "/etc/krb5.keytab".to_string(),
        service_principal: "mongodb/server.example.com@EXAMPLE.COM".to_string(),
    };
    
    let config = AuthenticationConfig {
        enabled: true,
        default_auth_method: "Kerberos".to_string(),
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
        kerberos: Some(kerberos_config),
    };
    
    let auth_manager = AuthenticationManager::new(config);
    
    // Test successful authentication
    let result = auth_manager.authenticate_kerberos("testuser", "kerberostoken").await?;
    assert!(result);
    
    // Test failed authentication with empty credentials
    let result = auth_manager.authenticate_kerberos("", "kerberostoken").await?;
    assert!(!result);
    
    let result = auth_manager.authenticate_kerberos("testuser", "").await?;
    assert!(!result);
    
    Ok(())
}

#[tokio::test]
async fn test_session_management() -> Result<()> {
    
    let config = AuthenticationConfig {
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
    };
    
    let auth_manager = AuthenticationManager::new(config);
    
    // Test session creation
    let session = auth_manager.create_session("testuser", "SCRAM-SHA-256").await?;
    assert_eq!(session.username, "testuser");
    assert_eq!(session.auth_method, "SCRAM-SHA-256");
    assert!(session.is_active);
    
    // Test session invalidation
    let result = auth_manager.invalidate_session(&session.id).await;
    assert!(result.is_ok());
    
    Ok(())
}

#[tokio::test]
async fn test_authentication_error_handling() -> Result<()> {
    
    let config = AuthenticationConfig {
        enabled: true,
        default_auth_method: "LDAP".to_string(),
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
        ldap: None, // LDAP not configured
        kerberos: None,
    };
    
    let auth_manager = AuthenticationManager::new(config);
    
    // Test LDAP authentication without configuration
    let result = auth_manager.authenticate_ldap("testuser", "testpassword").await;
    assert!(result.is_err());
    
    Ok(())
}
