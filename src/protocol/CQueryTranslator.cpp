
#include "protocol/CQueryTranslator.hpp"

#include <sstream>
#include <stdexcept>

using namespace FauxDB;

CQueryTranslator::CQueryTranslator(
	std::shared_ptr<FauxDB::CDatabase> database)
	: database_(database), currentCollection_(""),
	  targetDatabaseType_("PostgreSQL"), escapeIdentifiers_(false),
	  quoteStyle_("\"")
{
}

CQueryTranslator::~CQueryTranslator()
{
}

std::string
CQueryTranslator::translateDocumentQueryToSQL(const CQueryContext& context)
{
	std::string sql = "SELECT ";


	if (!context.projection_json.empty())
	{
		std::vector<std::string> columns;


		columns.push_back("*");
		sql += join(columns, ", ");
	}
	else
	{
		sql += "*";
	}


	if (!context.filter_json.empty())
	{
		sql += " WHERE " + translateFilter(context.filter_json);
	}


	if (!context.sort_json.empty())
	{
		sql += " ORDER BY " + translateSort(context.sort_json);
	}

	return sql;
}

std::string CQueryTranslator::translateFilter(const std::string& docFilter)
{
	if (docFilter.empty())
		return "";



	std::string sqlFilter = docFilter;



	size_t pos = sqlFilter.find("$eq");
	while (pos != std::string::npos)
	{
		sqlFilter.replace(pos, 3, "=");
		pos = sqlFilter.find("$eq", pos + 1);
	}


	pos = sqlFilter.find("$ne");
	while (pos != std::string::npos)
	{
		sqlFilter.replace(pos, 3, "!=");
		pos = sqlFilter.find("$ne", pos + 1);
	}


	pos = sqlFilter.find("$gt");
	while (pos != std::string::npos)
	{
		sqlFilter.replace(pos, 3, ">");
		pos = sqlFilter.find("$gt", pos + 1);
	}


	pos = sqlFilter.find("$lt");
	while (pos != std::string::npos)
	{
		sqlFilter.replace(pos, 3, "<");
		pos = sqlFilter.find("$lt", pos + 1);
	}

	return sqlFilter;
}

std::string CQueryTranslator::translateProjection(const std::string& docProjection)
{
	if (docProjection.empty())
		return "*";



	std::vector<std::string> columns;


	std::istringstream iss(docProjection);
	std::string field;
	while (std::getline(iss, field, ','))
	{

		field.erase(0, field.find_first_not_of(" \t"));
		field.erase(field.find_last_not_of(" \t") + 1);
		if (!field.empty())
		{
			columns.push_back(escapeSQLIdentifier(field));
		}
	}

	return columns.empty() ? "*" : join(columns, ", ");
}

std::string CQueryTranslator::translateSort(const std::string& docSort)
{
	if (docSort.empty())
		return "";


	std::vector<std::string> sortClauses;


	std::istringstream iss(docSort);
	std::string clause;
	while (std::getline(iss, clause, ','))
	{

		clause.erase(0, clause.find_first_not_of(" \t"));
		clause.erase(clause.find_last_not_of(" \t") + 1);

		if (!clause.empty())
		{
			size_t colonPos = clause.find(':');
			if (colonPos != std::string::npos)
			{
				std::string field = clause.substr(0, colonPos);
				std::string direction = clause.substr(colonPos + 1);


				field.erase(0, field.find_first_not_of(" \t"));
				field.erase(field.find_last_not_of(" \t") + 1);
				direction.erase(0, direction.find_first_not_of(" \t"));
				direction.erase(direction.find_last_not_of(" \t") + 1);

				if (!field.empty())
				{
					std::string sortClause = escapeSQLIdentifier(field);
					if (direction == "desc" || direction == "-1")
					{
						sortClause += " DESC";
					}
					else
					{
						sortClause += " ASC";
					}
					sortClauses.push_back(sortClause);
				}
			}
			else
			{

				sortClauses.push_back(escapeSQLIdentifier(clause) + " ASC");
			}
		}
	}

	return join(sortClauses, ", ");
}

std::string CQueryTranslator::translateUpdate(const std::string& docUpdate)
{
	if (docUpdate.empty())
		return "";



	std::string sqlUpdate = "UPDATE " + buildTableName("", currentCollection_);




	std::vector<std::string> setClauses;
	std::istringstream iss(docUpdate);
	std::string pair;
	while (std::getline(iss, pair, ','))
	{

		pair.erase(0, pair.find_first_not_of(" \t"));
		pair.erase(pair.find_last_not_of(" \t") + 1);

		if (!pair.empty())
		{
			size_t colonPos = pair.find(':');
			if (colonPos != std::string::npos)
			{
				std::string field = pair.substr(0, colonPos);
				std::string value = pair.substr(colonPos + 1);


				field.erase(0, field.find_first_not_of(" \t"));
				field.erase(field.find_last_not_of(" \t") + 1);
				value.erase(0, value.find_first_not_of(" \t"));
				value.erase(value.find_last_not_of(" \t") + 1);

				if (!field.empty())
				{
					setClauses.push_back(escapeSQLIdentifier(field) + " = " + escapeSQLString(value));
				}
			}
		}
	}

	if (setClauses.empty())
		return "";

	sqlUpdate += " SET " + join(setClauses, ", ");
	return sqlUpdate;
}

