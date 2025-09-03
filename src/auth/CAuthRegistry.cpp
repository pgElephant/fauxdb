/*-------------------------------------------------------------------------
 *
 * CAuthRegistry.cpp
 *      Authentication registry for managing different authentication types.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------*/

#include "auth/CAuthRegistry.hpp"
#include "auth/CBasicAuth.hpp"
#include "auth/CScramMongoAuth.hpp"
#include <algorithm>
#include <sstream>

namespace FauxDB
{

CAuthRegistry::CAuthRegistry()
    : lastError_("")
{
    setupDefaultAuths();
}

CAuthRegistry::~CAuthRegistry() = default;

bool CAuthRegistry::registerAuth(std::shared_ptr<IAuthentication> auth)
{
    if (!auth)
    {
        setError("Cannot register null authentication");
        return false;
    }
    
    std::string name = auth->getName();
    if (name.empty())
    {
        setError("Authentication name cannot be empty");
        return false;
    }
    
    if (hasAuth(name))
    {
        setError("Authentication already registered: " + name);
        return false;
    }
    
    auths_[name] = auth;
    typeToName_[auth->getType()] = name;
    
    /* Register in specific direction registry */
    if (auth->getDirection() == AuthDirection::MONGODB_SERVER_SIDE)
    {
        auto mongodbAuth = std::dynamic_pointer_cast<IMongoDBAuth>(auth);
        if (mongodbAuth)
        {
            mongodbAuths_[name] = mongodbAuth;
        }
    }
    else if (auth->getDirection() == AuthDirection::POSTGRESQL_CLIENT_SIDE)
    {
        auto postgresqlAuth = std::dynamic_pointer_cast<IPostgreSQLAuth>(auth);
        if (postgresqlAuth)
        {
            postgresqlAuths_[name] = postgresqlAuth;
        }
    }
    
    return true;
}

bool CAuthRegistry::unregisterAuth(const std::string& name)
{
    auto it = auths_.find(name);
    if (it == auths_.end())
    {
        setError("Authentication not found: " + name);
        return false;
    }
    
    auto auth = it->second;
    auths_.erase(it);
    
    /* Remove from type mapping */
    for (auto typeIt = typeToName_.begin(); typeIt != typeToName_.end(); ++typeIt)
    {
        if (typeIt->second == name)
        {
            typeToName_.erase(typeIt);
            break;
        }
    }
    
    /* Remove from specific direction registries */
    if (auth->getDirection() == AuthDirection::MONGODB_SERVER_SIDE)
    {
        mongodbAuths_.erase(name);
    }
    else if (auth->getDirection() == AuthDirection::POSTGRESQL_CLIENT_SIDE)
    {
        postgresqlAuths_.erase(name);
    }
    
    return true;
}

std::shared_ptr<IAuthentication> CAuthRegistry::getAuth(const std::string& name)
{
    auto it = auths_.find(name);
    if (it != auths_.end())
    {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<IAuthentication> CAuthRegistry::getAuth(AuthType type, AuthDirection direction)
{
    auto typeIt = typeToName_.find(type);
    if (typeIt != typeToName_.end())
    {
        auto auth = getAuth(typeIt->second);
        if (auth && auth->getDirection() == direction)
        {
            return auth;
        }
    }
    return nullptr;
}

/* PostgreSQL client-side authentication management */
bool CAuthRegistry::registerPostgreSQLAuth(std::shared_ptr<IPostgreSQLAuth> auth)
{
    if (!auth)
    {
        setError("Cannot register null PostgreSQL authentication");
        return false;
    }
    
    if (auth->getDirection() != AuthDirection::POSTGRESQL_CLIENT_SIDE)
    {
        setError("Authentication direction must be POSTGRESQL_CLIENT_SIDE");
        return false;
    }
    
    return registerAuth(auth);
}

std::shared_ptr<IPostgreSQLAuth> CAuthRegistry::getPostgreSQLAuth(const std::string& name)
{
    auto it = postgresqlAuths_.find(name);
    if (it != postgresqlAuths_.end())
    {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<IPostgreSQLAuth> CAuthRegistry::getPostgreSQLAuth(AuthType type)
{
    auto auth = getAuth(type, AuthDirection::POSTGRESQL_CLIENT_SIDE);
    if (auth)
    {
        return std::dynamic_pointer_cast<IPostgreSQLAuth>(auth);
    }
    return nullptr;
}

/* MongoDB server-side authentication management */
bool CAuthRegistry::registerMongoDBAuth(std::shared_ptr<IMongoDBAuth> auth)
{
    if (!auth)
    {
        setError("Cannot register null MongoDB authentication");
        return false;
    }
    
    if (auth->getDirection() != AuthDirection::MONGODB_SERVER_SIDE)
    {
        setError("Authentication direction must be MONGODB_SERVER_SIDE");
        return false;
    }
    
    return registerAuth(auth);
}

std::shared_ptr<IMongoDBAuth> CAuthRegistry::getMongoDBAuth(const std::string& name)
{
    auto it = mongodbAuths_.find(name);
    if (it != mongodbAuths_.end())
    {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<IMongoDBAuth> CAuthRegistry::getMongoDBAuth(AuthType type)
{
    auto auth = getAuth(type, AuthDirection::MONGODB_SERVER_SIDE);
    if (auth)
    {
        return std::dynamic_pointer_cast<IMongoDBAuth>(auth);
    }
    return nullptr;
}

/* Authentication creation */
std::shared_ptr<IAuthentication> CAuthRegistry::createAuth(AuthType type, AuthDirection direction, const AuthConfig& config)
{
    if (direction == AuthDirection::POSTGRESQL_CLIENT_SIDE)
    {
        return createPostgreSQLAuth(type, config);
    }
    else if (direction == AuthDirection::MONGODB_SERVER_SIDE)
    {
        return createMongoDBAuth(type, config);
    }
    
    setError("Unsupported authentication direction");
    return nullptr;
}

std::shared_ptr<IPostgreSQLAuth> CAuthRegistry::createPostgreSQLAuth(AuthType type, const AuthConfig& config)
{
    AuthConfig authConfig = config;
    authConfig.type = type;
    authConfig.direction = AuthDirection::POSTGRESQL_CLIENT_SIDE;
    
    switch (type)
    {
        case AuthType::BASIC:
        {
            auto auth = std::make_shared<CBasicAuth>(authConfig);
            if (auth->initialize(authConfig))
            {
                return auth;
            }
            break;
        }
        default:
            setError("Unsupported PostgreSQL authentication type: " + authTypeToString(type));
            break;
    }
    
    return nullptr;
}

std::shared_ptr<IMongoDBAuth> CAuthRegistry::createMongoDBAuth(AuthType type, const AuthConfig& config)
{
    AuthConfig authConfig = config;
    authConfig.type = type;
    authConfig.direction = AuthDirection::MONGODB_SERVER_SIDE;
    
    switch (type)
    {
        case AuthType::SCRAM_SHA_1:
        case AuthType::SCRAM_SHA_256:
        {
            auto auth = std::make_shared<CScramMongoAuth>(authConfig);
            if (auth->initialize(authConfig))
            {
                return auth;
            }
            break;
        }
        default:
            setError("Unsupported MongoDB authentication type: " + authTypeToString(type));
            break;
    }
    
    return nullptr;
}

/* Registry information */
std::vector<std::string> CAuthRegistry::getRegisteredAuths() const
{
    std::vector<std::string> names;
    for (const auto& pair : auths_)
    {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> CAuthRegistry::getPostgreSQLAuths() const
{
    std::vector<std::string> names;
    for (const auto& pair : postgresqlAuths_)
    {
        names.push_back(pair.first);
    }
    return names;
}

std::vector<std::string> CAuthRegistry::getMongoDBAuths() const
{
    std::vector<std::string> names;
    for (const auto& pair : mongodbAuths_)
    {
        names.push_back(pair.first);
    }
    return names;
}

bool CAuthRegistry::hasAuth(const std::string& name) const
{
    return auths_.find(name) != auths_.end();
}

bool CAuthRegistry::hasPostgreSQLAuth(const std::string& name) const
{
    return postgresqlAuths_.find(name) != postgresqlAuths_.end();
}

bool CAuthRegistry::hasMongoDBAuth(const std::string& name) const
{
    return mongodbAuths_.find(name) != mongodbAuths_.end();
}

/* Configuration management */
bool CAuthRegistry::loadFromConfig(const std::string& configKey, const std::string& configValue)
{
    /* Parse configuration and create appropriate authentication */
    /* This is a simplified implementation - in production, this would
     * parse the configuration more thoroughly */
    
    if (configKey.find("mongodb_server_auth") != std::string::npos)
    {
        /* Handle MongoDB server-side authentication configuration */
        return true;
    }
    else if (configKey.find("postgresql_client_auth") != std::string::npos)
    {
        /* Handle PostgreSQL client-side authentication configuration */
        return true;
    }
    
    setError("Unknown configuration key: " + configKey);
    return false;
}

std::string CAuthRegistry::getLastError() const
{
    return lastError_;
}

/* Default authentication setup */
void CAuthRegistry::setupDefaultAuths()
{
    /* Create default PostgreSQL client-side basic authentication */
    AuthConfig basicConfig;
    basicConfig.type = AuthType::BASIC;
    basicConfig.direction = AuthDirection::POSTGRESQL_CLIENT_SIDE;
    basicConfig.name = "Default PostgreSQL Basic Auth";
    basicConfig.required = false;
    basicConfig.database = "fauxdb";
    
    auto basicAuth = createPostgreSQLAuth(AuthType::BASIC, basicConfig);
    if (basicAuth)
    {
        registerAuth(basicAuth);
    }
    
    /* Create default MongoDB server-side SCRAM authentication */
    AuthConfig scramConfig;
    scramConfig.type = AuthType::SCRAM_SHA_256;
    scramConfig.direction = AuthDirection::MONGODB_SERVER_SIDE;
    scramConfig.name = "Default MongoDB SCRAM Auth";
    scramConfig.required = false;
    scramConfig.database = "admin";
    
    auto scramAuth = createMongoDBAuth(AuthType::SCRAM_SHA_256, scramConfig);
    if (scramAuth)
    {
        registerAuth(scramAuth);
    }
}

void CAuthRegistry::setError(const std::string& error) const
{
    lastError_ = error;
}

std::string CAuthRegistry::authTypeToString(AuthType type) const
{
    switch (type)
    {
        case AuthType::BASIC: return "BASIC";
        case AuthType::SCRAM_SHA_1: return "SCRAM_SHA_1";
        case AuthType::SCRAM_SHA_256: return "SCRAM_SHA_256";
        case AuthType::X509: return "X509";
        case AuthType::LDAP: return "LDAP";
        case AuthType::KERBEROS: return "KERBEROS";
        case AuthType::OAUTH2: return "OAUTH2";
        case AuthType::JWT: return "JWT";
        default: return "UNKNOWN";
    }
}

AuthType CAuthRegistry::stringToAuthType(const std::string& typeStr) const
{
    if (typeStr == "basic") return AuthType::BASIC;
    if (typeStr == "scram-sha-1") return AuthType::SCRAM_SHA_1;
    if (typeStr == "scram-sha-256") return AuthType::SCRAM_SHA_256;
    if (typeStr == "x509") return AuthType::X509;
    if (typeStr == "ldap") return AuthType::LDAP;
    if (typeStr == "kerberos") return AuthType::KERBEROS;
    if (typeStr == "oauth2") return AuthType::OAUTH2;
    if (typeStr == "jwt") return AuthType::JWT;
    return AuthType::BASIC;
}

std::string CAuthRegistry::authDirectionToString(AuthDirection direction) const
{
    switch (direction)
    {
        case AuthDirection::MONGODB_SERVER_SIDE: return "MONGODB_SERVER_SIDE";
        case AuthDirection::POSTGRESQL_CLIENT_SIDE: return "POSTGRESQL_CLIENT_SIDE";
        default: return "UNKNOWN";
    }
}

AuthDirection CAuthRegistry::stringToAuthDirection(const std::string& directionStr) const
{
    if (directionStr == "mongodb_server_side") return AuthDirection::MONGODB_SERVER_SIDE;
    if (directionStr == "postgresql_client_side") return AuthDirection::POSTGRESQL_CLIENT_SIDE;
    return AuthDirection::MONGODB_SERVER_SIDE;
}

} // namespace FauxDB
