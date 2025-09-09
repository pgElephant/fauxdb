/*-------------------------------------------------------------------------
 *
 * CScramMongoAuth.hpp
 *      SCRAM server-side authentication for FauxDB as MongoDB server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------*/

#pragma once

#include "IMongoDBAuth.hpp"

#include <map>
#include <memory>
#include <string>

namespace FauxDB
{

enum class ScramMechanism
{
    SCRAM_SHA_1 = 0,
    SCRAM_SHA_256 = 1
};

struct ScramCredentials
{
    std::string username;
    std::string password;
    std::string salt;
    int iterationCount;
    std::string storedKey;
    std::string serverKey;
    ScramMechanism mechanism;
};

struct ScramSession
{
    std::string username;
    std::string nonce;
    std::string clientFirstMessage;
    std::string serverFirstMessage;
    std::string clientFinalMessage;
    std::string serverFinalMessage;
    ScramMechanism mechanism;
    bool completed;
};

class CScramMongoAuth : public IMongoDBAuth
{
  public:
    CScramMongoAuth();
    explicit CScramMongoAuth(const AuthConfig& config);
    ~CScramMongoAuth() override;

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

    /* IMongoDBAuth interface implementation */
    MongoAuthChallenge createChallenge(const std::string& username) override;
    MongoAuthResponse
    processResponse(const std::string& username, const std::string& password,
                    const MongoAuthChallenge& challenge) override;
    bool validateClientProof(const std::string& username,
                             const std::string& clientProof,
                             const MongoAuthChallenge& challenge) override;
    std::string
    generateServerProof(const std::string& username,
                        const std::string& clientProof,
                        const MongoAuthChallenge& challenge) override;
    bool createUser(const std::string& username,
                    const std::string& password) override;
    bool deleteUser(const std::string& username) override;
    bool updateUserPassword(const std::string& username,
                            const std::string& newPassword) override;
    bool userExists(const std::string& username) override;
    bool authenticateMongoDBClient(const std::string& username,
                                   const std::string& password) override;

    /* SCRAM-specific methods */
    ScramMechanism getMechanism() const;
    void setMechanism(ScramMechanism mechanism);
    std::string generateNonce() const;
    std::string generateSalt() const;
    std::string computeStoredKey(const std::string& password,
                                 const std::string& salt, int iterations) const;
    std::string computeServerKey(const std::string& password,
                                 const std::string& salt, int iterations) const;

  private:
    AuthConfig config_;
    ScramMechanism mechanism_;
    mutable std::string lastError_;
    bool initialized_;
    std::map<std::string, ScramCredentials> users_;
    std::map<std::string, ScramSession> sessions_;

    bool validateConfig() const;
    bool validateCredentials(const std::string& username,
                             const std::string& password) const;
    std::string hashPassword(const std::string& password,
                             const std::string& salt, int iterations) const;
    std::string hmac(const std::string& key, const std::string& data) const;
    std::string sha1(const std::string& data) const;
    std::string sha256(const std::string& data) const;
    std::string base64Encode(const std::string& data) const;
    std::string base64Decode(const std::string& data) const;
    void setError(const std::string& error) const;
};

} // namespace FauxDB
