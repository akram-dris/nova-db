#include <sstream>
#include <unistd.h> // For isatty()
#include <iostream>
#include <string>
#include <fstream>
#include <memory>
#include <map>
#include <vector> // Added for std::vector
#include <future> // Added for std::future
#include <algorithm> // Added for std::min
#include "parser.h"
#include "pager.h"
#include "serializer.h"
#include "index.h" // Include index.h
#include "thread_pool.h" // Include thread_pool.h

std::shared_ptr<Pager> current_pager = nullptr;
std::unique_ptr<Index> current_index = nullptr; // Add Index instance
ThreadPool thread_pool(std::thread::hardware_concurrency()); // Global ThreadPool instance
std::string current_db_name = "nova"; // Global variable for current database name

void print_prompt() {
    std::cout << current_db_name << "> ";
}

// Helper function to display a row
void display_row(const std::vector<std::string>& row_values, const std::vector<Column>& columns) {
    for (size_t i = 0; i < row_values.size(); ++i) {
        std::cout << row_values[i];
        if (i < row_values.size() - 1) {
            std::cout << " | ";
        }
    }
    std::cout << std::endl;
}

int main() {
    std::string input_line;
    bool transaction_active = false;

    bool is_interactive = isatty(STDIN_FILENO);

    std::stringstream piped_input_buffer;
    if (!is_interactive) {
        piped_input_buffer << std::cin.rdbuf();
    }

    if (is_interactive) {
        // Clear screen
        std::cout << "\033[2J\033[H";
        // Display NovaDB banner
        std::cout << " _______           __         " << std::endl;
        std::cout << "|   |   |.---.-.--|  |.-----. " << std::endl;
        std::cout << "|       ||  _  |  _  ||  _  | " << std::endl;
        std::cout << "|__|_|__||___._|_____||   __| " << std::endl;
        std::cout << "                     |__|    " << std::endl;
        std::cout << std::endl;
        std::cout << "Welcome to NovaDB CLI!" << std::endl;
        std::cout << "Start by opening a database: .open <filename.db>" << std::endl;
        std::cout << "Type .help for a list of commands." << std::endl;
        std::cout << std::endl;
    }

    while (true) {
        if (is_interactive) {
            if (!std::getline(std::cin, input_line)) {
                break; // EOF or error
            }
        } else {
            if (!std::getline(piped_input_buffer, input_line)) {
                break; // EOF or error from piped input
            }
        }

        // Trim whitespace from the input line
        input_line.erase(0, input_line.find_first_not_of(" \t\n\r\f\v"));
        input_line.erase(input_line.find_last_not_of(" \t\n\r\f\v") + 1);

        // Ignore empty lines and comments

        if (input_line.empty() || input_line.rfind("--", 0) == 0) {
            continue;
        }

        if (input_line.rfind(".open", 0) == 0) {
            std::string filename = input_line.substr(6);
            try {
                current_pager = std::make_shared<Pager>(filename);
                current_pager->recover(); // Perform WAL recovery
                // For now, assume index root is always page 0 (or some fixed page)
                // If it's a new DB, root_page_num will be -1, and Index constructor will create it.
                current_index = std::make_unique<Index>(current_pager, INDEX_ROOT_PAGE_NUM); // Use INDEX_ROOT_PAGE_NUM

                current_db_name = filename; // Store full filename initially

                // Extract base name without extension for the prompt
                size_t last_dot_pos = current_db_name.rfind('.');
                if (last_dot_pos != std::string::npos) {
                    current_db_name = current_db_name.substr(0, last_dot_pos);
                }

                // Clear screen and display success message
                std::cout << "\033[2J\033[H";
                std::cout << "Database '" << filename << "' opened successfully." << std::endl;
                std::cout << "For more information or help, type .help" << std::endl;

            } catch (const std::exception& e) {
                std::cerr << "Error opening database: " << e.what() << std::endl;
            }
        } else if (input_line == ".help") {
            std::cout << "NovaDB CLI Commands:" << std::endl;
            std::cout << "  .open <filename.db> - Open or create a database file." << std::endl;
            std::cout << "  .tables             - List all tables in the current database." << std::endl;
            std::cout << "  .schema [table_name] - Display the schema of a specific table or all tables." << std::endl;
            std::cout << "  .help               - Display this help message." << std::endl;
            std::cout << "  .exit               - Exit the NovaDB CLI." << std::endl;
            std::cout << "SQL Commands:" << std::endl;
            std::cout << "  CREATE TABLE <table_name> (<col1> <type1>, ...);" << std::endl;
            std::cout << "  INSERT INTO <table_name> VALUES (...);" << std::endl;
            std::cout << "  SELECT * FROM <table_name> [WHERE <condition>];" << std::endl;
            std::cout << "  UPDATE <table_name> SET <col1> = <val1>, ... [WHERE <condition>];" << std::endl;
            std::cout << "  DELETE FROM <table_name> [WHERE <condition>];" << std::endl;
            std::cout << "  BEGIN TRANSACTION;" << std::endl;
            std::cout << "  COMMIT TRANSACTION;" << std::endl;
            std::cout << "  ROLLBACK TRANSACTION;" << std::endl;
        } else if (input_line == ".exit") {
            std::cout << "Goodbye from NovaDB." << std::endl;
            break;
        } else if (input_line == ".tables") {
            if (!current_pager) {
                std::cout << "Error: No database open. Use .open <filename.db>" << std::endl;
                continue;
            }
            std::cout << "Tables:" << std::endl;
            for (const auto& pair : current_pager->get_all_schemas()) {
                std::cout << "- " << pair.first << std::endl;
            }
        } else if (input_line.rfind(".schema", 0) == 0) {
            if (!current_pager) {
                std::cout << "Error: No database open. Use .open <filename.db>" << std::endl;
                continue;
            }
            std::string table_name_to_find = input_line.substr(7);
            table_name_to_find.erase(0, table_name_to_find.find_first_not_of(" \t\n\r\f\v"));

            try {
                if (table_name_to_find.empty()) { // Display all schemas
                    for (const auto& pair : current_pager->get_all_schemas()) {
                        std::cout << "Table: " << pair.first << std::endl;
                        std::cout << "  Columns:" << std::endl;
                        for (const auto& col : pair.second.columns) {
                            std::cout << "    - " << col.name << " (Type: " << col.type << ")" << std::endl;
                        }
                    }
                } else { // Display specific schema
                    const TableSchema& schema = current_pager->get_table_schema(table_name_to_find);
                    std::cout << "Table: " << schema.table_name << std::endl;
                    std::cout << "  Columns:" << std::endl;
                    for (const auto& col : schema.columns) {
                        std::cout << "    - " << col.name << " (Type: " << col.type << ")" << std::endl;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error reading schema: " << e.what() << std::endl;
            }
        } else {
            // Attempt to parse SQL statement
            auto statement = parse_statement(input_line);
            if (statement->type == STATEMENT_UNKNOWN && input_line.empty()) {
                continue; // Ignore empty lines that result in STATEMENT_UNKNOWN
            } else if (statement->type == STATEMENT_CREATE_TABLE) {
                if (!current_pager) {
                    std::cout << "Error: No database open. Use .open <filename.db>" << std::endl;
                    continue;
                }
                try {
                    TableSchema new_schema;
                    new_schema.table_name = statement->create_table_statement->table_name;
                    new_schema.root_page_num = -1; // Placeholder, actual root page will be assigned later
                    for (const auto& col_def : statement->create_table_statement->columns) {
                        Column col;
                        col.name = col_def.name;
                        col.type = column_type_to_string(col_def.type); // Convert enum to string
                        new_schema.columns.push_back(col);
                    }

                    current_pager->add_schema(new_schema);
                    current_pager->save_schemas();
                    std::cout << "Table '" << new_schema.table_name << "' schema stored." << std::endl;

                } catch (const std::exception& e) {
                    std::cerr << "Error creating table: " << e.what() << std::endl;
                }
            } else if (statement->type == STATEMENT_INSERT) {
                if (!statement->insert_statement) {
                    std::cerr << "ERROR: INSERT statement is null." << std::endl;
                    continue;
                }
                std::cout << "Parsed INSERT statement for table: " << statement->insert_statement->table_name << ", values: ";
                std::cout << std::endl;


                if (!current_pager) {
                    std::cout << "Error: No database open. Use .open <filename.db>" << std::endl;
                    continue;
                }

                try {
                    // 1. Retrieve Table Schema
                    const TableSchema& table_schema = current_pager->get_table_schema(statement->insert_statement->table_name);

                    // 2. Validate INSERT Statement
                    if (statement->insert_statement->values.size() != table_schema.columns.size()) {
                        std::cout << "Error: Number of values does not match number of columns in table '" << table_schema.table_name << "'." << std::endl;
                        continue;
                    }

                    // 3. Serialize Record
                    std::vector<char> record_page(PAGE_SIZE, 0); // One record per page for simplicity
                    size_t write_offset = 0;

                    // Store a placeholder for record size
                    size_t record_size_offset = write_offset;
                    serialize_int(record_page, write_offset, 0); // Placeholder for actual record size

                    for (size_t i = 0; i < table_schema.columns.size(); ++i) {
                        const auto& col_def = table_schema.columns[i];
                        const std::string& value_str = statement->insert_statement->values[i];

                        // Basic type validation and serialization
                        try {
                            serialize_value(record_page, write_offset, string_to_column_type(col_def.type), value_str);
                        } catch (const std::exception& e) {
                            std::cout << "Error: Invalid value for column '" << col_def.name << " ('" << col_def.type << "'): " << e.what() << std::endl;
                            continue; // Skip this record or handle error appropriately
                        }
                    }

                    // Now, write the actual record size
                    size_t current_record_size = write_offset - sizeof(int32_t); // Total bytes written minus the size of the size itself
                    size_t temp_offset = record_size_offset; // Reset offset to write the size
                    serialize_int(record_page, temp_offset, current_record_size);

                    // 4. Store Record
                    int page_to_write = current_pager->get_num_pages(); // Append to the end of the file
                    current_pager->write_page(page_to_write, record_page);
                    std::cout << "Record inserted into table '" << table_schema.table_name << "' on page " << page_to_write << "." << std::endl;

                    // Insert into index (assuming first column is the primary key)
                    if (current_index && !table_schema.columns.empty()) {
                        const std::string& primary_key_value = statement->insert_statement->values[0];
                        current_index->insert(primary_key_value, page_to_write);
                        std::cout << "Inserted into index: key='" << primary_key_value << "', page=" << page_to_write << std::endl;
                    }

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
                    const TableSchema& table_schema = current_pager->get_table_schema(statement->select_statement->table_name);

                    // Print column headers
                    for (size_t i = 0; i < table_schema.columns.size(); ++i) {
                        std::cout << table_schema.columns[i].name;
                        if (i < table_schema.columns.size() - 1) {
                            std::cout << " | ";
                        }
                    }
                    std::cout << std::endl;
                    for (size_t i = 0; i < table_schema.columns.size(); ++i) {
                        for (size_t j = 0; j < table_schema.columns[i].name.length(); ++j) {
                            std::cout << "-";
                        }
                        if (i < table_schema.columns.size() - 1) {
                            std::cout << "-|- ";
                        }
                    }
                    std::cout << std::endl;

                    // Check if index can be used
                    int indexed_record_page_num = -1;
                    bool use_index = false;
                    if (current_index && statement->select_statement->where_condition &&
                        statement->select_statement->where_condition->op == OP_EQ &&
                        !table_schema.columns.empty() &&
                        table_schema.columns[0].name == statement->select_statement->where_condition->column_name) {

                        indexed_record_page_num = current_index->search(statement->select_statement->where_condition->value);
                        if (indexed_record_page_num != -1) {
                            std::cout << "Using index for lookup. Found record on page: " << indexed_record_page_num << std::endl;
                            use_index = true;
                        }
                    }

                    if (use_index) { // Use index lookup
                        std::vector<char> record_page = current_pager->read_page(indexed_record_page_num);

                        size_t record_read_offset = 0;
                        int32_t record_size = deserialize_int(record_page, record_read_offset); // Read record size

                        bool is_zero_page = true;
                        // Check if the record_size is 0, which implies a deleted or empty record
                        if (record_size > 0) {
                            is_zero_page = false;
                        }

                        if (!is_zero_page) { // Only display if not a zeroed page
                            std::vector<std::string> row_values;
                            for (size_t i = 0; i < table_schema.columns.size(); ++i) {
                                const auto& col_def = table_schema.columns[i];
                                row_values.push_back(deserialize_value(record_page, record_read_offset, string_to_column_type(col_def.type)));
                            }
                            display_row(row_values, table_schema.columns);
                        }
                    } else { // Full table scan - Parallelize this part
                        std::vector<std::future<std::vector<std::vector<std::string>>>> futures;
                        int num_data_pages = current_pager->get_num_pages() - (INDEX_ROOT_PAGE_NUM + 1);
                        int num_threads = thread_pool.get_num_threads(); // Assuming get_num_threads() exists
                        int pages_per_thread = std::max(1, num_data_pages / num_threads);

                        for (int i = 0; i < num_threads; ++i) {
                            int start_page = INDEX_ROOT_PAGE_NUM + 1 + i * pages_per_thread;
                            int end_page = std::min(current_pager->get_num_pages(), start_page + pages_per_thread);

                            if (start_page >= end_page) break;

                            futures.push_back(thread_pool.enqueue([=, &table_schema, &statement]() {
                                std::vector<std::vector<std::string>> result_rows;
                                for (int page_num = start_page; page_num < end_page; ++page_num) {
                                    std::vector<char> record_page = current_pager->read_page(page_num);

                                    size_t record_read_offset = 0;
                                    int32_t record_size = deserialize_int(record_page, record_read_offset);

                                    if (record_size == 0) {
                                        continue;
                                    }

                                    std::vector<std::string> row_values;
                                    try {
                                        for (size_t col_idx = 0; col_idx < table_schema.columns.size(); ++col_idx) {
                                            const auto& col_def = table_schema.columns[col_idx];
                                            row_values.push_back(deserialize_value(record_page, record_read_offset, string_to_column_type(col_def.type)));
                                        }
                                    } catch (const std::out_of_range& e) {
                                        std::cerr << "Skipping page " << page_num << " due to deserialization error: " << e.what() << std::endl;
                                        continue;
                                    }

                                    bool condition_met = true;
                                    if (statement->select_statement->where_condition) {
                                        condition_met = false;

                                        const auto& wc = statement->select_statement->where_condition;
                                        int column_index = -1;
                                        for (size_t col_idx = 0; col_idx < table_schema.columns.size(); ++col_idx) {
                                            if (table_schema.columns[col_idx].name == wc->column_name) {
                                                column_index = col_idx;
                                                break;
                                            }
                                        }

                                        if (column_index != -1) {
                                            const auto& col_def = table_schema.columns[column_index];
                                            const std::string& record_value = row_values[column_index];

                                            if (wc->op == OP_EQ) {
                                                if (string_to_column_type(col_def.type) == COLUMN_TYPE_INT) {
                                                    try {
                                                        if (std::stoi(record_value) == std::stoi(wc->value)) {
                                                            condition_met = true;
                                                        }
                                                    } catch (const std::exception& e) {
                                                        std::cerr << "Warning: Type conversion error in WHERE clause for INT comparison: " << e.what() << std::endl;
                                                    }
                                                } else if (string_to_column_type(col_def.type) == COLUMN_TYPE_TEXT) {
                                                    std::string cleaned_wc_value = wc->value;
                                                    if (cleaned_wc_value.length() >= 2 && cleaned_wc_value.front() == '\'' && cleaned_wc_value.back() == '\'') {
                                                        cleaned_wc_value = cleaned_wc_value.substr(1, cleaned_wc_value.length() - 2);
                                                    }
                                                    if (record_value == cleaned_wc_value) {
                                                        condition_met = true;
                                                    }
                                                }
                                            } else if (wc->op == OP_GT) {
                                                if (string_to_column_type(col_def.type) == COLUMN_TYPE_INT) {
                                                    try {
                                                        if (std::stoi(record_value) > std::stoi(wc->value)) {
                                                            condition_met = true;
                                                        }
                                                    } catch (const std::exception& e) {
                                                        std::cerr << "Warning: Type conversion error in WHERE clause for INT comparison: " << e.what() << std::endl;
                                                    }
                                                } else if (string_to_column_type(col_def.type) == COLUMN_TYPE_TEXT) {
                                                    std::string cleaned_wc_value = wc->value;
                                                    if (cleaned_wc_value.length() >= 2 && cleaned_wc_value.front() == '\'' && cleaned_wc_value.back() == '\'') {
                                                        cleaned_wc_value = cleaned_wc_value.substr(1, cleaned_wc_value.length() - 2);
                                                    }
                                                    if (record_value > cleaned_wc_value) {
                                                        condition_met = true;
                                                    }
                                                }
                                            } else if (wc->op == OP_LT) {
                                                if (string_to_column_type(col_def.type) == COLUMN_TYPE_INT) {
                                                    try {
                                                        if (std::stoi(record_value) < std::stoi(wc->value)) {
                                                            condition_met = true;
                                                        }
                                                    } catch (const std::exception& e) {
                                                        std::cerr << "Warning: Type conversion error in WHERE clause for INT comparison: " << e.what() << std::endl;
                                                    }
                                                } else if (string_to_column_type(col_def.type) == COLUMN_TYPE_TEXT) {
                                                    std::string cleaned_wc_value = wc->value;
                                                    if (cleaned_wc_value.length() >= 2 && cleaned_wc_value.front() == '\'' && cleaned_wc_value.back() == '\'') {
                                                        cleaned_wc_value = cleaned_wc_value.substr(1, cleaned_wc_value.length() - 2);
                                                    }
                                                    if (record_value < cleaned_wc_value) {
                                                        condition_met = true;
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    if (condition_met) {
                                        result_rows.push_back(row_values);
                                    }
                                }
                                return result_rows;
                            }));
                        }

                        // Aggregate results from all futures
                        std::vector<std::vector<std::string>> all_result_rows;
                        for (auto& future : futures) {
                            std::vector<std::vector<std::string>> thread_rows = future.get();
                            all_result_rows.insert(all_result_rows.end(), thread_rows.begin(), thread_rows.end());
                        }

                        // Display aggregated results
                        for (const auto& row_values : all_result_rows) {
                            display_row(row_values, table_schema.columns);
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
                    const TableSchema& table_schema = current_pager->get_table_schema(statement->update_statement->table_name);

                    int updated_rows = 0;
                    bool use_index_for_update = false;
                    int indexed_record_page_num = -1;
                    // 2. Table Scan and Record Update
                    if (current_index && statement->update_statement->where_condition &&
                        statement->update_statement->where_condition->op == OP_EQ &&
                        !table_schema.columns.empty() &&
                        table_schema.columns[0].name == statement->update_statement->where_condition->column_name) {

                        indexed_record_page_num = current_index->search(statement->update_statement->where_condition->value);
                        if (indexed_record_page_num != -1) {
                            std::cout << "Using index for UPDATE. Found record on page: " << indexed_record_page_num << std::endl;
                            use_index_for_update = true;
                        }
                    }

                    if (use_index_for_update) {
                        std::vector<char> record_page = current_pager->read_page(indexed_record_page_num);
                        size_t record_read_offset = 0;
                        int32_t record_size = deserialize_int(record_page, record_read_offset); // Read record size

                        std::vector<std::string> row_values;
                        for (size_t i = 0; i < table_schema.columns.size(); ++i) {
                            const auto& col_def = table_schema.columns[i];
                            row_values.push_back(deserialize_value(record_page, record_read_offset, string_to_column_type(col_def.type)));
                        }

                        // Apply SET clauses
                        for (const auto& set_pair : statement->update_statement->set_clauses) {
                            int column_index_to_update = -1;
                            for (size_t i = 0; i < table_schema.columns.size(); ++i) {
                                if (table_schema.columns[i].name == set_pair.first) {
                                    column_index_to_update = i;
                                    break;
                                }
                            }

                            if (column_index_to_update != -1) {
                                row_values[column_index_to_update] = set_pair.second;
                            } else {
                                std::cerr << "Warning: Column '" << set_pair.first << "' not found in table '" << table_schema.table_name << "'." << std::endl;
                            }
                        }

                        // Serialize and Write Back
                        std::vector<char> updated_record_page(PAGE_SIZE, 0);
                        size_t updated_write_offset = 0;
                        // Store a placeholder for record size
                        size_t record_size_offset = updated_write_offset;
                        serialize_int(updated_record_page, updated_write_offset, 0); // Placeholder for actual record size

                        for (size_t i = 0; i < row_values.size(); ++i) {
                            const auto& col_def = table_schema.columns[i];
                            try {
                                serialize_value(updated_record_page, updated_write_offset, string_to_column_type(col_def.type), row_values[i]);
                            } catch (const std::exception& e) {
                                std::cerr << "Error: Failed to serialize updated value for column '" << col_def.name << "': " << e.what() << std::endl;
                            }
                        }
                        // Now, write the actual record size
                        size_t current_record_size = updated_write_offset - sizeof(int32_t); // Total bytes written minus the size of the size itself
                        size_t temp_offset = record_size_offset; // Reset offset to write the size
                        serialize_int(updated_record_page, temp_offset, current_record_size);

                        current_pager->write_page(indexed_record_page_num, updated_record_page);
                        updated_rows++;
                    } else { // Full table scan
                        for (int page_num = INDEX_ROOT_PAGE_NUM + 1; page_num < current_pager->get_num_pages(); ++page_num) {
                            std::vector<char> record_page = current_pager->read_page(page_num);
                            size_t record_read_offset = 0;
                            int32_t record_size = deserialize_int(record_page, record_read_offset); // Read record size

                            std::vector<std::string> row_values;
                            // Deserialize all values for the current record
                            for (size_t i = 0; i < table_schema.columns.size(); ++i) {
                                const auto& col_def = table_schema.columns[i];
                                row_values.push_back(deserialize_value(record_page, record_read_offset, string_to_column_type(col_def.type)));
                            }
                            bool condition_met = true; // Assume true if no WHERE clause
                            // Evaluate WHERE condition if present
                            if (statement->update_statement->where_condition) {
                                condition_met = false; // Reset to false, must be explicitly met

                                const auto& wc = statement->update_statement->where_condition;
                                int column_index = -1;
                                for (size_t i = 0; i < table_schema.columns.size(); ++i) {
                                    if (table_schema.columns[i].name == wc->column_name) {
                                        column_index = i;
                                        break;
                                    }
                                }

                                if (column_index != -1) {
                                    const auto& col_def = table_schema.columns[column_index];
                                    const std::string& record_value = row_values[column_index];

                                    if (wc->op == OP_EQ) {
                                        if (string_to_column_type(col_def.type) == COLUMN_TYPE_INT) {
                                            try {
                                                if (std::stoi(record_value) == std::stoi(wc->value)) {
                                                    condition_met = true;
                                                }
                                            } catch (const std::exception& e) {
                                                std::cerr << "Warning: Type conversion error in WHERE clause for INT comparison: " << e.what() << std::endl;
                                            }
                                        } else if (string_to_column_type(col_def.type) == COLUMN_TYPE_TEXT) {
                                            std::string cleaned_wc_value = wc->value;
                                            if (cleaned_wc_value.length() >= 2 && cleaned_wc_value.front() == '\'' && cleaned_wc_value.back() == '\'') {
                                                cleaned_wc_value = cleaned_wc_value.substr(1, cleaned_wc_value.length() - 2);
                                            }
                                            if (record_value == cleaned_wc_value) {
                                                condition_met = true;
                                            }
                                        }
                                    } else if (wc->op == OP_GT) {
                                        if (string_to_column_type(col_def.type) == COLUMN_TYPE_INT) {
                                            try {
                                                if (std::stoi(record_value) > std::stoi(wc->value)) {
                                                    condition_met = true;
                                                }
                                            } catch (const std::exception& e) {
                                                std::cerr << "Warning: Type conversion error in WHERE clause for INT comparison: " << e.what() << std::endl;
                                            }
                                        } else if (string_to_column_type(col_def.type) == COLUMN_TYPE_TEXT) {
                                            std::string cleaned_wc_value = wc->value;
                                            if (cleaned_wc_value.length() >= 2 && cleaned_wc_value.front() == '\'' && cleaned_wc_value.back() == '\'') {
                                                cleaned_wc_value = cleaned_wc_value.substr(1, cleaned_wc_value.length() - 2);
                                            }
                                            if (record_value > cleaned_wc_value) {
                                                condition_met = true;
                                            }
                                        }
                                    } else if (wc->op == OP_LT) {
                                        if (string_to_column_type(col_def.type) == COLUMN_TYPE_INT) {
                                            try {
                                                if (std::stoi(record_value) < std::stoi(wc->value)) {
                                                    condition_met = true;
                                                }
                                            } catch (const std::exception& e) {
                                                std::cerr << "Warning: Type conversion error in WHERE clause for INT comparison: " << e.what() << std::endl;
                                            }
                                        } else if (string_to_column_type(col_def.type) == COLUMN_TYPE_TEXT) {
                                            std::string cleaned_wc_value = wc->value;
                                            if (cleaned_wc_value.length() >= 2 && cleaned_wc_value.front() == '\'' && cleaned_wc_value.back() == '\'') {
                                                cleaned_wc_value = cleaned_wc_value.substr(1, cleaned_wc_value.length() - 2);
                                            }
                                            if (record_value < cleaned_wc_value) {
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
                                    for (size_t i = 0; i < table_schema.columns.size(); ++i) {
                                        if (table_schema.columns[i].name == set_pair.first) {
                                            column_index_to_update = i;
                                            break;
                                        }
                                    }
                                    if (column_index_to_update != -1) {
                                        row_values[column_index_to_update] = set_pair.second;
                                    } else {
                                        std::cerr << "Warning: Column '" << set_pair.first << "' not found in table '" << table_schema.table_name << "'." << std::endl;
                                    }
                                }

                                // Serialize and Write Back
                                std::vector<char> updated_record_page(PAGE_SIZE, 0);
                                size_t updated_write_offset = 0;
                                // Store a placeholder for record size
                                size_t record_size_offset = updated_write_offset;
                                serialize_int(updated_record_page, updated_write_offset, 0); // Placeholder for actual record size

                                for (size_t i = 0; i < row_values.size(); ++i) {
                                    const auto& col_def = table_schema.columns[i];
                                    try {
                                        serialize_value(updated_record_page, updated_write_offset, string_to_column_type(col_def.type), row_values[i]);
                                    } catch (const std::exception& e) {
                                        std::cerr << "Error: Failed to serialize updated value for column '" << col_def.name << "': " << e.what() << std::endl;
                                    }
                                }
                                // Now, write the actual record size
                                size_t current_record_size = updated_write_offset - sizeof(int32_t); // Total bytes written minus the size of the size itself
                                size_t temp_offset = record_size_offset; // Reset offset to write the size
                                serialize_int(updated_record_page, temp_offset, current_record_size);

                                current_pager->write_page(page_num, updated_record_page);
                                updated_rows++;
                            }
                        }
                    }
                    std::cout << "Updated " << updated_rows << " rows in table '" << table_schema.table_name << "'." << std::endl;

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
                    const TableSchema& table_schema = current_pager->get_table_schema(statement->delete_statement->table_name);

                    int deleted_rows = 0;
                    bool use_index_for_delete = false;
                    int indexed_record_page_num = -1;

                    if (current_index && statement->delete_statement->where_condition &&
                        statement->delete_statement->where_condition->op == OP_EQ &&
                        !table_schema.columns.empty() &&
                        table_schema.columns[0].name == statement->delete_statement->where_condition->column_name) {

                        indexed_record_page_num = current_index->search(statement->delete_statement->where_condition->value);
                        if (indexed_record_page_num != -1) {
                            std::cout << "Using index for DELETE. Found record on page: " << indexed_record_page_num << std::endl;
                            use_index_for_delete = true;
                        }
                    }

                    if (use_index_for_delete) {
                        // Mark for Deletion: Overwrite page with zeros
                        std::vector<char> empty_page(PAGE_SIZE, 0);
                        current_pager->write_page(indexed_record_page_num, empty_page);
                        deleted_rows++;

                        // Remove from index (assuming first column is the primary key)
                        if (current_index && !table_schema.columns.empty()) {
                            const std::string& primary_key_value = statement->delete_statement->where_condition->value;
                            current_index->remove(primary_key_value);
                            std::cout << "Removed from index: key='" << primary_key_value << "'" << std::endl;
                        }
                    } else { // Full table scan
                        for (int page_num = INDEX_ROOT_PAGE_NUM + 1; page_num < current_pager->get_num_pages(); ++page_num) {
                            std::vector<char> record_page = current_pager->read_page(page_num);

                            // Check if page is already zeroed out (considered deleted)
                            size_t record_read_offset = 0;
                            int32_t record_size = deserialize_int(record_page, record_read_offset); // Read record size

                            // Check if the record_size is 0, which implies a deleted or empty record
                            if (record_size == 0) {
                                continue; // Skip this page as it's empty or deleted
                            }

                            std::vector<std::string> row_values;
                            try {
                                // Deserialize all values for the current record
                                for (size_t i = 0; i < table_schema.columns.size(); ++i) {
                                    const auto& col_def = table_schema.columns[i];
                                    row_values.push_back(deserialize_value(record_page, record_read_offset, string_to_column_type(col_def.type)));
                                }
                            } catch (const std::out_of_range& e) {
                                // This page is likely corrupted or not a valid record page.
                                // Log the error and continue to the next page.
                                std::cerr << "Skipping page " << page_num << " due to deserialization error: " << e.what() << std::endl;
                                continue;
                            }

                            bool condition_met = true; // Assume true if no WHERE clause
                            // Evaluate WHERE condition if present
                            if (statement->delete_statement->where_condition) {
                                condition_met = false; // Reset to false, must be explicitly met

                                const auto& wc = statement->delete_statement->where_condition;
                                int column_index = -1;
                                for (size_t i = 0; i < table_schema.columns.size(); ++i) {
                                    if (table_schema.columns[i].name == wc->column_name) {
                                        column_index = i;
                                        break;
                                    }
                                }

                                if (column_index != -1) {
                                    const auto& col_def = table_schema.columns[column_index];
                                    const std::string& record_value = row_values[column_index];

                                    if (wc->op == OP_EQ) {
                                        if (string_to_column_type(col_def.type) == COLUMN_TYPE_INT) {
                                            try {
                                                if (std::stoi(record_value) == std::stoi(wc->value)) {
                                                    condition_met = true;
                                                }
                                            } catch (const std::exception& e) {
                                                std::cerr << "Warning: Type conversion error in WHERE clause for INT comparison: " << e.what() << std::endl;
                                            }
                                        } else if (string_to_column_type(col_def.type) == COLUMN_TYPE_TEXT) {
                                            std::string cleaned_wc_value = wc->value;
                                            if (cleaned_wc_value.length() >= 2 && cleaned_wc_value.front() == '\'' && cleaned_wc_value.back() == '\'') {
                                                cleaned_wc_value = cleaned_wc_value.substr(1, cleaned_wc_value.length() - 2);
                                            }
                                            if (record_value == cleaned_wc_value) {
                                                condition_met = true;
                                            }
                                        }
                                    } else if (wc->op == OP_GT) {
                                        if (string_to_column_type(col_def.type) == COLUMN_TYPE_INT) {
                                            try {
                                                if (std::stoi(record_value) > std::stoi(wc->value)) {
                                                    condition_met = true;
                                                }
                                            } catch (const std::exception& e) {
                                                std::cerr << "Warning: Type conversion error in WHERE clause for INT comparison: " << e.what() << std::endl;
                                            }
                                        } else if (string_to_column_type(col_def.type) == COLUMN_TYPE_TEXT) {
                                            std::string cleaned_wc_value = wc->value;
                                            if (cleaned_wc_value.length() >= 2 && cleaned_wc_value.front() == '\'' && cleaned_wc_value.back() == '\'') {
                                                cleaned_wc_value = cleaned_wc_value.substr(1, cleaned_wc_value.length() - 2);
                                            }
                                            if (record_value > cleaned_wc_value) {
                                                condition_met = true;
                                            }
                                        }
                                    } else if (wc->op == OP_LT) {
                                        if (string_to_column_type(col_def.type) == COLUMN_TYPE_INT) {
                                            try {
                                                if (std::stoi(record_value) < std::stoi(wc->value)) {
                                                    condition_met = true;
                                                }
                                            }
                                            catch (const std::exception& e) {
                                                std::cerr << "Warning: Type conversion error in WHERE clause for INT comparison: " << e.what() << std::endl;
                                            }
                                        } else if (string_to_column_type(col_def.type) == COLUMN_TYPE_TEXT) {
                                            std::string cleaned_wc_value = wc->value;
                                            if (cleaned_wc_value.length() >= 2 && cleaned_wc_value.front() == '\'' && cleaned_wc_value.back() == '\'') {
                                                cleaned_wc_value = cleaned_wc_value.substr(1, cleaned_wc_value.length() - 2);
                                            }
                                            if (record_value < cleaned_wc_value) {
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

                                // Remove from index (assuming first column is the primary key)
                                if (current_index && !table_schema.columns.empty()) {
                                    const std::string& primary_key_value = row_values[0];
                                    current_index->remove(primary_key_value);
                                    std::cout << "Removed from index: key='" << primary_key_value << "'" << std::endl;
                                }
                            }
                        }
                    }
                    std::cout << "Deleted " << deleted_rows << " rows from table '" << table_schema.table_name << "'." << std::endl;

                } catch (const std::exception& e) {
                    std::cerr << "Error executing DELETE statement: " << e.what() << std::endl;
                }
            } else if (statement->type == STATEMENT_BEGIN_TRANSACTION) {
                if (transaction_active) {
                    std::cout << "Error: A transaction is already active." << std::endl;
                } else {
                    transaction_active = true;
                    current_pager->begin_transaction(); // Call begin_transaction on pager
                    std::cout << "Transaction started." << std::endl;
                }
            } else if (statement->type == STATEMENT_COMMIT_TRANSACTION) {
                if (!transaction_active) {
                    std::cout << "Error: No active transaction to commit." << std::endl;
                }
                else {
                    current_pager->commit_transaction();
                    transaction_active = false;
                    std::cout << "Transaction committed." << std::endl;
                }
            } else if (statement->type == STATEMENT_ROLLBACK_TRANSACTION) {
                if (!transaction_active) {
                    std::cout << "Error: No active transaction to rollback." << std::endl;
                }
                else {
                    current_pager->rollback_transaction();
                    transaction_active = false;
                }
            } else {
                std::cout << "Unrecognized command or SQL statement: " << input_line << std::endl;
            }
        }
        if (isatty(STDIN_FILENO)) {
            print_prompt();
        }
    }

    return 0;
}
