/*-------------------------------------------------------------------------
 *
 * client_regression.cpp
 *      Implementation for FauxDB.
 *      Part of the FauxDB MongoDB-compatible database server.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */


#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <mongoc/mongoc.h>
#include <nlohmann/json.hpp>
#include <signal.h>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>

using json = nlohmann::json;

namespace FauxDB
{
namespace Regression
{

class ClientRegressionTest : public ::testing::Test
{
  protected:
    mongoc_client_t* client;
    mongoc_database_t* database;
    mongoc_collection_t* collection;
    pid_t server_pid;
    std::string test_data_dir;
    std::string expected_results_dir;

    void SetUp() override
    {

        mongoc_init();

        test_data_dir = "../regression/scripts/";
        expected_results_dir = "../regression/expected/";

        startFauxdbServer();

        std::this_thread::sleep_for(std::chrono::seconds(2));

        connectToServer();
    }

    void TearDown() override
    {

        if (collection)
        {
            mongoc_collection_destroy(collection);
        }
        if (database)
        {
            mongoc_database_destroy(database);
        }
        if (client)
        {
            mongoc_client_destroy(client);
        }

        stopFauxdbServer();

        mongoc_cleanup();
    }

  private:
    void startFauxdbServer()
    {
        server_pid = fork();
        if (server_pid == 0)
        {

            execl("../build/fauxdb", "fauxdb", nullptr);
            exit(1);
        }
        else if (server_pid < 0)
        {
            FAIL() << "Failed to fork fauxdb server process";
        }
    }

    void stopFauxdbServer()
    {
        if (server_pid > 0)
        {
            kill(server_pid, SIGTERM);
            waitpid(server_pid, nullptr, 0);
        }
    }

