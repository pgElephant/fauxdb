/*-------------------------------------------------------------------------
 *
 * CAuthRegistry.hpp
 *      Authentication registry for managing different authentication types.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------*/

#pragma once

#include "IAuthentication.hpp"
#include "IMongoDBAuth.hpp"
#include "IPostgreSQLAuth.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace FauxDB
{

class CAuthRegistry
{
  public:
    CAuthRegistry();
    ~CAuthRegistry();

    /* Registry management */
    bool registerAuth(std::shared_ptr<IAuthentication> auth);
    bool unregisterAuth(const std::string& name);
    std::shared_ptr<IAuthentication> getAuth(const std::string& name);
    std::shared_ptr<IAuthentication> getAuth(AuthType type,
                                             AuthDirection direction);

    /* PostgreSQL client-side authentication management */
    bool registerPostgreSQLAuth(std::shared_ptr<IPostgreSQLAuth> auth);
    std::shared_ptr<IPostgreSQLAuth> getPostgreSQLAuth(const std::string& name);
    std::shared_ptr<IPostgreSQLAuth> getPostgreSQLAuth(AuthType type);

    /* MongoDB server-side authentication management */
    bool registerMongoDBAuth(std::shared_ptr<IMongoDBAuth> auth);
    std::shared_ptr<IMongoDBAuth> getMongoDBAuth(const std::string& name);
    std::shared_ptr<IMongoDBAuth> getMongoDBAuth(AuthType type);

    /* Authentication creation */
    std::shared_ptr<IAuthentication> createAuth(AuthType type,
                                                AuthDirection direction,
                                                const AuthConfig& config);
    std::shared_ptr<IPostgreSQLAuth>
    createPostgreSQLAuth(AuthType type, const AuthConfig& config);
    std::shared_ptr<IMongoDBAuth> createMongoDBAuth(AuthType type,
                                                    const AuthConfig& config);

    /* Registry information */
    std::vector<std::string> getRegisteredAuths() const;
    std::vector<std::string> getPostgreSQLAuths() const;
    std::vector<std::string> getMongoDBAuths() const;
    bool hasAuth(const std::string& name) const;
    bool hasPostgreSQLAuth(const std::string& name) const;
    bool hasMongoDBAuth(const std::string& name) const;

    /* Configuration management */
    bool loadFromConfig(const std::string& configKey,
                        const std::string& configValue);
    std::string getLastError() const;

    /* Default authentication setup */
    void setupDefaultAuths();

  private:
    std::map<std::string, std::shared_ptr<IAuthentication>> auths_;
    std::map<std::string, std::shared_ptr<IPostgreSQLAuth>> postgresqlAuths_;
    std::map<std::string, std::shared_ptr<IMongoDBAuth>> mongodbAuths_;
    std::map<AuthType, std::string> typeToName_;
    mutable std::string lastError_;

    void setError(const std::string& error) const;
    std::string authTypeToString(AuthType type) const;
    AuthType stringToAuthType(const std::string& typeStr) const;
    std::string authDirectionToString(AuthDirection direction) const;
    AuthDirection stringToAuthDirection(const std::string& directionStr) const;
};

} // namespace FauxDB
