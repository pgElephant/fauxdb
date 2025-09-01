/*-------------------------------------------------------------------------
 *
 * CListCollectionsCommand.cpp
 *      ListCollections command implementation for document database
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "commands/CListCollectionsCommand.hpp"

#include "database/CPGConnectionPooler.hpp"

namespace FauxDB
{

CListCollectionsCommand::CListCollectionsCommand()
{
    /* Constructor */
}

string CListCollectionsCommand::getCommandName() const
{
    return "listCollections";
}

vector<uint8_t> CListCollectionsCommand::execute(const CommandContext& context)
{
    if (context.connectionPooler && requiresDatabase())
    {
        return executeWithDatabase(context);
    }
    else
    {
        return executeWithoutDatabase(context);
    }
}

bool CListCollectionsCommand::requiresDatabase() const
{
    return true;
}

vector<uint8_t>
CListCollectionsCommand::executeWithDatabase(const CommandContext& context)
{
    /* PostgreSQL implementation for listCollections */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();

    try
    {
        auto voidConnection = context.connectionPooler->getConnection();
        if (voidConnection)
        {
            auto connection =
                std::static_pointer_cast<PGConnection>(voidConnection);

            /* Build and execute list tables SQL */
            string sql = buildListTablesSQL();
            auto result = connection->database->executeQuery(sql);

            /* Create cursor response */
            CBsonType cursor;
            cursor.initialize();
            cursor.beginDocument();
            cursor.addInt64("id", 0); /* No cursor needed for simple list */
            cursor.addString("ns",
                             context.databaseName + ".$cmd.listCollections");

            /* Create firstBatch array */
            cursor.beginArray("firstBatch");

            if (result.success)
            {
                /* Add each table as a collection */
                for (const auto& row : result.rows)
                {
                    if (!row.empty())
                    {
                        string tableName = row[0];
                        CBsonType collectionInfo =
                            createCollectionInfo(tableName, "collection");
                        cursor.addArrayDocument(collectionInfo);
                    }
                }
            }

            cursor.endArray();    /* End firstBatch */
            cursor.endDocument(); /* End cursor */

            bson.addDocument("cursor", cursor);
            bson.addDouble("ok", 1.0);

            context.connectionPooler->returnConnection(voidConnection);
        }
        else
        {
            bson.addDouble("ok", 0.0);
            bson.addString("errmsg", "database connection failed");
        }
    }
    catch (const std::exception& e)
    {
        bson.addDouble("ok", 0.0);
        bson.addString("errmsg", "listCollections operation failed");
    }

    bson.endDocument();
    return bson.getDocument();
}

vector<uint8_t>
CListCollectionsCommand::executeWithoutDatabase(const CommandContext& context)
{
    /* Simple implementation without database */
    CBsonType bson;
    bson.initialize();
    bson.beginDocument();

    /* Create cursor response with mock collections */
    CBsonType cursor;
    cursor.initialize();
    cursor.beginDocument();
    cursor.addInt64("id", 0);
    cursor.addString("ns", context.databaseName + ".$cmd.listCollections");

    cursor.beginArray("firstBatch");

    /* Add mock collections */
    CBsonType mockCollection1 = createCollectionInfo("users", "collection");
    cursor.addArrayDocument(mockCollection1);

    CBsonType mockCollection2 = createCollectionInfo("products", "collection");
    cursor.addArrayDocument(mockCollection2);

    cursor.endArray();
    cursor.endDocument();

    bson.addDocument("cursor", cursor);
    bson.addDouble("ok", 1.0);
    bson.endDocument();

    return bson.getDocument();
}

string CListCollectionsCommand::buildListTablesSQL()
{
    /* Query to list all user tables in PostgreSQL */
    return "SELECT tablename FROM pg_tables WHERE schemaname = 'public' ORDER "
           "BY tablename";
}

CBsonType
CListCollectionsCommand::createCollectionInfo(const string& collectionName,
                                              const string& collectionType)
{
    CBsonType info;
    info.initialize();
    info.beginDocument();
    info.addString("name", collectionName);
    info.addString("type", collectionType);

    /* Add options document */
    CBsonType options;
    options.initialize();
    options.beginDocument();
    options.endDocument();
    info.addDocument("options", options);

    /* Add info document with basic stats */
    CBsonType infoDoc;
    infoDoc.initialize();
    infoDoc.beginDocument();
    infoDoc.addBool("readOnly", false);
    infoDoc.endDocument();
    info.addDocument("info", infoDoc);

    info.endDocument();
    return info;
}

bool CListCollectionsCommand::extractFilter(const vector<uint8_t>& buffer,
                                            ssize_t bufferSize)
{
    /* Simple filter extraction - placeholder implementation */
    return false;
}

} // namespace FauxDB
