#include <iostream>
#include <string>
#include <fstream>
#include "parser.h"

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
            std::cout << "Opening database '" << filename << "'." << std::endl;
            // In the future, this will open the database file.
        } else if (input_line == ".exit") {
            std::cout << "Goodbye from NovaDB." << std::endl;
            break;
        } else {
            // Attempt to parse SQL statement
            auto statement = parse_statement(input_line);
            if (statement->type == STATEMENT_UNKNOWN) {
                std::cout << "Unrecognized command or SQL statement: " << input_line << std::endl;
            } else if (statement->type == STATEMENT_CREATE_TABLE) {
                std::cout << "Parsed CREATE TABLE statement for table: " << statement->create_table_statement->table_name << std::endl;
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
            } else {
                std::cout << "Unrecognized command or SQL statement: " << input_line << std::endl;
            }
        }
    }

    return 0;
}
