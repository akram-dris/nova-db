#include "parser.h"
#include <iostream>

std::unique_ptr<Statement> parse_statement(const std::string& sql) {
    std::cout << "Parsing SQL: " << sql << std::endl;
    // Placeholder for actual parsing logic
    auto statement = std::make_unique<Statement>();
    statement->type = STATEMENT_UNKNOWN;
    return statement;
}
