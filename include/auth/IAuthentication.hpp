/*-------------------------------------------------------------------------
 *
 * IAuthentication.hpp
 *      Base authentication interface for modular authentication system.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------*/

#pragma once

#include <string>
#include <memory>

namespace FauxDB
{

enum class AuthType
{
    BASIC = 0,
    SCRAM_SHA_1 = 1,
    SCRAM_SHA_256 = 2,
    X509 = 3,
    LDAP = 4,
    KERBEROS = 5,
    OAUTH2 = 6,
    JWT = 7
};

enum class AuthDirection
{
    MONGODB_SERVER_SIDE = 0,  /* FauxDB as MongoDB server authenticating clients */
    POSTGRESQL_CLIENT_SIDE = 1 /* FauxDB as client authenticating to PostgreSQL */
};

struct AuthConfig
{
    AuthType type;
    AuthDirection direction;
    std::string name;
    bool required;
    std::string database;
    std::string username;
    std::string password;
    bool useSSL;
    std::string sslCert;
    std::string sslKey;
    std::string sslCA;
    std::string additionalParams;

    AuthConfig()
        : type(AuthType::BASIC), direction(AuthDirection::MONGODB_SERVER_SIDE),
          name(""), required(false), database(""), username(""), password(""),
          useSSL(false), sslCert(""), sslKey(""), sslCA(""), additionalParams("")
    {
    }
};

class IAuthentication
{
  public:
    virtual ~IAuthentication() = default;

    virtual bool initialize(const AuthConfig& config) = 0;
    virtual bool authenticate(const std::string& username, const std::string& password) = 0;
    virtual bool isRequired() const = 0;
    virtual AuthType getType() const = 0;
    virtual AuthDirection getDirection() const = 0;
    virtual std::string getName() const = 0;
    virtual std::string getLastError() const = 0;
    virtual std::string buildConnectionString(const std::string& host,
                                              const std::string& port,
                                              const std::string& database) const = 0;
    virtual bool configureSSL() = 0;
    virtual bool isSSLEnabled() const = 0;
};

} // namespace FauxDB