    void connectToServer()
    {

        client = mongoc_client_new("mongodb:
		ASSERT_NE(client, nullptr) << "Failed to create Document client";


		database = mongoc_client_get_database(client, "testdb");
		ASSERT_NE(database, nullptr) << "Failed to get database handle";


		collection = mongoc_client_get_collection(client, "testdb", "users");
		ASSERT_NE(collection, nullptr) << "Failed to get collection handle";
    }

  protected:
    void executeAndCompare(const std::string& testName, const bson_t* command,
                           const std::string& expectedFile)
    {
        bson_t reply;
        bson_error_t error;

        bool success = mongoc_database_command_simple(database, command,
                                                      nullptr, &reply, &error);

        char* result_str = bson_as_canonical_extended_json(&reply, nullptr);
        std::string actual_result(result_str);
        bson_free(result_str);

        std::string expected_result = loadExpectedResult(expectedFile);

        compareResults(testName, actual_result, expected_result, success);

        bson_destroy(&reply);
    }

    void insertAndCompare(const std::string& testName, const bson_t* document,
                          const std::string& expectedFile)
    {
        bson_error_t error;

        bool success = mongoc_collection_insert_one(collection, document,
                                                    nullptr, nullptr, &error);

        json result;
        result["success"] = success;
        result["insertedId"] = success ? "ObjectId(...)" : nullptr;
        if (!success)
        {
            result["error"] = error.message;
        }

        std::string actual_result = result.dump(2);

        std::string expected_result = loadExpectedResult(expectedFile);

        compareResults(testName, actual_result, expected_result, success);
    }

    void findAndCompare(const std::string& testName, const bson_t* filter,
                        const std::string& expectedFile)
    {
        mongoc_cursor_t* cursor;
        const bson_t* doc;
        bson_error_t error;

        cursor = mongoc_collection_find_with_opts(collection, filter, nullptr,
                                                  nullptr);

        json results = json::array();
        while (mongoc_cursor_next(cursor, &doc))
        {
            char* doc_str = bson_as_canonical_extended_json(doc, nullptr);
            json doc_json = json::parse(doc_str);
            results.push_back(doc_json);
            bson_free(doc_str);
        }

        bool success = !mongoc_cursor_error(cursor, &error);

        std::string actual_result = results.dump(2);

        std::string expected_result = loadExpectedResult(expectedFile);

        compareResults(testName, actual_result, expected_result, success);

        mongoc_cursor_destroy(cursor);
    }

    void updateAndCompare(const std::string& testName, const bson_t* filter,
                          const bson_t* update, const std::string& expectedFile)
    {
        bson_t reply;
        bson_error_t error;

        bool success = mongoc_collection_update_one(collection, filter, update,
                                                    nullptr, &reply, &error);

        char* result_str = bson_as_canonical_extended_json(&reply, nullptr);
        std::string actual_result(result_str);
        bson_free(result_str);

        std::string expected_result = loadExpectedResult(expectedFile);

        compareResults(testName, actual_result, expected_result, success);

        bson_destroy(&reply);
    }

    void deleteAndCompare(const std::string& testName, const bson_t* filter,
                          const std::string& expectedFile)
    {
        bson_t reply;
        bson_error_t error;

        bool success = mongoc_collection_delete_one(collection, filter, nullptr,
                                                    &reply, &error);

        char* result_str = bson_as_canonical_extended_json(&reply, nullptr);
        std::string actual_result(result_str);
        bson_free(result_str);

        std::string expected_result = loadExpectedResult(expectedFile);

        compareResults(testName, actual_result, expected_result, success);

        bson_destroy(&reply);
    }

  private:
    std::string loadExpectedResult(const std::string& filename)
    {
        std::string filepath = expected_results_dir + filename;
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            ADD_FAILURE() << "Could not open expected result file: "
                          << filepath;
            return "{}";
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        return content;
    }

    void compareResults(const std::string& testName, const std::string& actual,
                        const std::string& expected, bool success)
    {

        std::cout << "\n=== Test: " << testName << " ===" << std::endl;
        std::cout << "Success: " << (success ? "true" : "false") << std::endl;
        std::cout << "Actual Result:" << std::endl << actual << std::endl;
        std::cout << "Expected Result:" << std::endl << expected << std::endl;

        json actual_json, expected_json;

        try
        {
            actual_json = json::parse(actual);
            expected_json = json::parse(expected);

            EXPECT_EQ(actual_json, expected_json)
                << "JSON results do not match for test: " << testName;
        }
        catch (const json::parse_error& e)
        {

            EXPECT_EQ(actual, expected)
                << "String results do not match for test: " << testName
                << " (JSON parse error: " << e.what() << ")";
        }

        std::string actual_file =
            expected_results_dir + "actual_" + testName + ".json";
        std::ofstream out(actual_file);
        out << actual;
        out.close();
    }
};

TEST_F(ClientRegressionTest, TestPing)
{
    bson_t* command = BCON_NEW("ping", BCON_INT32(1));
    executeAndCompare("ping", command, "ping_expected.json");
    bson_destroy(command);
}

TEST_F(ClientRegressionTest, TestListDatabases)
{
    bson_t* command = BCON_NEW("listDatabases", BCON_INT32(1));
    executeAndCompare("list_databases", command,
                      "list_databases_expected.json");
    bson_destroy(command);
}

TEST_F(ClientRegressionTest, TestInsertDocument)
{
    bson_t* document =
        BCON_NEW("name", BCON_UTF8("John Doe"), "age", BCON_INT32(30), "email",
                 BCON_UTF8("john@example.com"));
    insertAndCompare("insert_document", document,
                     "insert_document_expected.json");
    bson_destroy(document);
}

TEST_F(ClientRegressionTest, TestFindDocuments)
{

    bson_t* document =
        BCON_NEW("name", BCON_UTF8("Jane Doe"), "age", BCON_INT32(28), "email",
                 BCON_UTF8("jane@example.com"));
    mongoc_collection_insert_one(collection, document, nullptr, nullptr,
                                 nullptr);
    bson_destroy(document);

    bson_t* filter = BCON_NEW("age", "{", "$gt", BCON_INT32(25), "}");
    findAndCompare("find_documents", filter, "find_documents_expected.json");
    bson_destroy(filter);
}

TEST_F(ClientRegressionTest, TestUpdateDocument)
{

    bson_t* document = BCON_NEW("name", BCON_UTF8("Update Test"), "age",
                                BCON_INT32(35), "status", BCON_UTF8("active"));
    mongoc_collection_insert_one(collection, document, nullptr, nullptr,
                                 nullptr);
    bson_destroy(document);

    bson_t* filter = BCON_NEW("name", BCON_UTF8("Update Test"));
    bson_t* update = BCON_NEW("$set", "{", "age", BCON_INT32(36), "}");
    updateAndCompare("update_document", filter, update,
                     "update_document_expected.json");
    bson_destroy(filter);
    bson_destroy(update);
}

TEST_F(ClientRegressionTest, TestDeleteDocument)
{

    bson_t* document = BCON_NEW("name", BCON_UTF8("Delete Test"), "temporary",
                                BCON_BOOL(true));
    mongoc_collection_insert_one(collection, document, nullptr, nullptr,
                                 nullptr);
    bson_destroy(document);

    bson_t* filter = BCON_NEW("name", BCON_UTF8("Delete Test"));
    deleteAndCompare("delete_document", filter,
                     "delete_document_expected.json");
    bson_destroy(filter);
}

TEST_F(ClientRegressionTest, TestCountDocuments)
{

    bson_t* doc1 =
        BCON_NEW("category", BCON_UTF8("test"), "value", BCON_INT32(1));
    bson_t* doc2 =
        BCON_NEW("category", BCON_UTF8("test"), "value", BCON_INT32(2));
    mongoc_collection_insert_one(collection, doc1, nullptr, nullptr, nullptr);
    mongoc_collection_insert_one(collection, doc2, nullptr, nullptr, nullptr);
    bson_destroy(doc1);
    bson_destroy(doc2);

    bson_t* filter = BCON_NEW("category", BCON_UTF8("test"));
    bson_t* command = BCON_NEW("count", BCON_UTF8("users"), "query", filter);
    executeAndCompare("count_documents", command,
                      "count_documents_expected.json");
    bson_destroy(filter);
    bson_destroy(command);
}

TEST_F(ClientRegressionTest, TestComplexQuery)
{

    bson_t* doc1 = BCON_NEW(
        "name", BCON_UTF8("Alice"), "age", BCON_INT32(30), "skills", "[",
        BCON_UTF8("python"), BCON_UTF8("sql"), "]", "address", "{", "city",
        BCON_UTF8("San Francisco"), "state", BCON_UTF8("CA"), "}");
    mongoc_collection_insert_one(collection, doc1, nullptr, nullptr, nullptr);
    bson_destroy(doc1);

    bson_t* filter =
        BCON_NEW("age", "{", "$gte", BCON_INT32(25), "$lte", BCON_INT32(35),
                 "}", "address.state", BCON_UTF8("CA"));
    findAndCompare("complex_query", filter, "complex_query_expected.json");
    bson_destroy(filter);
}

} // namespace Regression
} // namespace FauxDB
