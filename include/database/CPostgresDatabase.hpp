/*-------------------------------------------------------------------------
 *
 * CPostgresDatabase.hpp
 *      PostgreSQL database implementation for FauxDB.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------*/

#pragma once

#include "CDatabase.hpp"
#include "CLibpq.hpp"
#include <memory>
#include <string>
#include <vector>

namespace FauxDB
{

class CPostgresDatabase : public CDatabase
{
public:
	CPostgresDatabase();
	~CPostgresDatabase() override;

	bool initialize(const CDatabaseConfig& config) override;
	bool connect() override;
	bool connect(const CDatabaseConfig& config);
	void disconnect() override;
	bool isConnected() const override;
	CDatabaseStatus getStatus() const override;

	CDatabaseQueryResult executeQuery(const std::string& query) override;
	CDatabaseQueryResult executePreparedQuery(const std::string& queryName,
						      const std::vector<std::string>& params) override;
	CDatabaseQueryResult executeQuery(const std::string& query, const std::vector<std::string>& parameters);
	CDatabaseQueryResult executeQuery(const std::vector<uint8_t>& queryData);

	bool beginTransaction() override;
	bool commitTransaction() override;
	bool rollbackTransaction() override;
	CDatabaseTransactionStatus getTransactionStatus() const override;

	std::string getLastError() const override;
	size_t getLastInsertId() const override;
	size_t getAffectedRows() const override;

	bool ping() override;
	std::string getServerVersion() const override;
	std::string getConnectionInfo() const override;

	// Stub and utility methods
	bool createTable(const std::string& tableName, const std::vector<std::string>& columns);
	bool dropTable(const std::string& tableName);
	bool alterTable(const std::string& tableName, const std::string& operation);
	std::vector<std::string> getTableNames();
	std::vector<std::string> getColumnNames(const std::string& tableName);
	bool insertData(const std::string& tableName, const std::vector<std::string>& columns, const std::vector<std::vector<std::string>>& values);
	bool updateData(const std::string& tableName, const std::vector<std::string>& setColumns, const std::vector<std::string>& setValues, const std::string& whereClause);
	bool deleteData(const std::string& tableName, const std::string& whereClause);
	bool createIndex(const std::string& tableName, const std::string& indexName, const std::vector<std::string>& columns);
	bool dropIndex(const std::string& indexName);
	bool vacuumDatabase();
	bool analyzeDatabase();
	void setConfig(const CDatabaseConfig& config);
	CDatabaseConfig getConfig() const;
	void setConnectionTimeout(std::chrono::milliseconds timeout);
	void setQueryTimeout(std::chrono::milliseconds timeout);
	std::string getDatabaseInfo() const;
	std::string getVersion() const;
	size_t getActiveConnections() const;
	bool healthCheck();
	bool backupDatabase(const std::string& backupPath);
	bool restoreDatabase(const std::string& backupPath);
	bool exportData(const std::string& tableName, const std::string& exportPath);
	bool importData(const std::string& tableName, const std::string& importPath);
	bool createUser(const std::string& username, const std::string& password);
	bool dropUser(const std::string& username);
	bool grantPrivileges(const std::string& username, const std::string& privileges);
	bool revokePrivileges(const std::string& username, const std::string& privileges);
	void clearErrors();
	bool hasErrors() const;
	void setConnectionCallback(std::function<void(CDatabaseStatus)> callback);
	void setQueryCallback(std::function<void(const std::string&, const CDatabaseQueryResult&)> callback);
	void setErrorCallback(std::function<void(const std::string&, const std::string&)> callback);

protected:
	void setStatus(CDatabaseStatus status);
	void setTransactionStatus(CDatabaseTransactionStatus status);
	void updateLastActivity();
	void logDatabaseEvent(const std::string& event, const std::string& details);
	CDatabaseQueryResult processQuery(const std::string& query);
	CDatabaseQueryResult processParameterizedQuery(const std::string& query, const std::vector<std::string>& parameters);
	bool validateQuery(const std::string& query);
	std::string sanitizeQuery(const std::string& query);
	bool validateTransactionState();
	void logTransactionEvent(const std::string& event);

private:
	std::unique_ptr<CLibpq> libpq_;
	CDatabaseConfig postgresConfig_;
	bool connectionEstablished_;
	void initializePostgresDefaults();
	void setPostgresError();
	std::function<void(CDatabaseStatus)> connectionCallback_;
	std::function<void(const std::string&, const CDatabaseQueryResult&)> queryCallback_;
	std::function<void(const std::string&, const std::string&)> errorCallback_;
};

} // namespace FauxDB



