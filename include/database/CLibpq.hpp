/*-------------------------------------------------------------------------
 *
 * CLibpq.hpp
 *      PostgreSQL libpq wrapper for FauxDB.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#pragma once

#include <libpq-fe.h>
#include <memory>
#include <string>
#include <vector>
#include <chrono>

namespace FauxDB
{

struct PLibpqConfig
{
	std::string host;
	std::string port; 
	std::string database;
	std::string username;
	std::string password;
	std::string sslmode;
	std::string applicationName;
	std::string clientEncoding;
	std::string timezone;
	bool binaryResults;
	bool preparedStatements;
	std::chrono::milliseconds connectionTimeout;
	std::chrono::milliseconds queryTimeout;
	size_t maxConnections;
	bool autoCommit;
	bool sslEnabled;

	PLibpqConfig()
		: host("localhost"), port("5432"), database("")
		, username("")
		, password("")
		, sslmode("prefer")
		, applicationName("FauxDB")
		, clientEncoding("UTF8")
		, timezone("UTC")
		, binaryResults(false)
		, preparedStatements(true)
		, connectionTimeout(5000)
		, queryTimeout(30000)
		, maxConnections(10)
		, autoCommit(true)
		, sslEnabled(false)
	{}
};

class PLibpqResult
{
public:
	PLibpqResult(PGresult* result);
	~PLibpqResult();

	bool isValid() const;
	bool isTuplesOk() const;
	bool isCommandOk() const;
	bool isError() const;
	int getRowCount() const;
	int getColumnCount() const;
	std::string getColumnName(int columnIndex) const;
	std::string getValue(int rowIndex, int columnIndex) const;
	bool isNull(int rowIndex, int columnIndex) const;
	std::string getErrorMessage() const;
	std::vector<std::string> getRow(int rowIndex) const;
	std::vector<std::string> getColumnNames() const;
	std::vector<std::vector<std::string>> getAllRows() const;
	std::string getErrorDetail() const;
	std::string getErrorHint() const;

private:
	PGresult* result_;
	bool ownsResult_;
};

class PLibpqConnection
{
public:
	PLibpqConnection();
	~PLibpqConnection();

	bool connect(const std::string& connectionString);
	bool connect(const std::string& host, const std::string& port, const std::string& database,
				 const std::string& username, const std::string& password);
	void disconnect();
	bool isConnected() const;
	ConnStatusType getConnectionStatus() const;
	std::string getErrorMessage() const;

	std::unique_ptr<PLibpqResult> executeQuery(const std::string& query);
	std::unique_ptr<PLibpqResult> executeQuery(const std::string& query, const std::vector<std::string>& parameters);
	std::unique_ptr<PLibpqResult> executeQuery(const std::string& query, const std::vector<std::string>& parameters, const std::vector<int>& paramTypes);

	bool beginTransaction();
	bool commitTransaction();
	bool rollbackTransaction();
	bool isTransactionActive() const;

	PGconn* getRawConnection() const;

	std::string getServerVersion() const;
	std::string getServerEncoding() const;
	std::string getClientEncoding() const;
	std::string getApplicationName() const;
	std::string getLastError() const;
	void clearErrors();
	bool hasErrors() const;
	void setConfig(const PLibpqConfig& config);
	PLibpqConfig getConfig() const;
	std::string escapeString(const std::string& str) const;
	std::string escapeIdentifier(const std::string& identifier) const;
	bool ping();

private:
	PGconn* connection_;
	PLibpqConfig config_;
	std::string lastError_;
	bool connectionEstablished_;
};

class CLibpq
{
public:
	CLibpq();
	CLibpq(const PLibpqConfig& config);
	~CLibpq();

	bool connect(const PLibpqConfig& config);
	bool connect(const std::string& connectionString);
	bool connect(const std::string& host, const std::string& port, const std::string& database,
				 const std::string& username, const std::string& password);
	void disconnect();
	bool isConnected() const;
	ConnStatusType getConnectionStatus() const;
	std::string getErrorMessage() const;

	std::unique_ptr<PLibpqResult> executeQuery(const std::string& query);
	std::unique_ptr<PLibpqResult> executeQuery(const std::string& query, const std::vector<std::string>& parameters);
	std::unique_ptr<PLibpqResult> executeQuery(const std::string& query, const std::vector<std::string>& parameters, const std::vector<int>& paramTypes);

	bool beginTransaction();
	bool commitTransaction();
	bool rollbackTransaction();
	bool isTransactionActive() const;

	PGconn* getRawConnection() const;
	std::string getServerVersion() const;
	std::string getServerEncoding() const;
	std::string getClientEncoding() const;
	std::string getApplicationName() const;
	std::string getLastError() const;
	void clearErrors();
	bool hasErrors() const;
	void setConfig(const PLibpqConfig& config);
	PLibpqConfig getConfig() const;
	std::string escapeString(const std::string& str) const;
	std::string escapeIdentifier(const std::string& identifier) const;
	bool ping();

private:
	PGconn* connection_;
	PLibpqConfig config_;
	std::string lastError_;
	bool connectionEstablished_;
	
	// Helper methods
	void setError(const std::string& error);
	std::string buildConnectionString() const;
	bool setConnectionParameters();
	void cleanupConnection();
};

} /* namespace FauxDB */
