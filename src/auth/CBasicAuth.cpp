/*-------------------------------------------------------------------------
 *
 * CBasicAuth.cpp
 *      Basic authentication support for PostgreSQL connections in FauxDB.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------*/

#include "auth/CBasicAuth.hpp"
#include <sstream>
#include <algorithm>
#include <iostream>

namespace FauxDB
{

CBasicAuth::CBasicAuth()
    : lastError_(""), initialized_(false)
{
    config_.type = AuthType::BASIC;
    config_.direction = AuthDirection::POSTGRESQL_CLIENT_SIDE;
    config_.name = "Basic PostgreSQL Client Authentication";
}

CBasicAuth::CBasicAuth(const AuthConfig& config)
    : config_(config), lastError_(""), initialized_(false)
{
    initialize(config);
}

CBasicAuth::~CBasicAuth() = default;

bool CBasicAuth::initialize(const AuthConfig& config)
{
    config_ = config;
    config_.type = AuthType::BASIC;
    config_.direction = AuthDirection::POSTGRESQL_CLIENT_SIDE;
    if (config_.name.empty())
    {
        config_.name = "Basic PostgreSQL Client Authentication";
    }
    
    lastError_.clear();
    
    if (!validateConfig())
    {
        return false;
    }
    
    initialized_ = true;
    return true;
}

bool CBasicAuth::authenticate(const std::string& username, const std::string& password)
{
    if (!initialized_)
    {
        setError("Basic authentication not initialized");
        return false;
    }
    
    if (!config_.required)
    {
        return true; /* Authentication not required */
    }
    
    if (username.empty() || password.empty())
    {
        setError("Username and password are required for authentication");
        return false;
    }
    
    return validateCredentials(username, password);
}

bool CBasicAuth::isRequired() const
{
    return config_.required;
}

AuthType CBasicAuth::getType() const
{
    return AuthType::BASIC;
}

std::string CBasicAuth::getName() const
{
    return config_.name;
}

std::string CBasicAuth::getLastError() const
{
    return lastError_;
}

bool CBasicAuth::configureSSL()
{
    if (!initialized_)
    {
        setError("Basic authentication not initialized");
        return false;
    }
    
    if (!config_.useSSL)
    {
        return true; /* SSL not required */
    }
    
    /* Validate SSL configuration */
    if (config_.sslCert.empty() || config_.sslKey.empty())
    {
        setError("SSL certificate and key paths are required when SSL is enabled");
        return false;
    }
    
    return true;
}

bool CBasicAuth::isSSLEnabled() const
{
    return config_.useSSL;
}



std::string CBasicAuth::buildConnectionString(const std::string& host,
                                              const std::string& port,
                                              const std::string& database) const
{
    if (!initialized_)
    {
        return "";
    }
    
    std::ostringstream connStr;
    connStr << "host=" << host;
    connStr << " port=" << port;
    connStr << " dbname=" << database;
    
    if (!config_.username.empty())
    {
        connStr << " user=" << config_.username;
    }
    
    if (!config_.password.empty())
    {
        connStr << " password=" << config_.password;
    }
    
    /* Add SSL configuration if enabled */
    if (config_.useSSL)
    {
        connStr << " sslmode=require";
        if (!config_.sslCert.empty())
        {
            connStr << " sslcert=" << config_.sslCert;
        }
        if (!config_.sslKey.empty())
        {
            connStr << " sslkey=" << config_.sslKey;
        }
        if (!config_.sslCA.empty())
        {
            connStr << " sslrootcert=" << config_.sslCA;
        }
    }
    else
    {
        connStr << " sslmode=prefer";
    }
    
    return connStr.str();
}

bool CBasicAuth::validateConfig() const
{
    if (config_.type != AuthType::BASIC)
    {
        setError("Unsupported authentication type");
        return false;
    }
    
    if (config_.required)
    {
        if (config_.username.empty())
        {
            setError("Username is required when authentication is enabled");
            return false;
        }
        
        if (config_.password.empty())
        {
            setError("Password is required when authentication is enabled");
            return false;
        }
        
        if (config_.database.empty())
        {
            setError("Authentication database is required when authentication is enabled");
            return false;
        }
    }
    
    return true;
}

bool CBasicAuth::validateCredentials(const std::string& username,
                                    const std::string& password) const
{
    /* For basic authentication, we simply compare the provided credentials
     * with the configured ones. In a production environment, this would
     * typically involve checking against a user database or external
     * authentication service. */
    
    if (username != config_.username)
    {
        setError("Invalid username");
        return false;
    }
    
    if (password != config_.password)
    {
        setError("Invalid password");
        return false;
    }
    
    return true;
}

std::string CBasicAuth::buildSSLConnectionString() const
{
    if (!config_.useSSL)
    {
        return "";
    }
    
    std::ostringstream sslStr;
    sslStr << "sslmode=require";
    
    if (!config_.sslCert.empty())
    {
        sslStr << " sslcert=" << config_.sslCert;
    }
    
    if (!config_.sslKey.empty())
    {
        sslStr << " sslkey=" << config_.sslKey;
    }
    
    if (!config_.sslCA.empty())
    {
        sslStr << " sslrootcert=" << config_.sslCA;
    }
    
    return sslStr.str();
}

void CBasicAuth::setError(const std::string& error) const
{
    lastError_ = error;
}

/* IPostgreSQLAuth interface implementation */
bool CBasicAuth::validateConnection(const std::string& connectionString)
{
    if (!initialized_)
    {
        setError("Basic authentication not initialized");
        return false;
    }
    
    /* For basic authentication, we just validate the connection string format */
    if (connectionString.empty())
    {
        setError("Connection string is empty");
        return false;
    }
    
    /* Check for required components */
    if (connectionString.find("host=") == std::string::npos ||
        connectionString.find("dbname=") == std::string::npos)
    {
        setError("Connection string missing required components");
        return false;
    }
    
    return true;
}

std::string CBasicAuth::getPostgreSQLUser() const
{
    return config_.username;
}

std::string CBasicAuth::getPostgreSQLPassword() const
{
    return config_.password;
}

bool CBasicAuth::testConnection()
{
    if (!initialized_)
    {
        setError("Basic authentication not initialized");
        return false;
    }
    
    /* For basic authentication, we assume the connection is valid
     * if the credentials are properly configured */
    if (config_.required && (config_.username.empty() || config_.password.empty()))
    {
        setError("Username and password required for connection test");
        return false;
    }
    
    return true;
}

std::string CBasicAuth::getConnectionInfo() const
{
    std::ostringstream info;
    info << "PostgreSQL Client Basic Authentication - ";
    info << "User: " << (config_.username.empty() ? "not set" : config_.username);
    info << ", SSL: " << (config_.useSSL ? "enabled" : "disabled");
    info << ", Required: " << (config_.required ? "yes" : "no");
    return info.str();
}

std::string CBasicAuth::buildPostgreSQLConnectionString(const std::string& host,
                                                        const std::string& port,
                                                        const std::string& database) const
{
    return buildConnectionString(host, port, database);
}

} // namespace FauxDB
