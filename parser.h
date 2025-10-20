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

struct Statement {
    StatementType type;
    // Add more fields as needed for specific statement types
};

// Function to parse an SQL statement
std::unique_ptr<Statement> parse_statement(const std::string& sql);

#endif //NOVADB_PARSER_H
