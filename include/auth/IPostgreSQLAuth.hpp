/*-------------------------------------------------------------------------
 *
 * IPostgreSQLAuth.hpp
 *      PostgreSQL client-side authentication interface for FauxDB to
 * PostgreSQL.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------*/

#pragma once

#include "IAuthentication.hpp"

namespace FauxDB
{

class IPostgreSQLAuth : public IAuthentication
{
  public:
    virtual ~IPostgreSQLAuth() = default;

    /* PostgreSQL client-side authentication methods */
    virtual bool validateConnection(const std::string& connectionString) = 0;
    virtual std::string getPostgreSQLUser() const = 0;
    virtual std::string getPostgreSQLPassword() const = 0;
    virtual bool testConnection() = 0;
    virtual std::string getConnectionInfo() const = 0;
    virtual std::string
    buildPostgreSQLConnectionString(const std::string& host,
                                    const std::string& port,
                                    const std::string& database) const = 0;

    /* Override direction to always be POSTGRESQL_CLIENT_SIDE */
    AuthDirection getDirection() const override
    {
        return AuthDirection::POSTGRESQL_CLIENT_SIDE;
    }
};

} // namespace FauxDB
