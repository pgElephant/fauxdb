/* MongoDB Count Commands Implementation */
#include "protocol/CountCommand.hpp"

#include <sstream>

namespace FauxDB
{

int64_t CountCommand::executeCountQuery(const string& collection,
                                        shared_ptr<CPostgresDatabase> database)
{
    if (!database)
    {
        return 0;
    }

    try
    {
        stringstream sql;
        sql << "SELECT COUNT(*) FROM " << collection;

        auto result = database->executeQuery(sql.str());

        if (result.success && !result.rows.empty())
        {
            try
            {
                return std::stoll(result.rows[0][0]);
            }
            catch (...)
            {
                return 0;
            }
        }
    }
    catch (const std::exception& e)
    {
        /* Query failed */
    }

    return 0;
}

vector<uint8_t>
CountDocumentsCommand::execute(const string& collection,
                               const vector<uint8_t>& buffer, ssize_t bytesRead,
                               shared_ptr<CPGConnectionPooler> connectionPooler)
{
    CBsonType response = createBaseResponse();

    int64_t count = 0;
    auto database = getConnection(connectionPooler);

    if (database)
    {
        count = executeCountQuery(collection, database);
    }

    response.addInt64("n", count);
    response.endDocument();
    return response.getDocument();
}

vector<uint8_t> EstimatedDocumentCountCommand::execute(
    const string& collection, const vector<uint8_t>& buffer, ssize_t bytesRead,
    shared_ptr<CPGConnectionPooler> connectionPooler)
{
    CBsonType response = createBaseResponse();

    int64_t count = 0;
    auto database = getConnection(connectionPooler);

    if (database)
    {
        count = executeCountQuery(collection, database);
    }

    response.addInt64("n", count);
    response.endDocument();
    return response.getDocument();
}

} /* namespace FauxDB */
