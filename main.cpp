#include <iostream>
#include <string>
#include <fstream>
#include <memory>
#include "parser.h"
#include "pager.h"
#include "serializer.h"

std::unique_ptr<Pager> current_pager = nullptr;

void print_prompt() {
    std::cout << "nova> ";
}

int main() {
    std::string input_line;

    while (true) {
        print_prompt();
        if (!std::getline(std::cin, input_line)) {
            break; // End of input
        }

        if (input_line.rfind(".open", 0) == 0) {
            std::string filename = input_line.substr(6);
            try {
                current_pager = std::make_unique<Pager>(filename);
                std::cout << "Database '" << filename << "' opened successfully." << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Error opening database: " << e.what() << std::endl;
            }
        } else if (input_line == ".exit") {
            std::cout << "Goodbye from NovaDB." << std::endl;
            break;
        } else if (input_line == ".tables") {
            if (!current_pager) {
                std::cout << "Error: No database open. Use .open <filename.db>" << std::endl;
                continue;
            }
            std::cout << "Tables:" << std::endl;
            try {
                std::vector<char> metadata_page = current_pager->read_page(METADATA_PAGE_NUM);
                size_t offset = 0;
                // Read number of tables
                int32_t num_tables = deserialize_int(metadata_page, offset);
                for (int i = 0; i < num_tables; ++i) {
                    auto table_schema = deserialize_create_table_statement(metadata_page, offset);
                    std::cout << "- " << table_schema->table_name << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error reading metadata: " << e.what() << std::endl;
            }
        } else if (input_line.rfind(".schema", 0) == 0) {
            if (!current_pager) {
                std::cout << "Error: No database open. Use .open <filename.db>" << std::endl;
                continue;
            }
            std::string table_name_to_find = input_line.substr(7);
            // Trim leading whitespace
            table_name_to_find.erase(0, table_name_to_find.find_first_not_of(" \t\n\r\f\v"));

            try {
                std::vector<char> metadata_page = current_pager->read_page(METADATA_PAGE_NUM);
                size_t offset = 0;
                                    int32_t num_tables = deserialize_int(metadata_page, offset);
                                    bool found_specific_table = false;
                
                                    for (int i = 0; i < num_tables; ++i) {
                                        auto table_schema = deserialize_create_table_statement(metadata_page, offset);
                    if (table_name_to_find.empty()) { // Display all schemas
                        std::cout << "Table: " << table_schema->table_name << std::endl;
                        std::cout << "  Columns:" << std::endl;
                        for (const auto& col : table_schema->columns) {
                            std::cout << "    - " << col.name << " (Type: " << column_type_to_string(col.type) << ")" << std::endl;
                        }
                    } else if (table_schema->table_name == table_name_to_find) { // Display specific schema
                        std::cout << "Table: " << table_schema->table_name << std::endl;
                        std::cout << "  Columns:" << std::endl;
                        for (const auto& col : table_schema->columns) {
                            std::cout << "    - " << col.name << " (Type: " << column_type_to_string(col.type) << ")" << std::endl;
                        }
                        found_specific_table = true;
                        break; // Stop after finding the specific table
                    }
                }

                if (!found_specific_table && !table_name_to_find.empty()) {
                    std::cout << "Error: Table '" << table_name_to_find << "' not found." << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error reading metadata for schema: " << e.what() << std::endl;
            }
        } else {
            // Attempt to parse SQL statement
            auto statement = parse_statement(input_line);
            if (statement->type == STATEMENT_CREATE_TABLE) {
                std::cout << "Parsed CREATE TABLE statement for table: " << statement->create_table_statement->table_name << ", columns: ";
                for (const auto& col : statement->create_table_statement->columns) {
                    std::cout << col.name << " (" << column_type_to_string(col.type) << ") ";
                }
                std::cout << std::endl;

                // Store schema in metadata page
                if (!current_pager) {
                    std::cout << "Error: No database open. Use .open <filename.db>" << std::endl;
                    continue;
                }
                try {
                    std::vector<char> metadata_page_buffer(PAGE_SIZE, 0); // Buffer for new metadata page
                    size_t write_offset = 0;

                    // Read existing schemas
                    std::vector<std::unique_ptr<CreateTableStatement>> existing_schemas;
                    int32_t num_existing_tables = 0;

                    if (current_pager->get_num_pages() > METADATA_PAGE_NUM) {
                        std::vector<char> existing_metadata_page = current_pager->read_page(METADATA_PAGE_NUM);
                        size_t read_offset = 0;
                        num_existing_tables = deserialize_int(existing_metadata_page, read_offset); // Read num_tables and advance read_offset
                        for (int i = 0; i < num_existing_tables; ++i) {
                            existing_schemas.push_back(deserialize_create_table_statement(existing_metadata_page, read_offset));
                        }
                    }

                    // Skip space for num_tables for now, will write it at the end
                    write_offset += sizeof(int32_t);

                    // Serialize existing schemas
                    for (const auto& schema : existing_schemas) {
                        serialize_create_table_statement(metadata_page_buffer, write_offset, *schema);
                    }

                    // Serialize the new schema
                    serialize_create_table_statement(metadata_page_buffer, write_offset, *statement->create_table_statement);

                    // Now, go back and write the total number of tables at the beginning
                    size_t num_tables_offset = 0;
                    serialize_int(metadata_page_buffer, num_tables_offset, num_existing_tables + 1);

                    current_pager->write_page(METADATA_PAGE_NUM, metadata_page_buffer);
                    std::cout << "Table '" << statement->create_table_statement->table_name << "' schema stored." << std::endl;

                } catch (const std::exception& e) {
                    std::cerr << "Error storing table schema: " << e.what() << std::endl;
                }

            } else if (statement->type == STATEMENT_INSERT) {
                std::cout << "Parsed INSERT statement for table: " << statement->insert_statement->table_name << ", values: ";
                for (const auto& val : statement->insert_statement->values) {
                    std::cout << val << " ";
                }
                std::cout << std::endl;

                if (!current_pager) {
                    std::cout << "Error: No database open. Use .open <filename.db>" << std::endl;
                    continue;
                }

                try {
                    // 1. Retrieve Table Schema
                    std::unique_ptr<CreateTableStatement> table_schema = nullptr;
                    std::vector<char> metadata_page = current_pager->read_page(METADATA_PAGE_NUM);
                    size_t read_offset = 0;
                    int32_t num_tables = deserialize_int(metadata_page, read_offset);

                    for (int i = 0; i < num_tables; ++i) {
                        auto current_schema = deserialize_create_table_statement(metadata_page, read_offset);
                        if (current_schema->table_name == statement->insert_statement->table_name) {
                            table_schema = std::move(current_schema);
                            break;
                        }
                    }

                    if (!table_schema) {
                        std::cout << "Error: Table '" << statement->insert_statement->table_name << "' not found." << std::endl;
                        continue;
                    }

                    // 2. Validate INSERT Statement
                    if (statement->insert_statement->values.size() != table_schema->columns.size()) {
                        std::cout << "Error: Number of values does not match number of columns in table '" << table_schema->table_name << "'." << std::endl;
                        continue;
                    }

                    // 3. Serialize Record
                    std::vector<char> record_page(PAGE_SIZE, 0); // One record per page for simplicity
                    size_t write_offset = 0;

                    for (size_t i = 0; i < table_schema->columns.size(); ++i) {
                        const auto& col_def = table_schema->columns[i];
                        const std::string& value_str = statement->insert_statement->values[i];

                        // Basic type validation and serialization
                        try {
                            serialize_value(record_page, write_offset, col_def.type, value_str);
                        } catch (const std::exception& e) {
                            std::cout << "Error: Invalid value for column '" << col_def.name << "' (" << column_type_to_string(col_def.type) << "): " << e.what() << std::endl;
                            continue; // Skip this record or handle error appropriately
                        }
                    }

                    // 4. Store Record
                    int page_to_write = current_pager->get_num_pages(); // Append to the end of the file
                    current_pager->write_page(page_to_write, record_page);
                    std::cout << "Record inserted into table '" << table_schema->table_name << "' on page " << page_to_write << "." << std::endl;

                } catch (const std::exception& e) {
                    std::cerr << "Error executing INSERT statement: " << e.what() << std::endl;
                }
            } else if (statement->type == STATEMENT_SELECT) {
                std::cout << "Parsed SELECT statement for table: " << statement->select_statement->table_name << std::endl;

                if (!current_pager) {
                    std::cout << "Error: No database open. Use .open <filename.db>" << std::endl;
                    continue;
                }

                try {
                    // 1. Retrieve Table Schema
                    std::unique_ptr<CreateTableStatement> table_schema = nullptr;
                    std::vector<char> metadata_page = current_pager->read_page(METADATA_PAGE_NUM);
                    size_t read_offset = 0;
                    int32_t num_tables = deserialize_int(metadata_page, read_offset);

                    for (int i = 0; i < num_tables; ++i) {
                        auto current_schema = deserialize_create_table_statement(metadata_page, read_offset);
                        if (current_schema->table_name == statement->select_statement->table_name) {
                            table_schema = std::move(current_schema);
                            break;
                        }
                    }

                    if (!table_schema) {
                        std::cout << "Error: Table '" << statement->select_statement->table_name << "' not found." << std::endl;
                        continue;
                    }

                    // Print column headers
                    for (size_t i = 0; i < table_schema->columns.size(); ++i) {
                        std::cout << table_schema->columns[i].name;
                        if (i < table_schema->columns.size() - 1) {
                            std::cout << " | ";
                        }
                    }
                    std::cout << std::endl;
                    for (size_t i = 0; i < table_schema->columns.size(); ++i) {
                        for (size_t j = 0; j < table_schema->columns[i].name.length(); ++j) {
                            std::cout << "-";
                        }
                        if (i < table_schema->columns.size() - 1) {
                            std::cout << "-|- ";
                        }
                    }
                    std::cout << std::endl;

                    // 3. Table Scan and Record Deserialization
                    // Assuming records are stored one per page, starting from page 1 (after metadata page 0)
                    for (int page_num = METADATA_PAGE_NUM + 1; page_num < current_pager->get_num_pages(); ++page_num) {
                        std::vector<char> record_page = current_pager->read_page(page_num);

                        // Skip page if it's entirely zeros (heuristic for deleted records)
                        bool is_zero_page = true;
                        for (char c : record_page) {
                            if (c != 0) {
                                is_zero_page = false;
                                break;
                            }
                        }
                        if (is_zero_page) {
                            continue; // Skip this page, it's considered deleted
                        }

                        size_t record_read_offset = 0;

                        std::vector<std::string> row_values;
                        bool condition_met = true; // Assume true if no WHERE clause or condition is met

                        // Deserialize all values for the current record
                        for (size_t i = 0; i < table_schema->columns.size(); ++i) {
                            const auto& col_def = table_schema->columns[i];
                            row_values.push_back(deserialize_value(record_page, record_read_offset, col_def.type));
                        }

                        // Evaluate WHERE condition if present
                        if (statement->select_statement->where_condition) {
                            condition_met = false; // Reset to false, must be explicitly met

                            const auto& wc = statement->select_statement->where_condition;
                            int column_index = -1;
                            for (size_t i = 0; i < table_schema->columns.size(); ++i) {
                                if (table_schema->columns[i].name == wc->column_name) {
                                    column_index = i;
                                    break;
                                }
                            }

                            if (column_index != -1) {
                                const auto& col_def = table_schema->columns[column_index];
                                const std::string& record_value = row_values[column_index];

                                // For now, only handle OP_EQ
                                if (wc->op == OP_EQ) {
                                    if (col_def.type == COLUMN_TYPE_INT) {
                                        try {
                                            if (std::stoi(record_value) == std::stoi(wc->value)) {
                                                condition_met = true;
                                            }
                                        } catch (const std::exception& e) {
                                            // Handle conversion error, e.g., log it
                                            std::cerr << "Warning: Type conversion error in WHERE clause for INT comparison: " << e.what() << std::endl;
                                        }
                                    } else if (col_def.type == COLUMN_TYPE_TEXT) {
                                        // Remove surrounding quotes from wc->value if present
                                        std::string cleaned_wc_value = wc->value;
                                        if (cleaned_wc_value.length() >= 2 && cleaned_wc_value.front() == '\'' && cleaned_wc_value.back() == '\'') {
                                            cleaned_wc_value = cleaned_wc_value.substr(1, cleaned_wc_value.length() - 2);
                                        }
                                        if (record_value == cleaned_wc_value) {
                                            condition_met = true;
                                        }
                                    }
                                }
                            }
                        }

                        // 4. Display Results (only if condition met)
                        if (condition_met) {
                            for (size_t i = 0; i < row_values.size(); ++i) {
                                std::cout << row_values[i];
                                if (i < row_values.size() - 1) {
                                    std::cout << " | ";
                                }
                            }
                            std::cout << std::endl;
                        }
                    }

                } catch (const std::exception& e) {
                    std::cerr << "Error executing SELECT statement: " << e.what() << std::endl;
                }
            } else if (statement->type == STATEMENT_UPDATE) {
                if (!current_pager) {
                    std::cout << "Error: No database open. Use .open <filename.db>" << std::endl;
                    continue;
                }

                try {
                    // 1. Retrieve Table Schema
                    std::unique_ptr<CreateTableStatement> table_schema = nullptr;
                    std::vector<char> metadata_page = current_pager->read_page(METADATA_PAGE_NUM);
                    size_t read_offset = 0;
                    int32_t num_tables = deserialize_int(metadata_page, read_offset);

                    for (int i = 0; i < num_tables; ++i) {
                        auto current_schema = deserialize_create_table_statement(metadata_page, read_offset);
                        if (current_schema->table_name == statement->update_statement->table_name) {
                            table_schema = std::move(current_schema);
                            break;
                        }
                    }

                    if (!table_schema) {
                        std::cout << "Error: Table '" << statement->update_statement->table_name << "' not found." << std::endl;
                        continue;
                    }

                    int updated_rows = 0;
                    // 2. Table Scan and Record Update
                    for (int page_num = METADATA_PAGE_NUM + 1; page_num < current_pager->get_num_pages(); ++page_num) {
                        std::vector<char> record_page = current_pager->read_page(page_num);
                        size_t record_read_offset = 0;

                        std::vector<std::string> row_values;
                        // Deserialize all values for the current record
                        for (size_t i = 0; i < table_schema->columns.size(); ++i) {
                            const auto& col_def = table_schema->columns[i];
                            row_values.push_back(deserialize_value(record_page, record_read_offset, col_def.type));
                        }

                        bool condition_met = true; // Assume true if no WHERE clause
                        // Evaluate WHERE condition if present
                        if (statement->update_statement->where_condition) {
                            condition_met = false; // Reset to false, must be explicitly met

                            const auto& wc = statement->update_statement->where_condition;
                            int column_index = -1;
                            for (size_t i = 0; i < table_schema->columns.size(); ++i) {
                                if (table_schema->columns[i].name == wc->column_name) {
                                    column_index = i;
                                    break;
                                }
                            }

                            if (column_index != -1) {
                                const auto& col_def = table_schema->columns[column_index];
                                const std::string& record_value = row_values[column_index];

                                // For now, only handle OP_EQ
                                if (wc->op == OP_EQ) {
                                    if (col_def.type == COLUMN_TYPE_INT) {
                                        try {
                                            if (std::stoi(record_value) == std::stoi(wc->value)) {
                                                condition_met = true;
                                            }
                                        } catch (const std::exception& e) {
                                            std::cerr << "Warning: Type conversion error in WHERE clause for INT comparison: " << e.what() << std::endl;
                                        }
                                    } else if (col_def.type == COLUMN_TYPE_TEXT) {
                                        std::string cleaned_wc_value = wc->value;
                                        if (cleaned_wc_value.length() >= 2 && cleaned_wc_value.front() == '\'' && cleaned_wc_value.back() == '\'') {
                                            cleaned_wc_value = cleaned_wc_value.substr(1, cleaned_wc_value.length() - 2);
                                        }
                                        if (record_value == cleaned_wc_value) {
                                            condition_met = true;
                                        }
                                    }
                                }
                            }
                        }

                        if (condition_met) {
                            // Apply SET clauses
                            for (const auto& set_pair : statement->update_statement->set_clauses) {
                                int column_index_to_update = -1;
                                for (size_t i = 0; i < table_schema->columns.size(); ++i) {
                                    if (table_schema->columns[i].name == set_pair.first) {
                                        column_index_to_update = i;
                                        break;
                                    }
                                }

                                if (column_index_to_update != -1) {
                                    row_values[column_index_to_update] = set_pair.second;
                                } else {
                                    std::cerr << "Warning: Column '" << set_pair.first << "' not found in table '" << table_schema->table_name << "'." << std::endl;
                                }
                            }

                            // Serialize and Write Back
                            std::vector<char> updated_record_page(PAGE_SIZE, 0);
                            size_t updated_write_offset = 0;
                            for (size_t i = 0; i < row_values.size(); ++i) {
                                const auto& col_def = table_schema->columns[i];
                                try {
                                    serialize_value(updated_record_page, updated_write_offset, col_def.type, row_values[i]);
                                } catch (const std::exception& e) {
                                    std::cerr << "Error: Failed to serialize updated value for column '" << col_def.name << "': " << e.what() << std::endl;
                                    // Decide how to handle this error: skip update for this record, or rollback
                                    // For now, we'll just log and continue, potentially leaving a corrupted record
                                }
                            }
                            current_pager->write_page(page_num, updated_record_page);
                            updated_rows++;
                        }
                    }
                    std::cout << "Updated " << updated_rows << " rows in table '" << table_schema->table_name << "'." << std::endl;

                } catch (const std::exception& e) {
                    std::cerr << "Error executing UPDATE statement: " << e.what() << std::endl;
                }
            } else if (statement->type == STATEMENT_DELETE) {
                std::cout << "Parsed DELETE statement for table: " << statement->delete_statement->table_name;
                if (statement->delete_statement->where_condition) {
                    std::cout << ", WHERE condition: column_name=" << statement->delete_statement->where_condition->column_name
                              << ", op=" << statement->delete_statement->where_condition->op
                              << ", value=" << statement->delete_statement->where_condition->value;
                }
                std::cout << std::endl;

                if (!current_pager) {
                    std::cout << "Error: No database open. Use .open <filename.db>" << std::endl;
                    continue;
                }

                try {
                    // 1. Retrieve Table Schema
                    std::unique_ptr<CreateTableStatement> table_schema = nullptr;
                    std::vector<char> metadata_page = current_pager->read_page(METADATA_PAGE_NUM);
                    size_t read_offset = 0;
                    int32_t num_tables = deserialize_int(metadata_page, read_offset);

                    for (int i = 0; i < num_tables; ++i) {
                        auto current_schema = deserialize_create_table_statement(metadata_page, read_offset);
                        if (current_schema->table_name == statement->delete_statement->table_name) {
                            table_schema = std::move(current_schema);
                            break;
                        }
                    }

                    if (!table_schema) {
                        std::cout << "Error: Table '" << statement->delete_statement->table_name << "' not found." << std::endl;
                        continue;
                    }

                    int deleted_rows = 0;
                    // 2. Table Scan and Record Deletion
                    for (int page_num = METADATA_PAGE_NUM + 1; page_num < current_pager->get_num_pages(); ++page_num) {
                        std::vector<char> record_page = current_pager->read_page(page_num);

                        // Check if page is already zeroed out (considered deleted)
                        bool is_zero_page = true;
                        for (char c : record_page) {
                            if (c != 0) {
                                is_zero_page = false;
                                break;
                            }
                        }
                        if (is_zero_page) {
                            continue; // Skip this page, it's already deleted
                        }

                        size_t record_read_offset = 0;

                        std::vector<std::string> row_values;
                        // Deserialize all values for the current record
                        for (size_t i = 0; i < table_schema->columns.size(); ++i) {
                            const auto& col_def = table_schema->columns[i];
                            row_values.push_back(deserialize_value(record_page, record_read_offset, col_def.type));
                        }

                        bool condition_met = true; // Assume true if no WHERE clause
                        // Evaluate WHERE condition if present
                        if (statement->delete_statement->where_condition) {
                            condition_met = false; // Reset to false, must be explicitly met

                            const auto& wc = statement->delete_statement->where_condition;
                            int column_index = -1;
                            for (size_t i = 0; i < table_schema->columns.size(); ++i) {
                                if (table_schema->columns[i].name == wc->column_name) {
                                    column_index = i;
                                    break;
                                }
                            }

                            if (column_index != -1) {
                                const auto& col_def = table_schema->columns[column_index];
                                const std::string& record_value = row_values[column_index];

                                // For now, only handle OP_EQ
                                if (wc->op == OP_EQ) {
                                    if (col_def.type == COLUMN_TYPE_INT) {
                                        try {
                                            if (std::stoi(record_value) == std::stoi(wc->value)) {
                                                condition_met = true;
                                            }
                                        } catch (const std::exception& e) {
                                            std::cerr << "Warning: Type conversion error in WHERE clause for INT comparison: " << e.what() << std::endl;
                                        }
                                    } else if (col_def.type == COLUMN_TYPE_TEXT) {
                                        std::string cleaned_wc_value = wc->value;
                                        if (cleaned_wc_value.length() >= 2 && cleaned_wc_value.front() == '\'' && cleaned_wc_value.back() == '\'') {
                                            cleaned_wc_value = cleaned_wc_value.substr(1, cleaned_wc_value.length() - 2);
                                        }
                                        if (record_value == cleaned_wc_value) {
                                            condition_met = true;
                                        }
                                    }
                                }
                            }
                        }

                        if (condition_met) {
                            // Mark for Deletion: Overwrite page with zeros
                            std::vector<char> empty_page(PAGE_SIZE, 0);
                            current_pager->write_page(page_num, empty_page);
                            deleted_rows++;
                        }
                    }
                    std::cout << "Deleted " << deleted_rows << " rows from table '" << table_schema->table_name << "'." << std::endl;

                } catch (const std::exception& e) {
                    std::cerr << "Error executing DELETE statement: " << e.what() << std::endl;
                }
            } else {
                std::cout << "Unrecognized command or SQL statement: " << input_line << std::endl;
            }
        }
    }

    return 0;
}
