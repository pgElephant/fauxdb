/* MongoDB Find Command Implementation */
#include "protocol/FindCommand.hpp"
#include <sstream>

namespace FauxDB
{

vector<uint8_t>
FindCommand::execute(
    const string& collection,
    const vector<uint8_t>& buffer,
    ssize_t bytesRead,
    shared_ptr<CPGConnectionPooler> connectionPooler)
{
    CBsonType response = createBaseResponse();
    
    /* Create cursor response structure */
    CBsonType cursor;
    cursor.initialize();
    cursor.beginDocument();
    cursor.addInt64("id", 0);
    cursor.addString("ns", collection + ".collection");
    
    vector<CBsonType> documents;
    
    /* Try PostgreSQL integration */
    auto database = getConnection(connectionPooler);
    if (database)
    {
        try
        {
            stringstream sql;
            sql << "SELECT * FROM " << collection << " LIMIT " << DEFAULT_LIMIT;
            
            auto result = database->executeQuery(sql.str());
            
            if (result.success && !result.rows.empty())
            {
                for (const auto& row : result.rows)
                {
                    documents.push_back(rowToBsonDocument(row, result.columnNames));
                }
            }
        }
        catch (const std::exception& e)
        {
            /* Query failed - return empty results */
        }
    }
    
    /* Add documents to cursor firstBatch array */
    cursor.beginArray("firstBatch");
    for (const auto& doc : documents)
    {
        cursor.addArrayDocument(doc);
    }
    cursor.endArray();
    cursor.endDocument();
    
    response.addDocument("cursor", cursor);
    response.endDocument();
    
    return response.getDocument();
}

} /* namespace FauxDB */
