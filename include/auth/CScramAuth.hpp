/*-------------------------------------------------------------------------
 *
 * CScramAuth.hpp
 *      SCRAM-SHA-1 and SCRAM-SHA-256 authentication implementation
 *      Compatible with MongoDB authentication protocol
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "../CTypes.hpp"
#include "../database/CPostgresDatabase.hpp"

#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace FauxDB
{

enum class ScramMechanism
{
    SCRAM_SHA_1,
    SCRAM_SHA_256
};

struct ScramCredentials
{
    std::string username;
    std::string salt;
    int iterationCount;
    std::string storedKey;
    std::string serverKey;
    ScramMechanism mechanism;
    std::string pgUsername;
    std::string pgPassword;
};

struct ScramSession
{
    std::string sessionId;
    std::string username;
    std::string clientNonce;
    std::string serverNonce;
    std::string salt;
    int iterationCount;
    std::string authMessage;
    ScramMechanism mechanism;
    bool authenticated;
};

class CScramAuth
{
  public:
    CScramAuth(std::shared_ptr<CPostgresDatabase> database);
    ~CScramAuth() = default;


    bool createUser(const std::string& username,
                    const std::string& password,
                    const std::string& pgUsername,
                    const std::string& pgPassword = "",
                    ScramMechanism mechanism = ScramMechanism::SCRAM_SHA_256);
    bool deleteUser(const std::string& username);
    bool updateUserPassword(
        const std::string& username, const std::string& newPassword,
        ScramMechanism mechanism = ScramMechanism::SCRAM_SHA_256);
    bool userExists(const std::string& username);
    bool validatePostgreSQLUser(const std::string& pgUsername,
                                const std::string& pgPassword);
    std::string getPostgreSQLUsername(const std::string& username);


    std::string startAuthentication(const std::string& username,
                                    const std::string& clientFirstMessage,
                                    ScramMechanism mechanism);
    std::string continueAuthentication(const std::string& sessionId,
                                       const std::string& clientFinalMessage);
    bool isAuthenticated(const std::string& sessionId);
    void clearSession(const std::string& sessionId);


    static std::string generateNonce();
    static std::string generateSalt();
    static std::vector<uint8_t> pbkdf2(const std::string& password,
                                       const std::string& salt, int iterations,
                                       int keyLength, ScramMechanism mechanism);
    static std::vector<uint8_t> hmac(const std::vector<uint8_t>& key,
                                     const std::string& message,
                                     ScramMechanism mechanism);
    static std::string base64Encode(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> base64Decode(const std::string& encoded);

  private:
    std::shared_ptr<CPostgresDatabase> database_;
    std::unordered_map<std::string, ScramSession> sessions_;
    std::mt19937 randomGenerator_;

    // Internal helpers
    ScramCredentials loadUserCredentials(const std::string& username);
    bool storeUserCredentials(const ScramCredentials& credentials);
    std::string parseClientFirstMessage(const std::string& message,
                                        std::string& username,
                                        std::string& nonce);
    std::string parseClientFinalMessage(const std::string& message,
                                        std::string& channelBinding,
                                        std::string& nonce, std::string& proof);
    std::string createServerFirstMessage(const ScramSession& session);
    std::string createServerFinalMessage(const ScramSession& session,
                                         bool success,
                                         const std::string& error = "");
    bool verifyClientProof(const ScramSession& session,
                           const std::string& clientProof);
    void initializeAuthTables();
};

} // namespace FauxDB
