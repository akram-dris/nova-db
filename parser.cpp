#include "parser.h"
#include "serializer.h" // For deserialize_int
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>

// Helper function to convert string to uppercase
std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

// Helper function to trim whitespace
std::string trim(const std::string& s) {
    size_t first = s.find_first_not_of(" \t\n\r\f\v");
    if (std::string::npos == first) {
        return s;
    }
    size_t last = s.find_last_not_of(" \t\n\r\f\v");
    return s.substr(first, (last - first + 1));
}

ColumnType string_to_column_type(const std::string& type_str) {
    std::string upper_type = to_upper(type_str);
    if (upper_type == "INT") {
        return COLUMN_TYPE_INT;
    } else if (upper_type == "TEXT") {
        return COLUMN_TYPE_TEXT;
    } else {
        return COLUMN_TYPE_UNKNOWN;
    }
}

std::string column_type_to_string(ColumnType type) {
    switch (type) {
        case COLUMN_TYPE_INT: return "INT";
        case COLUMN_TYPE_TEXT: return "TEXT";
        case COLUMN_TYPE_UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN"; // Should not be reached
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

            // Parse column definitions (e.g., (id INT, name TEXT))
            std::string remaining_sql;
            std::getline(ss, remaining_sql); // Get the rest of the line
            remaining_sql = trim(remaining_sql);

            // Remove trailing semicolon if present
            if (!remaining_sql.empty() && remaining_sql.back() == ';') {
                remaining_sql.pop_back();
                remaining_sql = trim(remaining_sql);
            }

            if (remaining_sql.length() > 2 && remaining_sql.front() == '(' && remaining_sql.back() == ')') {
                std::string columns_str = remaining_sql.substr(1, remaining_sql.length() - 2);
                std::stringstream cols_ss(columns_str);
                std::string col_token;

                while (std::getline(cols_ss, col_token, ',')) {
                    std::stringstream col_def_ss(trim(col_token));
                    std::string col_name;
                    std::string col_type_str;
                    col_def_ss >> col_name >> col_type_str;

                    if (!col_name.empty() && !col_type_str.empty()) {
                        ColumnDefinition col_def;
                        col_def.name = col_name;
                        col_def.type = string_to_column_type(col_type_str);
                        statement->create_table_statement->columns.push_back(col_def);
                    }
                }
            }
        }
    } else if (to_upper(token) == "INSERT") {
        ss >> token;
        if (to_upper(token) == "INTO") {
            ss >> token; // table_name
            statement->type = STATEMENT_INSERT;
            statement->insert_statement = std::make_unique<InsertStatement>();
            statement->insert_statement->table_name = token;

            // Parse values (e.g., (value1, value2, ...))
            std::string remaining_sql;
            std::getline(ss, remaining_sql); // Get the rest of the line
            remaining_sql = trim(remaining_sql);

            // Remove trailing semicolon if present
            if (!remaining_sql.empty() && remaining_sql.back() == ';') {
                remaining_sql.pop_back();
                remaining_sql = trim(remaining_sql);
            }

            // Expecting VALUES (val1, val2, ...)
            size_t values_pos = to_upper(remaining_sql).find("VALUES");
            if (values_pos != std::string::npos) {
                std::string values_part = remaining_sql.substr(values_pos + 6); // Skip "VALUES"
                values_part = trim(values_part);

                if (values_part.length() > 2 && values_part.front() == '(' && values_part.back() == ')') {
                    std::string actual_values_str = values_part.substr(1, values_part.length() - 2);
                    std::stringstream vals_ss(actual_values_str);
                    std::string val_token;

                    while (std::getline(vals_ss, val_token, ',')) {
                        statement->insert_statement->values.push_back(trim(val_token));
                    }
                }
            }
        }
    } else if (to_upper(token) == "SELECT") {
        ss >> token; // Expecting '*'
        if (token == "*") {
            ss >> token;
            if (to_upper(token) == "FROM") {
                ss >> token; // table_name
                statement->type = STATEMENT_SELECT;
                statement->select_statement = std::make_unique<SelectStatement>();
                statement->select_statement->table_name = token;
                // Further parsing for WHERE clause will go here
            }
        }
    } else if (to_upper(token) == "UPDATE") {
        ss >> token; // table_name
        statement->type = STATEMENT_UPDATE;
        statement->update_statement = std::make_unique<UpdateStatement>();
        statement->update_statement->table_name = token;

        ss >> token; // Expecting "SET"
        if (to_upper(token) == "SET") {
            // For now, just consume the rest of the line as set_clauses and where_clause
            std::string remaining_sql;
            std::getline(ss, remaining_sql);
            remaining_sql.erase(0, remaining_sql.find_first_not_of(" \t\n\r\f\v"));
            statement->update_statement->set_clauses.push_back({"raw_set_clause", remaining_sql});
        }
    } else if (to_upper(token) == "DELETE") {
        ss >> token;
        if (to_upper(token) == "FROM") {
            ss >> token; // table_name
            statement->type = STATEMENT_DELETE;
            statement->delete_statement = std::make_unique<DeleteStatement>();
            statement->delete_statement->table_name = token;

            // For now, just consume the rest of the line as where_clause
            std::string remaining_sql;
            std::getline(ss, remaining_sql);
            remaining_sql.erase(0, remaining_sql.find_first_not_of(" \t\n\r\f\v"));
            if (to_upper(remaining_sql.substr(0, 5)) == "WHERE") {
                statement->delete_statement->where_clause = remaining_sql.substr(5);
            }
        }
    }

    return statement;
}