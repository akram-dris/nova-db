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
            } else {
                std::cout << "Parsed statement type: " << statement->type << std::endl;
            }
        }
    }

    return 0;
}
