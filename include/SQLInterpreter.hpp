#ifndef SQLINTERPRETER_HPP
#define SQLINTERPRETER_HPP

#include <optional>

#include "StorageEngine.hpp"
#include "sql/SQLStatement.h"
#include "sql/statements.h"

using DatabaseRef = reference_wrapper<Database>;

class SQLInterpreter {
    optional<DatabaseRef> db;
public:
    SQLInterpreter();
    SQLInterpreter(Database& db);

    void execute(const string& sql);

private:
    void setDatabase(Database& db);
    void executeStatement(hsql::SQLStatement *statement);
    void executeSelect(hsql::SelectStatement *select);
};

#endif // SQLINTERPRETER_HPP