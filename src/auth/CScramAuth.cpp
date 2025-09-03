/*-------------------------------------------------------------------------
 *
 * CScramAuth.cpp
 *      SCRAM-SHA-1 and SCRAM-SHA-256 authentication implementation
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "auth/CScramAuth.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <libpq-fe.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <sstream>

using namespace std;

namespace FauxDB
{

CScramAuth::CScramAuth(shared_ptr<CPostgresDatabase> database)
    : database_(database),
      randomGenerator_(chrono::steady_clock::now().time_since_epoch().count())
{
    initializeAuthTables();
}

bool CScramAuth::createUser(const string& username, const string& password,
                            const string& pgUsername, const string& pgPassword,
                            ScramMechanism mechanism)
{
    if (!database_ || userExists(username))
    {
        return false;
    }

    // Validate PostgreSQL user exists (if password provided, validate it too)
    if (!pgPassword.empty() && !validatePostgreSQLUser(pgUsername, pgPassword))
    {
        return false;
    }

    ScramCredentials credentials;
    credentials.username = username;
    credentials.pgUsername = pgUsername;
    credentials.pgPassword =
        pgPassword; // Store if needed for connection pooling
    credentials.salt = generateSalt();
    credentials.iterationCount = 4096; // MongoDB default
    credentials.mechanism = mechanism;

    // Generate SCRAM keys
    int keyLength = (mechanism == ScramMechanism::SCRAM_SHA_256) ? 32 : 20;
    auto saltedPassword =
        pbkdf2(password, credentials.salt, credentials.iterationCount,
               keyLength, mechanism);

    string clientKeyStr = "Client Key";
    string serverKeyStr = "Server Key";

    auto clientKey = hmac(saltedPassword, clientKeyStr, mechanism);
    auto serverKey = hmac(saltedPassword, serverKeyStr, mechanism);

    // StoredKey = H(ClientKey)
    const EVP_MD* md = (mechanism == ScramMechanism::SCRAM_SHA_256)
                           ? EVP_sha256()
                           : EVP_sha1();
    vector<uint8_t> storedKey(EVP_MD_size(md));
    unsigned int storedKeyLen;
    EVP_Digest(clientKey.data(), clientKey.size(), storedKey.data(),
               &storedKeyLen, md, nullptr);
    storedKey.resize(storedKeyLen);

    credentials.storedKey = base64Encode(storedKey);
    credentials.serverKey = base64Encode(serverKey);

    return storeUserCredentials(credentials);
}

bool CScramAuth::deleteUser(const string& username)
{
    if (!database_)
    {
        return false;
    }

    string sql = "DELETE FROM fauxdb_users WHERE username = $1";
    vector<string> params = {username};

    try
    {
        auto result = database_->executeQuery(sql, params);
        return result.success;
    }
    catch (const exception&)
    {
        return false;
    }
}

bool CScramAuth::updateUserPassword(const string& username,
                                    const string& newPassword,
                                    ScramMechanism mechanism)
{
    if (!userExists(username))
    {
        return false;
    }

    // Get the existing PostgreSQL username mapping
    string pgUsername = getPostgreSQLUsername(username);
    if (pgUsername.empty())
    {
        return false;
    }

    deleteUser(username);
    return createUser(username, newPassword, pgUsername, "", mechanism);
}

bool CScramAuth::userExists(const string& username)
{
    if (!database_)
    {
        return false;
    }

    string sql = "SELECT COUNT(*) FROM fauxdb_users WHERE username = $1";
    vector<string> params = {username};

    try
    {
        auto result = database_->executeQuery(sql, params);
        if (result.success && !result.rows.empty() && !result.rows[0].empty())
        {
            return stoi(result.rows[0][0]) > 0;
        }
    }
    catch (const exception&)
    {
        /* Fall through to return false */
    }

    return false;
}

bool CScramAuth::validatePostgreSQLUser(const string& pgUsername,
                                        const string& pgPassword)
{
    if (!database_)
    {
        return false;
    }

    // Try to create a connection with the provided PostgreSQL credentials
    // This validates that the PostgreSQL user exists and password is correct
    try
    {
        // For now, use a simple connection test approach
        // In a real implementation, you'd get these from database config
        string connString =
            "host=localhost port=5432 dbname=fauxdb user=" + pgUsername;

        if (!pgPassword.empty())
        {
            connString += " password=" + pgPassword;
        }

        // Quick connection test
        PGconn* testConn = PQconnectdb(connString.c_str());
        bool isValid = (PQstatus(testConn) == CONNECTION_OK);
        PQfinish(testConn);

        return isValid;
    }
    catch (const exception&)
    {
        return false;
    }
}

