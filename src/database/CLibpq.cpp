/*-------------------------------------------------------------------------
 *
 * CLibpq.cpp
 *      PostgreSQL libpq wrapper implementation for FauxDB.
 *      Provides a clean interface to PostgreSQL C API.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */
#include "CLibpq.hpp"

#include <algorithm>
#include <libpq-fe.h>
#include <sstream>
#include <stdexcept>

namespace FauxDB
{

/*-------------------------------------------------------------------------
 * PLibpqResult implementation
 *-------------------------------------------------------------------------*/
PLibpqResult::PLibpqResult(PGresult* result)
	: result_(result), ownsResult_(true)
{
}

PLibpqResult::~PLibpqResult()
{
	if (result_ && ownsResult_)
	{
		PQclear(result_);
	}
}

bool PLibpqResult::isValid() const
{
	return result_ != nullptr;
}

bool PLibpqResult::isTuplesOk() const
{
	return result_ && PQresultStatus(result_) == PGRES_TUPLES_OK;
}

bool PLibpqResult::isCommandOk() const
{
	return result_ && PQresultStatus(result_) == PGRES_COMMAND_OK;
}

bool PLibpqResult::isError() const
{
	return result_ && (PQresultStatus(result_) == PGRES_FATAL_ERROR ||
					   PQresultStatus(result_) == PGRES_NONFATAL_ERROR);
}

int PLibpqResult::getRowCount() const
{
	return result_ ? PQntuples(result_) : 0;
}

int PLibpqResult::getColumnCount() const
{
	return result_ ? PQnfields(result_) : 0;
}

std::string PLibpqResult::getColumnName(int columnIndex) const
{
	if (!result_ || columnIndex < 0 || columnIndex >= getColumnCount())
	{
		return "";
	}
	const char* name = PQfname(result_, columnIndex);
	return name ? name : "";
}

std::string PLibpqResult::getValue(int rowIndex, int columnIndex) const
{
	if (!result_ || rowIndex < 0 || rowIndex >= getRowCount() ||
		columnIndex < 0 || columnIndex >= getColumnCount())
	{
		return "";
	}
	const char* value = PQgetvalue(result_, rowIndex, columnIndex);
	return value ? value : "";
}

std::vector<std::string> PLibpqResult::getRow(int rowIndex) const
{
	std::vector<std::string> row;
	if (!result_ || rowIndex < 0 || rowIndex >= getRowCount())
	{
		return row;
	}

	int colCount = getColumnCount();
	for (int i = 0; i < colCount; ++i)
	{
		row.push_back(getValue(rowIndex, i));
	}
	return row;
}

std::vector<std::string> PLibpqResult::getColumnNames() const
{
	std::vector<std::string> names;
	if (!result_)
	{
		return names;
	}

	int colCount = getColumnCount();
	for (int i = 0; i < colCount; ++i)
	{
		names.push_back(getColumnName(i));
	}
	return names;
}

std::vector<std::vector<std::string>> PLibpqResult::getAllRows() const
{
	std::vector<std::vector<std::string>> rows;
	if (!result_)
	{
		return rows;
	}

	int rowCount = getRowCount();
	for (int i = 0; i < rowCount; ++i)
	{
		rows.push_back(getRow(i));
	}
	return rows;
}

std::string PLibpqResult::getErrorMessage() const
{
	if (!result_)
	{
		return "";
	}
	const char* error = PQresultErrorMessage(result_);
	return error ? error : "";
}

std::string PLibpqResult::getErrorDetail() const
{
	if (!result_)
	{
		return "";
	}
	const char* detail = PQresultErrorField(result_, PG_DIAG_MESSAGE_DETAIL);
	return detail ? detail : "";
}

std::string PLibpqResult::getErrorHint() const
{
	if (!result_)
	{
		return "";
	}
	const char* hint = PQresultErrorField(result_, PG_DIAG_MESSAGE_HINT);
	return hint ? hint : "";
}

/*-------------------------------------------------------------------------
 * CLibpq implementation
 *-------------------------------------------------------------------------*/
CLibpq::CLibpq() : connection_(nullptr), connectionEstablished_(false)
{
}

CLibpq::CLibpq(const PLibpqConfig& config)
	: connection_(nullptr), config_(config), connectionEstablished_(false)
{
}

CLibpq::~CLibpq()
{
	disconnect();
}

bool CLibpq::connect(const PLibpqConfig& config)
{
	config_ = config;
	return connect(buildConnectionString());
}

bool CLibpq::connect(const std::string& host, const std::string& port, const std::string& database,
					 const std::string& username, const std::string& password)
{
	PLibpqConfig config;
	config.host = host;
	config.port = port;
	config.database = database;
	config.username = username;
	config.password = password;
	
	return connect(config);
}

bool CLibpq::connect(const std::string& connectionString)
{
	if (connection_)
	{
		disconnect();
	}

	try
	{
		connection_ = PQconnectdb(connectionString.c_str());

		if (!connection_ || PQstatus(connection_) != CONNECTION_OK)
		{
			setError(PQerrorMessage(connection_));
			return false;
		}

		if (!setConnectionParameters())
		{
			disconnect();
			return false;
		}

		connectionEstablished_ = true;
		return true;
	}
	catch (const std::exception& e)
	{
		setError("Exception during connection: " + std::string(e.what()));
		return false;
	}
}

void CLibpq::disconnect()
{
	if (connection_)
	{
		if (isTransactionActive())
		{
			rollbackTransaction();
		}

		cleanupConnection();
		connection_ = nullptr;
		connectionEstablished_ = false;
	}
}

bool CLibpq::isConnected() const
{
	return connection_ && PQstatus(connection_) == CONNECTION_OK;
}

ConnStatusType CLibpq::getConnectionStatus() const
{
	return connection_ ? PQstatus(connection_) : CONNECTION_BAD;
}

std::unique_ptr<PLibpqResult> CLibpq::executeQuery(const std::string& query)
{
	if (!isConnected())
	{
		setError("Not connected to database");
		return nullptr;
	}

	try
	{
		PGresult* result = PQexec(connection_, query.c_str());
		return std::make_unique<PLibpqResult>(result);
	}
	catch (const std::exception& e)
	{
		setError("Exception during query execution: " + std::string(e.what()));
		return nullptr;
	}
}

std::unique_ptr<PLibpqResult>
CLibpq::executeQuery(const std::string& query,
					 const std::vector<std::string>& parameters)
{
	if (!isConnected())
	{
		setError("Not connected to database");
		return nullptr;
	}

	try
	{
		/* Convert string parameters to const char* for PQexecParams */
		std::vector<const char*> paramPtrs;
		for (const auto& param : parameters)
		{
			paramPtrs.push_back(param.c_str());
		}

		PGresult* result =
			PQexecParams(connection_, query.c_str(), parameters.size(), nullptr,
						 paramPtrs.data(), nullptr, nullptr, 0);
		return std::make_unique<PLibpqResult>(result);
	}
	catch (const std::exception& e)
	{
		setError("Exception during parameterized query execution: " +
				 std::string(e.what()));
		return nullptr;
	}
}

std::unique_ptr<PLibpqResult>
CLibpq::executeQuery(const std::string& query,
					 const std::vector<std::string>& parameters,
					 const std::vector<int>& paramTypes)
{
	if (!isConnected())
	{
		setError("Not connected to database");
		return nullptr;
	}

	if (parameters.size() != paramTypes.size())
	{
		setError("Parameter count mismatch");
		return nullptr;
	}

	try
	{
		/* Convert string parameters to const char* for PQexecParams */
		std::vector<const char*> paramPtrs;
		for (const auto& param : parameters)
		{
			paramPtrs.push_back(param.c_str());
		}

		/* Convert int paramTypes to Oid* */
		std::vector<Oid> oidTypes;
		for (int type : paramTypes)
		{
			oidTypes.push_back(static_cast<Oid>(type));
		}

		PGresult* result = PQexecParams(connection_, query.c_str(),
										parameters.size(), oidTypes.data(),
										paramPtrs.data(), nullptr, nullptr, 0);
		return std::make_unique<PLibpqResult>(result);
	}
	catch (const std::exception& e)
	{
		setError("Exception during typed parameterized query execution: " +
				 std::string(e.what()));
		return nullptr;
	}
}

bool CLibpq::beginTransaction()
{
	if (!isConnected())
	{
		setError("Not connected to database");
		return false;
	}

	auto result = executeQuery("BEGIN");
	return result && result->isCommandOk();
}

bool CLibpq::commitTransaction()
{
	if (!isConnected())
	{
		setError("Not connected to database");
		return false;
	}

	auto result = executeQuery("COMMIT");
	return result && result->isCommandOk();
}

bool CLibpq::rollbackTransaction()
{
	if (!isConnected())
	{
		setError("Not connected to database");
		return false;
	}

	auto result = executeQuery("ROLLBACK");
	return result && result->isCommandOk();
}

bool CLibpq::isTransactionActive() const
{
	if (!connection_)
	{
		return false;
	}

	PGresult* result = PQexec(connection_, "SELECT txid_current()");
	if (!result)
	{
		return false;
	}

	bool active = (PQresultStatus(result) == PGRES_TUPLES_OK);
	PQclear(result);

	return active;
}

std::string CLibpq::getServerVersion() const
{
	if (!connection_)
	{
		return "";
	}
	return PQserverVersion(connection_)
			   ? std::to_string(PQserverVersion(connection_))
			   : "";
}

std::string CLibpq::getServerEncoding() const
{
	if (!connection_)
	{
		return "";
	}
	const char* encoding = PQparameterStatus(connection_, "server_encoding");
	return encoding ? encoding : "";
}

std::string CLibpq::getClientEncoding() const
{
	if (!connection_)
	{
		return "";
	}
	const char* encoding = PQparameterStatus(connection_, "client_encoding");
	return encoding ? encoding : "";
}

std::string CLibpq::getApplicationName() const
{
	if (!connection_)
	{
		return "";
	}
	const char* appName = PQparameterStatus(connection_, "application_name");
	return appName ? appName : "";
}

std::string CLibpq::getLastError() const
{
	if (connection_)
	{
		const char* error = PQerrorMessage(connection_);
		return error ? error : lastError_;
	}
	return lastError_;
}

void CLibpq::clearErrors()
{
	lastError_.clear();
}

bool CLibpq::hasErrors() const
{
	return !lastError_.empty();
}

void CLibpq::setConfig(const PLibpqConfig& config)
{
	config_ = config;
}

PLibpqConfig CLibpq::getConfig() const
{
	return config_;
}

std::string CLibpq::escapeString(const std::string& str) const
{
	if (!connection_)
	{
		return str;
	}

	char* escaped = PQescapeLiteral(connection_, str.c_str(), str.length());
	if (!escaped)
	{
		return str;
	}

	std::string result(escaped);
	PQfreemem(escaped);
	return result;
}

std::string CLibpq::escapeIdentifier(const std::string& identifier) const
{
	if (!connection_)
	{
		return identifier;
	}

	char* escaped = PQescapeIdentifier(connection_, identifier.c_str(),
									   identifier.length());
	if (!escaped)
	{
		return identifier;
	}

	std::string result(escaped);
	PQfreemem(escaped);
	return result;
}

bool CLibpq::ping()
{
	if (!isConnected())
	{
		return false;
	}

	auto result = executeQuery("SELECT 1");
	return result && result->isTuplesOk();
}

void CLibpq::setError(const std::string& error)
{
	lastError_ = error;
}

std::string CLibpq::buildConnectionString() const
{
	std::ostringstream oss;
	oss << "host=" << config_.host << " port=" << config_.port
		<< " dbname=" << config_.database << " user=" << config_.username
		<< " password=" << config_.password;

	if (!config_.sslmode.empty())
	{
		oss << " sslmode=" << config_.sslmode;
	}

	if (!config_.applicationName.empty())
	{
		oss << " application_name=" << config_.applicationName;
	}

	if (!config_.clientEncoding.empty())
	{
		oss << " client_encoding=" << config_.clientEncoding;
	}

	// timezone is set after connection using SET timezone, not in connection string

	return oss.str();
}

bool CLibpq::setConnectionParameters()
{
	if (!connection_)
	{
		return false;
	}

	/* Set connection parameters */
	if (!config_.applicationName.empty())
	{
		PQexec(connection_,
			   ("SET application_name = '" + config_.applicationName + "'")
				   .c_str());
	}

	if (!config_.clientEncoding.empty())
	{
		PQexec(
			connection_,
			("SET client_encoding = '" + config_.clientEncoding + "'").c_str());
	}

	if (!config_.timezone.empty())
	{
		PQexec(connection_,
			   ("SET timezone = '" + config_.timezone + "'").c_str());
	}

	return true;
}

void CLibpq::cleanupConnection()
{
	if (connection_)
	{
		PQfinish(connection_);
	}
}

} /* namespace FauxDB */
