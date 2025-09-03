/*-------------------------------------------------------------------------
 *
 * CScramMongoAuth.cpp
 *      SCRAM server-side authentication for FauxDB as MongoDB server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------*/

#include "auth/CScramMongoAuth.hpp"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <random>
#include <iomanip>

namespace FauxDB
{

CScramMongoAuth::CScramMongoAuth()
    : mechanism_(ScramMechanism::SCRAM_SHA_256), lastError_(""), initialized_(false)
{
    config_.type = AuthType::SCRAM_SHA_256;
    config_.direction = AuthDirection::MONGODB_SERVER_SIDE;
    config_.name = "SCRAM-SHA-256 MongoDB Server Authentication";
}

CScramMongoAuth::CScramMongoAuth(const AuthConfig& config)
    : config_(config), mechanism_(ScramMechanism::SCRAM_SHA_256), lastError_(""), initialized_(false)
{
    initialize(config);
}

CScramMongoAuth::~CScramMongoAuth() = default;

bool CScramMongoAuth::initialize(const AuthConfig& config)
{
    config_ = config;
    config_.direction = AuthDirection::MONGODB_SERVER_SIDE;
    
    if (config_.type == AuthType::SCRAM_SHA_1)
    {
        mechanism_ = ScramMechanism::SCRAM_SHA_1;
        config_.name = "SCRAM-SHA-1 MongoDB Server Authentication";
    }
    else
    {
        mechanism_ = ScramMechanism::SCRAM_SHA_256;
        config_.type = AuthType::SCRAM_SHA_256;
        config_.name = "SCRAM-SHA-256 MongoDB Server Authentication";
    }
    
    lastError_.clear();
    
    if (!validateConfig())
    {
        return false;
    }
    
    initialized_ = true;
    return true;
}

bool CScramMongoAuth::authenticate(const std::string& username, const std::string& password)
{
    if (!initialized_)
    {
        setError("SCRAM authentication not initialized");
        return false;
    }
    
    if (!config_.required)
    {
        return true; /* Authentication not required */
    }
    
    if (username.empty() || password.empty())
    {
        setError("Username and password are required for SCRAM authentication");
        return false;
    }
    
    return authenticateMongoDBClient(username, password);
}

AuthType CScramMongoAuth::getType() const
{
    return config_.type;
}

std::string CScramMongoAuth::getName() const
{
    return config_.name;
}

bool CScramMongoAuth::isRequired() const
{
    return config_.required;
}

std::string CScramMongoAuth::getLastError() const
{
    return lastError_;
}

std::string CScramMongoAuth::buildConnectionString(const std::string& host,
                                                   const std::string& port,
                                                   const std::string& database) const
{
    /* For MongoDB server-side authentication, we don't build connection strings */
    return "";
}

bool CScramMongoAuth::configureSSL()
{
    if (!initialized_)
    {
        setError("SCRAM authentication not initialized");
        return false;
    }
    
    /* SSL configuration is handled at the server level for MongoDB */
    return true;
}

bool CScramMongoAuth::isSSLEnabled() const
{
    return config_.useSSL;
}

/* IMongoDBAuth interface implementation */
MongoAuthChallenge CScramMongoAuth::createChallenge(const std::string& username)
{
    MongoAuthChallenge challenge;
    
    if (!initialized_)
    {
        setError("SCRAM authentication not initialized");
        return challenge;
    }
    
    if (!userExists(username))
    {
        setError("User does not exist: " + username);
        return challenge;
    }
    
    challenge.nonce = generateNonce();
    challenge.salt = generateSalt();
    challenge.iterationCount = 4096; /* Standard SCRAM iteration count */
    
    /* Store challenge in session for later validation */
    ScramSession session;
    session.username = username;
    session.nonce = challenge.nonce;
    session.mechanism = mechanism_;
    session.completed = false;
    
    sessions_[username] = session;
    
    return challenge;
}

MongoAuthResponse CScramMongoAuth::processResponse(const std::string& username,
                                                   const std::string& password,
                                                   const MongoAuthChallenge& challenge)
{
    MongoAuthResponse response;
    
    if (!initialized_)
    {
        response.success = false;
        response.message = "SCRAM authentication not initialized";
        return response;
    }
    
    if (!userExists(username))
    {
        response.success = false;
        response.message = "User does not exist: " + username;
        return response;
    }
    
    /* Validate the challenge response */
    if (validateClientProof(username, password, challenge))
    {
        response.success = true;
        response.message = "Authentication successful";
        response.proof = generateServerProof(username, password, challenge);
        
        /* Mark session as completed */
        if (sessions_.find(username) != sessions_.end())
        {
            sessions_[username].completed = true;
        }
    }
    else
    {
        response.success = false;
        response.message = "Authentication failed";
    }
    
    return response;
}

bool CScramMongoAuth::validateClientProof(const std::string& username,
                                          const std::string& clientProof,
                                          const MongoAuthChallenge& challenge)
{
    /* Simplified validation - in a real implementation, this would involve
     * proper SCRAM protocol validation with stored keys */
    
    if (!userExists(username))
    {
        return false;
    }
    
    /* For demonstration, we'll do a simple credential check */
    auto it = users_.find(username);
    if (it != users_.end())
    {
        return it->second.password == clientProof; /* Simplified */
    }
    
    return false;
}

std::string CScramMongoAuth::generateServerProof(const std::string& username,
                                                 const std::string& clientProof,
                                                 const MongoAuthChallenge& challenge)
{
    /* Generate server proof for SCRAM authentication */
    std::ostringstream proof;
    proof << "v=" << base64Encode("server_proof_" + username + "_" + challenge.nonce);
    return proof.str();
}

bool CScramMongoAuth::createUser(const std::string& username, const std::string& password)
{
    if (!initialized_)
    {
        setError("SCRAM authentication not initialized");
        return false;
    }
    
    if (username.empty() || password.empty())
    {
        setError("Username and password are required");
        return false;
    }
    
    if (userExists(username))
    {
        setError("User already exists: " + username);
        return false;
    }
    
    ScramCredentials credentials;
    credentials.username = username;
    credentials.password = password;
    credentials.salt = generateSalt();
    credentials.iterationCount = 4096;
    credentials.mechanism = mechanism_;
    
    /* Compute stored key and server key */
    credentials.storedKey = computeStoredKey(password, credentials.salt, credentials.iterationCount);
    credentials.serverKey = computeServerKey(password, credentials.salt, credentials.iterationCount);
    
    users_[username] = credentials;
    return true;
}

bool CScramMongoAuth::deleteUser(const std::string& username)
{
    if (!initialized_)
    {
        setError("SCRAM authentication not initialized");
        return false;
    }
    
    auto it = users_.find(username);
    if (it != users_.end())
    {
        users_.erase(it);
        return true;
    }
    
    setError("User not found: " + username);
    return false;
}

bool CScramMongoAuth::updateUserPassword(const std::string& username, const std::string& newPassword)
{
    if (!initialized_)
    {
        setError("SCRAM authentication not initialized");
        return false;
    }
    
    if (!userExists(username))
    {
        setError("User not found: " + username);
        return false;
    }
    
    if (newPassword.empty())
    {
        setError("New password cannot be empty");
        return false;
    }
    
    auto it = users_.find(username);
    if (it != users_.end())
    {
        it->second.password = newPassword;
        it->second.salt = generateSalt();
        it->second.storedKey = computeStoredKey(newPassword, it->second.salt, it->second.iterationCount);
        it->second.serverKey = computeServerKey(newPassword, it->second.salt, it->second.iterationCount);
        return true;
    }
    
    return false;
}

bool CScramMongoAuth::userExists(const std::string& username)
{
    return users_.find(username) != users_.end();
}

bool CScramMongoAuth::authenticateMongoDBClient(const std::string& username, const std::string& password)
{
    if (!initialized_)
    {
        setError("SCRAM authentication not initialized");
        return false;
    }
    
    if (!config_.required)
    {
        return true; /* Authentication not required */
    }
    
    if (username.empty() || password.empty())
    {
        setError("Username and password are required");
        return false;
    }
    
    if (!userExists(username))
    {
        setError("User does not exist: " + username);
        return false;
    }
    
    auto it = users_.find(username);
    if (it != users_.end())
    {
        return it->second.password == password; /* Simplified validation */
    }
    
    return false;
}

/* SCRAM-specific methods */
ScramMechanism CScramMongoAuth::getMechanism() const
{
    return mechanism_;
}

void CScramMongoAuth::setMechanism(ScramMechanism mechanism)
{
    mechanism_ = mechanism;
    if (mechanism == ScramMechanism::SCRAM_SHA_1)
    {
        config_.type = AuthType::SCRAM_SHA_1;
    }
    else
    {
        config_.type = AuthType::SCRAM_SHA_256;
    }
}

std::string CScramMongoAuth::generateNonce() const
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::string nonce;
    for (int i = 0; i < 16; ++i)
    {
        nonce += static_cast<char>(dis(gen));
    }
    
