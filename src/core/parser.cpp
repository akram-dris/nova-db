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

std::string column_type_to_string(ColumnType type) {
    switch (type) {
        case COLUMN_TYPE_INT: return "INT";
        case COLUMN_TYPE_TEXT: return "TEXT";
        case COLUMN_TYPE_UNKNOWN: return "UNKNOWN";
    }
    return "UNKNOWN"; // Should not be reached
}

std::unique_ptr<Statement> parse_statement(const std::string& sql) {
    std::string trimmed_sql = trim(sql);
    if (trimmed_sql.empty()) {
        auto statement = std::make_unique<Statement>();
        statement->type = STATEMENT_UNKNOWN; // Or a new STATEMENT_EMPTY type
        return statement;
    }

    // std::cout << "Parsing SQL: " << sql << std::endl; // Removed for cleaner output
    auto statement = std::make_unique<Statement>();
    statement->type = STATEMENT_UNKNOWN;

    std::stringstream ss(trimmed_sql);
    std::string token;

    ss >> token;
    // Remove trailing semicolon from the first token if present
    if (!token.empty() && token.back() == ';') {
        token.pop_back();
    }

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
                std::string table_name_with_semicolon = token;
                // Remove trailing semicolon if present
                if (!table_name_with_semicolon.empty() && table_name_with_semicolon.back() == ';') {
                    table_name_with_semicolon.pop_back();
                }
                statement->select_statement->table_name = trim(table_name_with_semicolon);
                // Further parsing for WHERE clause will go here
                std::string remaining_sql;
                std::getline(ss, remaining_sql); // Get the rest of the line
                remaining_sql = trim(remaining_sql);

                size_t where_pos = to_upper(remaining_sql).find("WHERE");
                if (where_pos != std::string::npos) {
                    std::string condition_str = remaining_sql.substr(where_pos + 5); // Skip "WHERE"
                    condition_str = trim(condition_str);

                    // Remove trailing semicolon if present
                    if (!condition_str.empty() && condition_str.back() == ';') {
                        condition_str.pop_back();
                        condition_str = trim(condition_str);
                    }

                                    // Parse condition: column_name operator value
                                    size_t op_pos = std::string::npos;
                                    OperatorType op_type = OP_UNKNOWN;
                    
                                    if ((op_pos = condition_str.find("=")) != std::string::npos) {
                                        op_type = OP_EQ;
                                    } else if ((op_pos = condition_str.find(">")) != std::string::npos) {
                                        op_type = OP_GT;
                                    } else if ((op_pos = condition_str.find("<")) != std::string::npos) {
                                        op_type = OP_LT;
                                    }
                                    // Add more operators as needed
                    
                                    if (op_pos != std::string::npos && op_type != OP_UNKNOWN) {
                                        std::string column_name = trim(condition_str.substr(0, op_pos));
                                        std::string value = trim(condition_str.substr(op_pos + 1));
                    
                                        if (!column_name.empty() && !value.empty()) {
                                            statement->select_statement->where_condition = std::make_unique<WhereCondition>();
                                            statement->select_statement->where_condition->column_name = column_name;
                                            statement->select_statement->where_condition->op = op_type;
                                            statement->select_statement->where_condition->value = value;
                                        }
                                    }                }
            }
        }
    } else if (to_upper(token) == "UPDATE") {
        ss >> token; // table_name
        statement->type = STATEMENT_UPDATE;
        statement->update_statement = std::make_unique<UpdateStatement>();
        std::string table_name_with_semicolon = token;
        // Remove trailing semicolon if present
        if (!table_name_with_semicolon.empty() && table_name_with_semicolon.back() == ';') {
            table_name_with_semicolon.pop_back();
        }
        statement->update_statement->table_name = trim(table_name_with_semicolon);

        ss >> token; // Expecting "SET"
        if (to_upper(token) == "SET") {
            std::string remaining_sql;
            std::getline(ss, remaining_sql);
            remaining_sql = trim(remaining_sql);

            // Find WHERE clause position
            size_t where_pos = to_upper(remaining_sql).find("WHERE");
            std::string set_part;
            std::string where_part;

            if (where_pos != std::string::npos) {
                set_part = remaining_sql.substr(0, where_pos);
                where_part = remaining_sql.substr(where_pos + 5); // Skip "WHERE"
            } else {
                set_part = remaining_sql;
            }

            // Parse SET clauses
            std::stringstream set_ss(set_part);
            std::string set_token;
            while (std::getline(set_ss, set_token, ',')) {
                size_t eq_pos = set_token.find("=");
                if (eq_pos != std::string::npos) {
                    std::string column_name = trim(set_token.substr(0, eq_pos));
                    std::string value = trim(set_token.substr(eq_pos + 1));
                    if (!column_name.empty() && !value.empty()) {
                        statement->update_statement->set_clauses.push_back({column_name, value});
                    }
                }
            }

            // Parse WHERE clause
            if (!where_part.empty()) {
                where_part = trim(where_part);
                // Remove trailing semicolon if present
                if (!where_part.empty() && where_part.back() == ';') {
                    where_part.pop_back();
                    where_part = trim(where_part);
                }

                // Parse condition: column_name operator value
                size_t op_pos = std::string::npos;
                OperatorType op_type = OP_UNKNOWN;

                if ((op_pos = where_part.find("=")) != std::string::npos) {
                    op_type = OP_EQ;
                } else if ((op_pos = where_part.find(">")) != std::string::npos) {
                    op_type = OP_GT;
                } else if ((op_pos = where_part.find("<")) != std::string::npos) {
                    op_type = OP_LT;
                }
                // Add more operators as needed

                if (op_pos != std::string::npos && op_type != OP_UNKNOWN) {
                    std::string column_name = trim(where_part.substr(0, op_pos));
                    std::string value = trim(where_part.substr(op_pos + 1));

                    if (!column_name.empty() && !value.empty()) {
                        statement->update_statement->where_condition = std::make_unique<WhereCondition>();
                        statement->update_statement->where_condition->column_name = column_name;
                        statement->update_statement->where_condition->op = op_type;
                        statement->update_statement->where_condition->value = value;
                    }
                }
            }
        }
    } else if (to_upper(token) == "DELETE") {
        ss >> token;
        if (to_upper(token) == "FROM") {
            ss >> token; // table_name
            statement->type = STATEMENT_DELETE;
            statement->delete_statement = std::make_unique<DeleteStatement>();
            std::string table_name_with_semicolon = token;
            // Remove trailing semicolon if present
            if (!table_name_with_semicolon.empty() && table_name_with_semicolon.back() == ';') {
                table_name_with_semicolon.pop_back();
            }
            statement->delete_statement->table_name = trim(table_name_with_semicolon);

            // Parse WHERE clause
            std::string remaining_sql;
            std::getline(ss, remaining_sql);
            remaining_sql = trim(remaining_sql);

            size_t where_pos = to_upper(remaining_sql).find("WHERE");
            if (where_pos != std::string::npos) {
                std::string condition_str = remaining_sql.substr(where_pos + 5); // Skip "WHERE"
                condition_str = trim(condition_str);

                // Remove trailing semicolon if present
                if (!condition_str.empty() && condition_str.back() == ';') {
                    condition_str.pop_back();
                    condition_str = trim(condition_str);
                }

                // Parse condition: column_name operator value
                size_t op_pos = std::string::npos;
                OperatorType op_type = OP_UNKNOWN;

                if ((op_pos = condition_str.find("=")) != std::string::npos) {
                    op_type = OP_EQ;
                } else if ((op_pos = condition_str.find(">")) != std::string::npos) {
                    op_type = OP_GT;
                } else if ((op_pos = condition_str.find("<")) != std::string::npos) {
                    op_type = OP_LT;
                }
                // Add more operators as needed

                if (op_pos != std::string::npos && op_type != OP_UNKNOWN) {
                    std::string column_name = trim(condition_str.substr(0, op_pos));
                    std::string value = trim(condition_str.substr(op_pos + 1));

                    if (!column_name.empty() && !value.empty()) {
                        statement->delete_statement->where_condition = std::make_unique<WhereCondition>();
                        statement->delete_statement->where_condition->column_name = column_name;
                        statement->delete_statement->where_condition->op = op_type;
                        statement->delete_statement->where_condition->value = value;
                    }
                }
            }
        }
    } else if (to_upper(token) == "BEGIN") {
        std::string next_token;
        ss >> next_token; // Expecting "TRANSACTION"
        next_token = trim(next_token);
        if (!next_token.empty() && next_token.back() == ';') {
            next_token.pop_back();
        }
        if (to_upper(next_token) == "TRANSACTION") {
            statement->type = STATEMENT_BEGIN_TRANSACTION;
            statement->begin_transaction_statement = std::make_unique<BeginTransactionStatement>();
        }
    } else if (to_upper(token) == "COMMIT") {
        std::string next_token;
        ss >> next_token;
        next_token = trim(next_token);
        if (!next_token.empty() && next_token.back() == ';') {
            next_token.pop_back();
        }
        if (to_upper(next_token) == "TRANSACTION" || next_token.empty()) { // Allow "COMMIT" or "COMMIT TRANSACTION"
            statement->type = STATEMENT_COMMIT_TRANSACTION;
            statement->commit_transaction_statement = std::make_unique<CommitTransactionStatement>();
        }
    } else if (to_upper(token) == "ROLLBACK") {
        std::string next_token;
        ss >> next_token;
        next_token = trim(next_token);
        if (!next_token.empty() && next_token.back() == ';') {
            next_token.pop_back();
        }
        if (to_upper(next_token) == "TRANSACTION" || next_token.empty()) { // Allow "ROLLBACK" or "ROLLBACK TRANSACTION"
            statement->type = STATEMENT_ROLLBACK_TRANSACTION;
            statement->rollback_transaction_statement = std::make_unique<RollbackTransactionStatement>();
        }
    }

    return statement;
}
