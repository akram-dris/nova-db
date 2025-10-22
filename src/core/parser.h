#ifndef NOVADB_PARSER_H
#define NOVADB_PARSER_H

#include <string>
#include <vector>
#include <memory>

enum StatementType {
    STATEMENT_CREATE_TABLE,
    STATEMENT_INSERT,
    STATEMENT_SELECT,
    STATEMENT_UPDATE,
    STATEMENT_DELETE,
    STATEMENT_BEGIN_TRANSACTION,
    STATEMENT_COMMIT_TRANSACTION,
    STATEMENT_ROLLBACK_TRANSACTION,
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

enum OperatorType {
    OP_EQ, // Equal
    OP_NE, // Not Equal
    OP_GT, // Greater Than
    OP_LT, // Less Than
    OP_GE, // Greater Than or Equal
    OP_LE, // Less Than or Equal
    OP_UNKNOWN
};

struct WhereCondition {
    std::string column_name;
    OperatorType op;
    std::string value; // Value as a string, will be converted based on column type
};

struct SelectStatement {
    std::string table_name;
    std::unique_ptr<WhereCondition> where_condition;
};

struct UpdateStatement {
    std::string table_name;
    std::vector<std::pair<std::string, std::string>> set_clauses; // e.g., {{"column", "value"}}
    std::unique_ptr<WhereCondition> where_condition;
};

struct DeleteStatement {
    std::string table_name;
    std::unique_ptr<WhereCondition> where_condition;
};

struct BeginTransactionStatement {};
struct CommitTransactionStatement {};
struct RollbackTransactionStatement {};

struct Statement {
    StatementType type;
    std::unique_ptr<CreateTableStatement> create_table_statement;
    std::unique_ptr<InsertStatement> insert_statement;
    std::unique_ptr<SelectStatement> select_statement;
    std::unique_ptr<UpdateStatement> update_statement;
    std::unique_ptr<DeleteStatement> delete_statement;
    std::unique_ptr<BeginTransactionStatement> begin_transaction_statement;
    std::unique_ptr<CommitTransactionStatement> commit_transaction_statement;
    std::unique_ptr<RollbackTransactionStatement> rollback_transaction_statement;
    // Add more unique_ptrs for other statement types as needed
};

// Function to parse an SQL statement
std::unique_ptr<Statement> parse_statement(const std::string& sql);

#endif //NOVADB_PARSER_H