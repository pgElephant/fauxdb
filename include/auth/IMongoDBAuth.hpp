/*-------------------------------------------------------------------------
 *
 * IMongoDBAuth.hpp
 *      MongoDB server-side authentication interface for FauxDB as MongoDB
 * server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------*/

#pragma once

#include "IAuthentication.hpp"

#include <map>
#include <vector>

namespace FauxDB
{

struct MongoAuthChallenge
{
    std::string nonce;
    std::string salt;
    int iterationCount;
    std::string serverKey;
    std::string storedKey;
    std::map<std::string, std::string> additionalData;
};

struct MongoAuthResponse
{
    bool success;
    std::string message;
    std::string proof;
    std::map<std::string, std::string> additionalData;
};

class IMongoDBAuth : public IAuthentication
{
  public:
    virtual ~IMongoDBAuth() = default;

    /* MongoDB server-side authentication methods */
    virtual MongoAuthChallenge createChallenge(const std::string& username) = 0;
    virtual MongoAuthResponse
    processResponse(const std::string& username, const std::string& password,
                    const MongoAuthChallenge& challenge) = 0;
    virtual bool validateClientProof(const std::string& username,
                                     const std::string& clientProof,
                                     const MongoAuthChallenge& challenge) = 0;
    virtual std::string
    generateServerProof(const std::string& username,
                        const std::string& clientProof,
                        const MongoAuthChallenge& challenge) = 0;
    virtual bool createUser(const std::string& username,
                            const std::string& password) = 0;
    virtual bool deleteUser(const std::string& username) = 0;
    virtual bool updateUserPassword(const std::string& username,
                                    const std::string& newPassword) = 0;
    virtual bool userExists(const std::string& username) = 0;
    virtual bool authenticateMongoDBClient(const std::string& username,
                                           const std::string& password) = 0;

    /* Override direction to always be MONGODB_SERVER_SIDE */
    AuthDirection getDirection() const override
    {
        return AuthDirection::MONGODB_SERVER_SIDE;
    }
};

} // namespace FauxDB