string CScramAuth::getPostgreSQLUsername(const string& username)
{
    if (!database_)
    {
        return "";
    }

    string sql = "SELECT pg_username FROM fauxdb_users WHERE username = $1";
    vector<string> params = {username};

    try
    {
        auto result = database_->executeQuery(sql, params);
        if (result.success && !result.rows.empty() && !result.rows[0].empty())
        {
            return result.rows[0][0];
        }
    }
    catch (const exception&)
    {
        /* Return empty string on error */
    }

    return "";
}

string CScramAuth::startAuthentication(const string& username,
                                       const string& clientFirstMessage,
                                       ScramMechanism mechanism)
{
    string clientNonce;
    string parsedUsername;

    string gs2Header = parseClientFirstMessage(clientFirstMessage,
                                               parsedUsername, clientNonce);

    if (parsedUsername != username || !userExists(username))
    {
        return ""; // Authentication failure
    }

    ScramCredentials credentials = loadUserCredentials(username);
    if (credentials.mechanism != mechanism)
    {
        return ""; // Mechanism mismatch
    }

    // Create session
    ScramSession session;
    session.sessionId = generateNonce(); // Use as session ID
    session.username = username;
    session.clientNonce = clientNonce;
    session.serverNonce = generateNonce();
    session.salt = credentials.salt;
    session.iterationCount = credentials.iterationCount;
    session.mechanism = mechanism;
    session.authenticated = false;

    // Build auth message (without server final)
    string clientFirstBare = clientFirstMessage.substr(gs2Header.length());
    session.authMessage =
        clientFirstBare + "," + createServerFirstMessage(session);

    sessions_[session.sessionId] = session;

    return session.sessionId + ":" + createServerFirstMessage(session);
}

string CScramAuth::continueAuthentication(const string& sessionId,
                                          const string& clientFinalMessage)
{
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end())
    {
        return createServerFinalMessage(ScramSession{}, false,
                                        "invalid session");
    }

    ScramSession& session = it->second;

    string channelBinding, nonce, proof;
    string clientFinalWithoutProof = parseClientFinalMessage(
        clientFinalMessage, channelBinding, nonce, proof);

    // Verify nonce
    if (nonce != session.clientNonce + session.serverNonce)
    {
        clearSession(sessionId);
        return createServerFinalMessage(session, false, "invalid nonce");
    }

    // Complete auth message
    session.authMessage += "," + clientFinalWithoutProof;

    // Verify client proof
    if (verifyClientProof(session, proof))
    {
        session.authenticated = true;
        return createServerFinalMessage(session, true);
    }
    else
    {
        clearSession(sessionId);
        return createServerFinalMessage(session, false,
                                        "authentication failed");
    }
}

bool CScramAuth::isAuthenticated(const string& sessionId)
{
    auto it = sessions_.find(sessionId);
    return it != sessions_.end() && it->second.authenticated;
}

void CScramAuth::clearSession(const string& sessionId)
{
    sessions_.erase(sessionId);
}

string CScramAuth::generateNonce()
{
    vector<uint8_t> nonce(24);
    RAND_bytes(nonce.data(), nonce.size());
    return base64Encode(nonce);
}

string CScramAuth::generateSalt()
{
    vector<uint8_t> salt(16);
    RAND_bytes(salt.data(), salt.size());
    return base64Encode(salt);
}

vector<uint8_t> CScramAuth::pbkdf2(const string& password, const string& salt,
                                   int iterations, int keyLength,
                                   ScramMechanism mechanism)
{
    const EVP_MD* md = (mechanism == ScramMechanism::SCRAM_SHA_256)
                           ? EVP_sha256()
                           : EVP_sha1();
    vector<uint8_t> saltBytes = base64Decode(salt);
    vector<uint8_t> key(keyLength);

    PKCS5_PBKDF2_HMAC(password.c_str(), password.length(), saltBytes.data(),
                      saltBytes.size(), iterations, md, keyLength, key.data());

    return key;
}

