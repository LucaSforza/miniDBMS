#include <iostream>

#include "SQLInterpreter.hpp"
#include "sql/SQLStatement.h"
#include "sql/Table.h"

SQLInterpreter::SQLInterpreter(): db(nullopt) {}

SQLInterpreter::SQLInterpreter(Database& db): db(db) {}

void SQLInterpreter::setDatabase(Database& db) { this->db = db; }

void SQLInterpreter::execute(const string& sql) {
    hsql::SQLParserResult result;

    hsql::SQLParser::parse(sql,&result);

    if(result.isValid() && result.size() > 0) {
        for(auto statement : result.getStatements()) {
            executeStatement(statement);
        }        
    } else cout << "SQL_PARSER_ERROR: " << result.errorMsg() << endl;

}

void SQLInterpreter::executeStatement(hsql::SQLStatement *statement) {
    switch (statement->type()) {
        case hsql::StatementType::kStmtSelect : 
        executeSelect(dynamic_cast<hsql::SelectStatement*>(statement));
        break;
        default:
            cout << "SQL: unsupported query" << endl;
            break;
    }
}

void SQLInterpreter::executeSelect(hsql::SelectStatement *select) {
    if(select->fromTable == NULL) {
        cout << "SQL: from Table void" << endl;
        return;
    }
    auto table = select->fromTable;

    switch (table->type) {
    case hsql::TableRefType::kTableSelect:
        /* code */
        break;
    
    default:
        cout << "SQL: unsupported query" << endl;
    }

}