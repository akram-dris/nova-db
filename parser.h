#ifndef NOVADB_PARSER_H
#define NOVADB_PARSER_H

#include <string>
#include <vector>
#include <memory>

// Forward declarations
struct Statement;

enum StatementType {
    STATEMENT_CREATE_TABLE,
    STATEMENT_INSERT,
    STATEMENT_SELECT,
    STATEMENT_UPDATE,
    STATEMENT_DELETE,
    STATEMENT_UNKNOWN
};

enum ColumnType {
    COLUMN_TYPE_INT,
    COLUMN_TYPE_TEXT,
    COLUMN_TYPE_UNKNOWN
};

std::string column_type_to_string(ColumnType type);

struct ColumnDefinition {
    std::string name;
    ColumnType type;
    // Add constraints like PRIMARY KEY, NOT NULL later
};

struct CreateTableStatement {
    std::string table_name;
    std::vector<ColumnDefinition> columns;
};

struct InsertStatement {
    std::string table_name;
    std::vector<std::string> values;
};

struct SelectStatement {
    std::string table_name;
    std::string where_clause; // Placeholder for now
};

struct UpdateStatement {
    std::string table_name;
    std::vector<std::pair<std::string, std::string>> set_clauses; // e.g., {{"column", "value"}}
    std::string where_clause; // Placeholder for now
};

struct DeleteStatement {
    std::string table_name;
    std::string where_clause; // Placeholder for now
};

struct Statement {
    StatementType type;
    std::unique_ptr<CreateTableStatement> create_table_statement;
    std::unique_ptr<InsertStatement> insert_statement;
    std::unique_ptr<SelectStatement> select_statement;
    std::unique_ptr<UpdateStatement> update_statement;
    std::unique_ptr<DeleteStatement> delete_statement;
    // Add more unique_ptrs for other statement types as needed
};

// Function to parse an SQL statement
std::unique_ptr<Statement> parse_statement(const std::string& sql);

#endif //NOVADB_PARSER_H
