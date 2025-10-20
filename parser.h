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

struct ColumnDefinition {
    std::string name;
    std::string type;
    // Add constraints like PRIMARY KEY, NOT NULL later
};

struct CreateTableStatement {
    std::string table_name;
    std::vector<ColumnDefinition> columns;
};

struct Statement {
    StatementType type;
    std::unique_ptr<CreateTableStatement> create_table_statement;
    // Add more unique_ptrs for other statement types as needed
};

// Function to parse an SQL statement
std::unique_ptr<Statement> parse_statement(const std::string& sql);

#endif //NOVADB_PARSER_H
