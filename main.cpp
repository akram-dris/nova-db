#include <iostream>
#include <string>

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

        if (input_line == ".exit") {
            std::cout << "Goodbye from NovaDB." << std::endl;
            break;
        }

        std::cout << "You entered: " << input_line << std::endl;
    }

    return 0;
}