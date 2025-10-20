#include "serializer.h"
#include <cstring> // For std::memcpy
#include <stdexcept>

// --- String Serialization ---
void serialize_string(std::vector<char>& buffer, size_t& offset, const std::string& value) {
    // Write string length
    uint32_t len = value.length();
    if (offset + sizeof(len) > buffer.size()) {
        throw std::out_of_range("Buffer overflow during string length serialization");
    }
    std::memcpy(buffer.data() + offset, &len, sizeof(len));
    offset += sizeof(len);

    // Write string data
    if (offset + len > buffer.size()) {
        throw std::out_of_range("Buffer overflow during string data serialization");
    }
    std::memcpy(buffer.data() + offset, value.data(), len);
    offset += len;
}

std::string deserialize_string(const std::vector<char>& buffer, size_t& offset) {
    // Read string length
    uint32_t len;
    if (offset + sizeof(len) > buffer.size()) {
        throw std::out_of_range("Buffer underflow during string length deserialization");
    }
    std::memcpy(&len, buffer.data() + offset, sizeof(len));
    offset += sizeof(len);

    // Read string data
    if (offset + len > buffer.size()) {
        throw std::out_of_range("Buffer underflow during string data deserialization");
    }
    std::string value(buffer.data() + offset, len);
    offset += len;
    return value;
}

// --- Integer Serialization ---
void serialize_int(std::vector<char>& buffer, size_t& offset, int32_t value) {
    if (offset + sizeof(value) > buffer.size()) {
        throw std::out_of_range("Buffer overflow during int serialization");
    }
    std::memcpy(buffer.data() + offset, &value, sizeof(value));
    offset += sizeof(value);
}

int32_t deserialize_int(const std::vector<char>& buffer, size_t& offset) {
    int32_t value;
    if (offset + sizeof(value) > buffer.size()) {
        throw std::out_of_range("Buffer underflow during int deserialization");
    }
    std::memcpy(&value, buffer.data() + offset, sizeof(value));
    offset += sizeof(value);
    return value;
}

// --- ColumnType Serialization ---
void serialize_column_type(std::vector<char>& buffer, size_t& offset, ColumnType type) {
    serialize_int(buffer, offset, static_cast<int32_t>(type));
}

ColumnType deserialize_column_type(const std::vector<char>& buffer, size_t& offset) {
    return static_cast<ColumnType>(deserialize_int(buffer, offset));
}

// --- ColumnDefinition Serialization ---
void serialize_column_definition(std::vector<char>& buffer, size_t& offset, const ColumnDefinition& col_def) {
    serialize_string(buffer, offset, col_def.name);
    serialize_column_type(buffer, offset, col_def.type);
}

ColumnDefinition deserialize_column_definition(const std::vector<char>& buffer, size_t& offset) {
    ColumnDefinition col_def;
    col_def.name = deserialize_string(buffer, offset);
    col_def.type = deserialize_column_type(buffer, offset);
    return col_def;
}

// --- CreateTableStatement Serialization ---
void serialize_create_table_statement(std::vector<char>& buffer, size_t& offset, const CreateTableStatement& stmt) {
    serialize_string(buffer, offset, stmt.table_name);
    serialize_int(buffer, offset, stmt.columns.size());
    for (const auto& col_def : stmt.columns) {
        serialize_column_definition(buffer, offset, col_def);
    }
}

std::unique_ptr<CreateTableStatement> deserialize_create_table_statement(const std::vector<char>& buffer, size_t& offset) {
    auto stmt = std::make_unique<CreateTableStatement>();
    stmt->table_name = deserialize_string(buffer, offset);
    int32_t num_columns = deserialize_int(buffer, offset);
    for (int i = 0; i < num_columns; ++i) {
        stmt->columns.push_back(deserialize_column_definition(buffer, offset));
    }
    return stmt;
}

// --- Value Serialization ---
void serialize_value(std::vector<char>& buffer, size_t& offset, ColumnType type, const std::string& value_str) {
    switch (type) {
        case COLUMN_TYPE_INT: {
            try {
                int32_t val = std::stoi(value_str);
                serialize_int(buffer, offset, val);
            } catch (const std::exception& e) {
                throw std::runtime_error("Invalid INT value: " + value_str);
            }
            break;
        }
        case COLUMN_TYPE_TEXT: {
            // Remove surrounding quotes if present
            std::string cleaned_str = value_str;
            if (cleaned_str.length() >= 2 && cleaned_str.front() == '\'' && cleaned_str.back() == '\'') {
                cleaned_str = cleaned_str.substr(1, cleaned_str.length() - 2);
            }
            serialize_string(buffer, offset, cleaned_str);
            break;
        }
        case COLUMN_TYPE_UNKNOWN:
            throw std::runtime_error("Cannot serialize UNKNOWN column type");
    }
}

std::string deserialize_value(const std::vector<char>& buffer, size_t& offset, ColumnType type) {
    switch (type) {
        case COLUMN_TYPE_INT: {
            int32_t val = deserialize_int(buffer, offset);
            return std::to_string(val);
        }
        case COLUMN_TYPE_TEXT: {
            return deserialize_string(buffer, offset);
        }
        case COLUMN_TYPE_UNKNOWN:
            throw std::runtime_error("Cannot deserialize UNKNOWN column type");
    }
    return ""; // Should not be reached
}