vector<uint8_t> CScramAuth::hmac(const vector<uint8_t>& key,
                                 const string& message,
                                 ScramMechanism mechanism)
{
    const EVP_MD* md = (mechanism == ScramMechanism::SCRAM_SHA_256)
                           ? EVP_sha256()
                           : EVP_sha1();
    vector<uint8_t> result(EVP_MD_size(md));
    unsigned int resultLen;

    HMAC(md, key.data(), key.size(),
         reinterpret_cast<const unsigned char*>(message.c_str()),
         message.length(), result.data(), &resultLen);

    result.resize(resultLen);
    return result;
}

string CScramAuth::base64Encode(const vector<uint8_t>& data)
{
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_push(b64, bio);

    BIO_write(b64, data.data(), data.size());
    BIO_flush(b64);

    BUF_MEM* bufferPtr;
    BIO_get_mem_ptr(b64, &bufferPtr);
    string result(bufferPtr->data, bufferPtr->length);

    BIO_free_all(b64);
    return result;
}

vector<uint8_t> CScramAuth::base64Decode(const string& encoded)
{
    BIO* bio = BIO_new_mem_buf(encoded.data(), encoded.length());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_push(b64, bio);

    vector<uint8_t> result(encoded.length());
    int decodedLength = BIO_read(b64, result.data(), result.size());

    BIO_free_all(b64);

    if (decodedLength > 0)
    {
        result.resize(decodedLength);
    }
    else
    {
        result.clear();
    }

    return result;
}

ScramCredentials CScramAuth::loadUserCredentials(const string& username)
{
    ScramCredentials credentials;

    if (!database_)
    {
        return credentials;
    }

    string sql =
        "SELECT username, salt, iteration_count, stored_key, server_key, "
        "mechanism, pg_username FROM fauxdb_users WHERE username = $1";
    vector<string> params = {username};

    try
    {
        auto result = database_->executeQuery(sql, params);
        if (result.success && !result.rows.empty())
        {
            const auto& row = result.rows[0];
            credentials.username = row[0];
            credentials.salt = row[1];
            credentials.iterationCount = stoi(row[2]);
            credentials.storedKey = row[3];
            credentials.serverKey = row[4];
            credentials.mechanism = (row[5] == "SCRAM-SHA-256")
                                        ? ScramMechanism::SCRAM_SHA_256
                                        : ScramMechanism::SCRAM_SHA_1;
            credentials.pgUsername = row[6];
        }
    }
    catch (const exception&)
    {
        /* Return empty credentials on error */
    }

    return credentials;
}

bool CScramAuth::storeUserCredentials(const ScramCredentials& credentials)
{
    if (!database_)
    {
        return false;
    }

    string mechanismStr =
        (credentials.mechanism == ScramMechanism::SCRAM_SHA_256)
            ? "SCRAM-SHA-256"
            : "SCRAM-SHA-1";

    string sql = "INSERT INTO fauxdb_users (username, salt, iteration_count, "
                 "stored_key, server_key, mechanism, pg_username, pg_password) "
                 "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)";
    vector<string> params = {credentials.username,
                             credentials.salt,
                             to_string(credentials.iterationCount),
                             credentials.storedKey,
                             credentials.serverKey,
                             mechanismStr,
                             credentials.pgUsername,
                             credentials.pgPassword};

    try
    {
        auto result = database_->executeQuery(sql, params);
        return result.success;
    }
    catch (const exception&)
    {
        return false;
    }
}

string CScramAuth::parseClientFirstMessage(const string& message,
                                           string& username, string& nonce)
{
    // Format: gs2-header,username,nonce
    // gs2-header is typically "n,,"

    size_t pos = 0;
    size_t commaPos = message.find(',', pos);
    if (commaPos == string::npos)
        return "";

    string gs2Header = message.substr(0, commaPos + 1);

    // Skip second comma
    commaPos = message.find(',', commaPos + 1);
    if (commaPos == string::npos)
        return "";
    gs2Header = message.substr(0, commaPos + 1);

    // Parse bare message
    string bareMessage = message.substr(commaPos + 1);

    // Extract username (n=username)
    pos = bareMessage.find("n=");
    if (pos == string::npos)
        return "";
    pos += 2;

    size_t endPos = bareMessage.find(',', pos);
    if (endPos == string::npos)
        return "";

    username = bareMessage.substr(pos, endPos - pos);

    // Extract nonce (r=nonce)
    pos = bareMessage.find("r=", endPos);
    if (pos == string::npos)
        return "";
    pos += 2;

    endPos = bareMessage.find(',', pos);
    if (endPos == string::npos)
    {
        nonce = bareMessage.substr(pos);
    }
    else
    {
        nonce = bareMessage.substr(pos, endPos - pos);
    }

    return gs2Header;
}

