#include "parser.h"
#include <iostream>
#include <sstream>
#include <algorithm>

// Helper function to convert string to uppercase
std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

std::unique_ptr<Statement> parse_statement(const std::string& sql) {
    std::cout << "Parsing SQL: " << sql << std::endl;
    auto statement = std::make_unique<Statement>();
    statement->type = STATEMENT_UNKNOWN;

    std::stringstream ss(sql);
    std::string token;

    ss >> token;
    if (to_upper(token) == "CREATE") {
        ss >> token;
        if (to_upper(token) == "TABLE") {
            ss >> token; // table_name
            statement->type = STATEMENT_CREATE_TABLE;
            statement->create_table_statement = std::make_unique<CreateTableStatement>();
            statement->create_table_statement->table_name = token;
            // Further parsing for columns will go here
        }
    } else if (to_upper(token) == "INSERT") {
        ss >> token;
        if (to_upper(token) == "INTO") {
            ss >> token; // table_name
            statement->type = STATEMENT_INSERT;
            statement->insert_statement = std::make_unique<InsertStatement>();
            statement->insert_statement->table_name = token;

            // For now, just consume the rest of the line as values
            std::string remaining_sql;
            std::getline(ss, remaining_sql);
            // Trim leading whitespace
            remaining_sql.erase(0, remaining_sql.find_first_not_of(" \t\n\r\f\v"));
            if (to_upper(remaining_sql.substr(0, 6)) == "VALUES") {
                statement->insert_statement->values.push_back(remaining_sql.substr(6));
            }
        }
    }

    return statement;
}
