#include <iostream>
#include <string>
#include <fstream>

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
            std::cout << "Unrecognized command: " << input_line << std::endl;
        }
    }

    return 0;
}
