/*-------------------------------------------------------------------------
 *
 * CBasicAuth.hpp
 *      Basic client-side authentication for FauxDB to PostgreSQL connections.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------*/

#pragma once

#include "IPostgreSQLAuth.hpp"

#include <memory>
#include <string>

namespace FauxDB
{

class CBasicAuth : public IPostgreSQLAuth
{
  public:
    CBasicAuth();
    explicit CBasicAuth(const AuthConfig& config);
    ~CBasicAuth() override;

    /* IAuthentication interface implementation */
    bool initialize(const AuthConfig& config) override;
    bool authenticate(const std::string& username,
                      const std::string& password) override;
    bool isRequired() const override;
    AuthType getType() const override;
    std::string getName() const override;
    std::string getLastError() const override;
    std::string
    buildConnectionString(const std::string& host, const std::string& port,
                          const std::string& database) const override;
    bool configureSSL() override;
    bool isSSLEnabled() const override;

    /* IPostgreSQLAuth interface implementation */
    bool validateConnection(const std::string& connectionString) override;
    std::string getPostgreSQLUser() const override;
    std::string getPostgreSQLPassword() const override;
    bool testConnection() override;
    std::string getConnectionInfo() const override;
    std::string
    buildPostgreSQLConnectionString(const std::string& host,
                                    const std::string& port,
                                    const std::string& database) const override;

  private:
    AuthConfig config_;
    mutable std::string lastError_;
    bool initialized_;

    bool validateConfig() const;
    bool validateCredentials(const std::string& username,
                             const std::string& password) const;
    std::string buildSSLConnectionString() const;
    void setError(const std::string& error) const;
};

} // namespace FauxDB
