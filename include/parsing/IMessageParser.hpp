/*-------------------------------------------------------------------------
 *
 * IMessageParser.hpp
 *      Interface for message parsing in FauxDB.
 *      Abstract base class for all message parsers.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */
#pragma once
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>
using namespace std;
namespace FauxDB
{

/**
 * Message parser interface
 * Abstract base class for all message parsers
 */
class IMessageParser
{
  public:
    virtual ~IMessageParser() = default;

    /* Core parsing interface */
    virtual std::error_code
    parseMessage(const std::vector<uint8_t>& rawMessage) = 0;
    virtual bool isValidMessage() const noexcept = 0;
    virtual std::string getErrorMessage() const = 0;
    virtual void reset() noexcept = 0;
};

} /* namespace FauxDB */