    return base64Encode(nonce);
}

std::string CScramMongoAuth::generateSalt() const
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::string salt;
    for (int i = 0; i < 16; ++i)
    {
        salt += static_cast<char>(dis(gen));
    }
    
    return base64Encode(salt);
}

std::string CScramMongoAuth::computeStoredKey(const std::string& password, const std::string& salt, int iterations) const
{
    /* Simplified implementation - in reality, this would use proper PBKDF2 */
    std::ostringstream key;
    key << "stored_key_" << password << "_" << salt << "_" << iterations;
    return base64Encode(key.str());
}

std::string CScramMongoAuth::computeServerKey(const std::string& password, const std::string& salt, int iterations) const
{
    /* Simplified implementation - in reality, this would use proper PBKDF2 */
    std::ostringstream key;
    key << "server_key_" << password << "_" << salt << "_" << iterations;
    return base64Encode(key.str());
}

bool CScramMongoAuth::validateConfig() const
{
    if (config_.type != AuthType::SCRAM_SHA_1 && config_.type != AuthType::SCRAM_SHA_256)
    {
        setError("Unsupported SCRAM authentication type");
        return false;
    }
    
    if (config_.required)
    {
        if (config_.database.empty())
        {
            setError("Authentication database is required when authentication is enabled");
            return false;
        }
    }
    
    return true;
}

std::string CScramMongoAuth::base64Encode(const std::string& data) const
{
    /* Simplified base64 encoding - in production, use a proper base64 library */
    std::ostringstream encoded;
    for (unsigned char c : data)
    {
        encoded << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
    return encoded.str();
}

std::string CScramMongoAuth::base64Decode(const std::string& data) const
{
    /* Simplified base64 decoding - in production, use a proper base64 library */
    std::string decoded;
    for (size_t i = 0; i < data.length(); i += 2)
    {
        std::string byte = data.substr(i, 2);
        decoded += static_cast<char>(std::stoi(byte, nullptr, 16));
    }
    return decoded;
}

void CScramMongoAuth::setError(const std::string& error) const
{
    lastError_ = error;
}

} // namespace FauxDB
