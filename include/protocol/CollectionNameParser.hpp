/*-------------------------------------------------------------------------
 *
 * CollectionNameParser.hpp
 *      Collection name parsing utilities for MongoDB commands.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

/* Collection Name Parser Utility */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

using namespace std;

namespace FauxDB
{

/**
 * @brief Utility class for parsing collection names from MongoDB requests
 */
class CollectionNameParser
{
  public:
    /**
     * @brief Extract collection name from BSON buffer
     * @param buffer BSON buffer
     * @param bytesRead Number of bytes read
     * @param commandName Known command name (for optimization)
     * @return Collection name or empty string if not found
     */
    static string extractCollectionName(const vector<uint8_t>& buffer,
                                        ssize_t bytesRead,
                                        const string& commandName = "");

  private:
    /**
     * @brief Parse BSON document to find collection name
     * @param buffer BSON buffer
     * @param offset Current offset in buffer
     * @param commandName Known command name
     * @return Collection name or empty string
     */
    static string parseBsonForCollection(const vector<uint8_t>& buffer,
                                         size_t offset,
                                         const string& commandName);

    /**
     * @brief Read BSON string from buffer
     * @param buffer BSON buffer
     * @param offset Current offset
     * @param stringLength Length of string
     * @return String value
     */
    static string readBsonString(const vector<uint8_t>& buffer, size_t offset,
                                 uint32_t stringLength);

    /**
     * @brief Check if field name indicates collection name
     * @param fieldName Field name to check
     * @param commandName Command name for context
     * @return true if field contains collection name
     */
    static bool isCollectionField(const string& fieldName,
                                  const string& commandName);
};

} /* namespace FauxDB */
