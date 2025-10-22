#ifndef NOVADB_SERIALIZER_H
#define NOVADB_SERIALIZER_H

#include <vector>
#include <string>
#include <cstdint>
#include "parser.h" // For ColumnType, ColumnDefinition, CreateTableStatement
#include "index.h" // For BTreeNode

// Basic serialization functions
void serialize_string(std::vector<char>& buffer, size_t& offset, const std::string& value);
std::string deserialize_string(const std::vector<char>& buffer, size_t& offset);

void serialize_int(std::vector<char>& buffer, size_t& offset, int32_t value);
int32_t deserialize_int(const std::vector<char>& buffer, size_t& offset);

// ColumnType serialization
void serialize_column_type(std::vector<char>& buffer, size_t& offset, ColumnType type);
ColumnType deserialize_column_type(const std::vector<char>& buffer, size_t& offset);
ColumnType string_to_column_type(const std::string& type_str);

// ColumnDefinition serialization
void serialize_column_definition(std::vector<char>& buffer, size_t& offset, const ColumnDefinition& col_def);
ColumnDefinition deserialize_column_definition(const std::vector<char>& buffer, size_t& offset);

// CreateTableStatement serialization (for schema storage)
void serialize_create_table_statement(std::vector<char>& buffer, size_t& offset, const CreateTableStatement& stmt);
std::unique_ptr<CreateTableStatement> deserialize_create_table_statement(const std::vector<char>& buffer, size_t& offset);

// Value serialization
void serialize_value(std::vector<char>& buffer, size_t& offset, ColumnType type, const std::string& value_str);
std::string deserialize_value(const std::vector<char>& buffer, size_t& offset, ColumnType type);

// BTreeNode serialization
void serialize_btree_node(std::vector<char>& buffer, size_t& offset, const BTreeNode& node);
std::unique_ptr<BTreeNode> deserialize_btree_node(const std::vector<char>& buffer, size_t& offset);

#endif //NOVADB_SERIALIZER_H