std::string CQueryTranslator::translateAggregation(const std::string& docAggregation)
{
	if (docAggregation.empty())
		return "";



	std::string sql = "SELECT ";





	sql += "* FROM " + buildTableName("", currentCollection_);

	return sql;
}

std::string CQueryTranslator::translateIndex(const std::string& docIndex)
{
	if (docIndex.empty())
		return "";


	std::string sql = "CREATE INDEX ";





	sql += "idx_" + currentCollection_ + " ON " + buildTableName("", currentCollection_);

	return sql;
}

std::string CQueryTranslator::translateSchema(const std::string& docSchema)
{
	if (docSchema.empty())
		return "";


	std::string sql =
		"CREATE TABLE " + buildTableName("", currentCollection_) + " (";





	sql += "id SERIAL PRIMARY KEY, data JSONB";

	return sql + ")";
}

std::string CQueryTranslator::translateGeoQuery(const std::string& docGeoQuery)
{
	if (docGeoQuery.empty())
		return "";



	std::string sql =
		"SELECT * FROM " + buildTableName("", currentCollection_) + " WHERE ";





	sql += "ST_DWithin(geometry, ST_GeomFromText('POINT(0 0)', 4326), 1000)";

	return sql;
}

std::string CQueryTranslator::translateTextSearch(const std::string& docTextSearch)
{
	if (docTextSearch.empty())
		return "";



	std::string sql =
		"SELECT * FROM " + buildTableName("", currentCollection_) + " WHERE ";





	sql += "to_tsvector('english', content) @@ plainto_tsquery('english', '" +
		   escapeSQLString(docTextSearch) + "')";

	return sql;
}

std::string CQueryTranslator::translateArrayOperator(const std::string& field,
													 const std::string& value)
{
	return field + " IN (" + value + ")";
}

std::string CQueryTranslator::translateRegexOperator(const std::string& field,
													 const std::string& value)
{
	return field + " REGEXP '" + value + "'";
}

std::string CQueryTranslator::translateGeoOperator(const std::string& field,
												   const std::string& value)
{
	return field + " GEO_OP '" + value + "'";
}

std::string CQueryTranslator::translateDateOperator(const std::string& field,
													const std::string& value)
{
	return field + " DATE_OP '" + value + "'";
}

std::string CQueryTranslator::translateMathOperator(const std::string& field,
													const std::string& value)
{
	return field + " MATH_OP '" + value + "'";
}

std::string CQueryTranslator::escapeSQLIdentifier(const std::string& identifier)
{
	return quoteStyle_ + identifier + quoteStyle_;
}

std::string CQueryTranslator::escapeSQLString(const std::string& str)
{
	return "'" + str + "'";
}

std::string
CQueryTranslator::convertDocumentTypeToPostgreSQL(const std::string& documentType)
{
	(void)documentType;

	return "TEXT";
}

std::string CQueryTranslator::buildTableName(const std::string& database,
											 const std::string& collection)
{
	return database + "." + collection;
}

std::string CQueryTranslator::buildColumnName(const std::string& field)
{
	return field;
}

bool CQueryTranslator::isValidJSON(const std::string& jsonStr)
{
	return !jsonStr.empty();
}

std::string CQueryTranslator::extractFieldValue(const std::string& jsonStr,
												const std::string& fieldName)
{
	(void)jsonStr;
	(void)fieldName;

	return "";
}

std::string CQueryTranslator::join(const std::vector<std::string>& elements,
								   const std::string& delimiter)
{
	std::string result;
	for (size_t i = 0; i < elements.size(); ++i)
	{
		result += elements[i];
		if (i + 1 < elements.size())
			result += delimiter;
	}
	return result;
}


const std::unordered_map<std::string, std::string>
	CQueryTranslator::comparisonOperators_ = {};
const std::unordered_map<std::string, std::string>
	CQueryTranslator::logicalOperators_ = {};
const std::unordered_map<std::string, std::string>
	CQueryTranslator::arrayOperators_ = {};
const std::unordered_map<std::string, std::string>
	CQueryTranslator::geoOperators_ = {};
