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
        } else {
            // Attempt to parse SQL statement
            auto statement = parse_statement(input_line);
            if (statement->type == STATEMENT_CREATE_TABLE) {
                std::cout << "Parsed CREATE TABLE statement for table: " << statement->create_table_statement->table_name << ", columns: ";
                for (const auto& col : statement->create_table_statement->columns) {
                    std::cout << col.name << " (" << col.type << ") ";
                }
                std::cout << std::endl;

                // Store schema in metadata page
                if (!current_pager) {
                    std::cout << "Error: No database open. Use .open <filename.db>" << std::endl;
                    continue;
                }
                try {
                    std::vector<char> metadata_page(PAGE_SIZE, 0); // Initialize with zeros
                    size_t offset = 0;

                    // Read existing number of tables
                    int32_t num_tables = 0;
                    if (current_pager->get_num_pages() > METADATA_PAGE_NUM) {
                        std::vector<char> existing_metadata = current_pager->read_page(METADATA_PAGE_NUM);
                        size_t read_offset = 0;
                        num_tables = deserialize_int(existing_metadata, read_offset);
                    }

                    // Increment table count and serialize
                    num_tables++;
                    serialize_int(metadata_page, offset, num_tables);

                    // Re-serialize existing tables (if any) and then the new one
                    if (current_pager->get_num_pages() > METADATA_PAGE_NUM) {
                        std::vector<char> existing_metadata = current_pager->read_page(METADATA_PAGE_NUM);
                        size_t read_offset = sizeof(int32_t); // Skip num_tables
                        for (int i = 0; i < num_tables - 1; ++i) {
                            auto existing_schema = deserialize_create_table_statement(existing_metadata, read_offset);
                            serialize_create_table_statement(metadata_page, offset, *existing_schema);
                        }
                    }

                    serialize_create_table_statement(metadata_page, offset, *statement->create_table_statement);
                    current_pager->write_page(METADATA_PAGE_NUM, metadata_page);
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
            } else if (statement->type == STATEMENT_SELECT) {
                std::cout << "Parsed SELECT statement for table: " << statement->select_statement->table_name << std::endl;
            } else if (statement->type == STATEMENT_UPDATE) {
                std::cout << "Parsed UPDATE statement for table: " << statement->update_statement->table_name << ", SET clause: ";
                for (const auto& set_pair : statement->update_statement->set_clauses) {
                    std::cout << set_pair.second << " ";
                }
                std::cout << std::endl;
            } else if (statement->type == STATEMENT_DELETE) {
                std::cout << "Parsed DELETE statement for table: " << statement->delete_statement->table_name << ", WHERE clause: " << statement->delete_statement->where_clause << std::endl;
            } else {
                std::cout << "Unrecognized command or SQL statement: " << input_line << std::endl;
            }
        }
    }

    return 0;
}