string CScramAuth::parseClientFinalMessage(const string& message,
                                           string& channelBinding,
                                           string& nonce, string& proof)
{
    // Format: channel-binding,nonce,proof
    size_t pos = 0;

    // Channel binding
    size_t commaPos = message.find(',', pos);
    if (commaPos == string::npos)
        return "";
    channelBinding = message.substr(pos, commaPos - pos);

    pos = commaPos + 1;

    // Extract nonce (r=nonce)
    size_t noncePos = message.find("r=", pos);
    if (noncePos == string::npos)
        return "";
    noncePos += 2;

    commaPos = message.find(',', noncePos);
    if (commaPos == string::npos)
        return "";
    nonce = message.substr(noncePos, commaPos - noncePos);

    // Extract proof (p=proof)
    size_t proofPos = message.find("p=", commaPos);
    if (proofPos == string::npos)
        return "";
    proofPos += 2;

    proof = message.substr(proofPos);

    // Return message without proof for auth message
    return message.substr(0, proofPos - 2);
}

string CScramAuth::createServerFirstMessage(const ScramSession& session)
{
    return "r=" + session.clientNonce + session.serverNonce +
           ",s=" + session.salt + ",i=" + to_string(session.iterationCount);
}

string CScramAuth::createServerFinalMessage(const ScramSession& session,
                                            bool success, const string& error)
{
    if (!success)
    {
        return "e=" + (error.empty() ? "authentication failed" : error);
    }

    // Calculate server signature
    ScramCredentials credentials = loadUserCredentials(session.username);
    vector<uint8_t> serverKey = base64Decode(credentials.serverKey);
    vector<uint8_t> serverSignature =
        hmac(serverKey, session.authMessage, session.mechanism);

    return "v=" + base64Encode(serverSignature);
}

bool CScramAuth::verifyClientProof(const ScramSession& session,
                                   const string& proof)
{
    ScramCredentials credentials = loadUserCredentials(session.username);
    vector<uint8_t> storedKey = base64Decode(credentials.storedKey);
    vector<uint8_t> clientSignature =
        hmac(storedKey, session.authMessage, session.mechanism);
    vector<uint8_t> clientProofBytes = base64Decode(proof);

    // ClientKey = ClientProof XOR ClientSignature
    if (clientProofBytes.size() != clientSignature.size())
    {
        return false;
    }

    vector<uint8_t> clientKey(clientProofBytes.size());
    for (size_t i = 0; i < clientProofBytes.size(); ++i)
    {
        clientKey[i] = clientProofBytes[i] ^ clientSignature[i];
    }

    // Verify StoredKey = H(ClientKey)
    const EVP_MD* md = (session.mechanism == ScramMechanism::SCRAM_SHA_256)
                           ? EVP_sha256()
                           : EVP_sha1();
    vector<uint8_t> computedStoredKey(EVP_MD_size(md));
    unsigned int storedKeyLen;
    EVP_Digest(clientKey.data(), clientKey.size(), computedStoredKey.data(),
               &storedKeyLen, md, nullptr);
    computedStoredKey.resize(storedKeyLen);

    return computedStoredKey == storedKey;
}

void CScramAuth::initializeAuthTables()
{
    if (!database_)
    {
        return;
    }

    string sql = "CREATE TABLE IF NOT EXISTS fauxdb_users ("
                 "id SERIAL PRIMARY KEY, "
                 "username VARCHAR(255) UNIQUE NOT NULL, "
                 "pg_username VARCHAR(255) NOT NULL, "
                 "pg_password TEXT, "
                 "salt TEXT NOT NULL, "
                 "iteration_count INTEGER NOT NULL, "
                 "stored_key TEXT NOT NULL, "
                 "server_key TEXT NOT NULL, "
                 "mechanism VARCHAR(20) NOT NULL, "
                 "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP, "
                 "last_login TIMESTAMP, "
                 "UNIQUE(username), "
                 "FOREIGN KEY (pg_username) REFERENCES pg_user(usename) ON "
                 "DELETE CASCADE"
                 ")";

    try
    {
        database_->executeQuery(sql);
    }
    catch (const exception&)
    {
        /* Table creation failed - may already exist */
    }
}

} // namespace FauxDB
