/*-------------------------------------------------------------------------
 *
 * CAuthCommand.hpp
 *      MongoDB authentication command implementation
 *      Supports SCRAM-SHA-1 and SCRAM-SHA-256
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include "../../auth/CScramAuth.hpp"
#include "CBaseCommand.hpp"

namespace FauxDB
{

class CAuthCommand : public CBaseCommand
{
  public:
    CAuthCommand();
    virtual ~CAuthCommand() = default;

    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

  private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);

    // Authentication handlers
    vector<uint8_t> handleSaslStart(const CommandContext& context);
    vector<uint8_t> handleSaslContinue(const CommandContext& context);

    // SCRAM helpers
    ScramMechanism parseScramMechanism(const string& mechanismName);
    string extractPayload(const vector<uint8_t>& buffer, ssize_t bufferSize);
    int extractConversationId(const vector<uint8_t>& buffer,
                              ssize_t bufferSize);

    static shared_ptr<CScramAuth> scramAuth_;
};

class CSaslStartCommand : public CBaseCommand
{
  public:
    CSaslStartCommand();
    virtual ~CSaslStartCommand() = default;

    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

  private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);
};

class CSaslContinueCommand : public CBaseCommand
{
  public:
    CSaslContinueCommand();
    virtual ~CSaslContinueCommand() = default;

    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

  private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);
};

class CCreateUserCommand : public CBaseCommand
{
  public:
    CCreateUserCommand();
    virtual ~CCreateUserCommand() = default;

    string getCommandName() const override;
    vector<uint8_t> execute(const CommandContext& context) override;
    bool requiresDatabase() const override;

  private:
    vector<uint8_t> executeWithDatabase(const CommandContext& context);
    vector<uint8_t> executeWithoutDatabase(const CommandContext& context);

    string extractUsername(const vector<uint8_t>& buffer, ssize_t bufferSize);
    string extractPassword(const vector<uint8_t>& buffer, ssize_t bufferSize);
    vector<string> extractRoles(const vector<uint8_t>& buffer,
                                ssize_t bufferSize);
};

} // namespace FauxDB
