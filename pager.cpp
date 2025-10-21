#include "pager.h"
#include "wal.h" // Include wal.h
#include <stdexcept>
#include <filesystem>
#include <iostream> // Added for std::cerr and std::cout
#include <map> // For std::map
#include <cstring> // For std::memcpy

namespace fs = std::filesystem;

// Define a local struct for reading WAL records, as wal.h's LogRecord is now minimal
struct WalReadRecord {
    LogRecordType type;
    uint32_t transaction_id;
    int page_num;
    std::vector<char> new_page_data;
    std::vector<char> old_page_data;

    WalReadRecord() : page_num(0), new_page_data(PAGE_SIZE), old_page_data(PAGE_SIZE) {}
};

// Helper functions for serialization/deserialization
namespace {
    void serialize_string(std::vector<char>& buffer, const std::string& s) {
        size_t len = s.length();
        buffer.insert(buffer.end(), reinterpret_cast<const char*>(&len), reinterpret_cast<const char*>(&len) + sizeof(len));
        buffer.insert(buffer.end(), s.begin(), s.end());
    }

    std::string deserialize_string(const std::vector<char>& buffer, size_t& offset) {
        size_t len;
        std::memcpy(&len, buffer.data() + offset, sizeof(len));
        offset += sizeof(len);
        std::string s(buffer.data() + offset, len);
        offset += len;
        return s;
    }

    void serialize_int(std::vector<char>& buffer, int val) {
        buffer.insert(buffer.end(), reinterpret_cast<const char*>(&val), reinterpret_cast<const char*>(&val) + sizeof(val));
    }

    int deserialize_int(const std::vector<char>& buffer, size_t& offset) {
        int val;
        std::memcpy(&val, buffer.data() + offset, sizeof(val));
        offset += sizeof(val);
        return val;
    }

    void serialize_column(std::vector<char>& buffer, const Column& col) {
        serialize_string(buffer, col.name);
        serialize_string(buffer, col.type);
    }

    Column deserialize_column(const std::vector<char>& buffer, size_t& offset) {
        Column col;
        col.name = deserialize_string(buffer, offset);
        col.type = deserialize_string(buffer, offset);
        return col;
    }

    void serialize_table_schema(std::vector<char>& buffer, const TableSchema& schema) {
        serialize_string(buffer, schema.table_name);
        serialize_int(buffer, schema.root_page_num);
        serialize_int(buffer, schema.columns.size());
        for (const auto& col : schema.columns) {
            serialize_column(buffer, col);
        }
    }

    TableSchema deserialize_table_schema(const std::vector<char>& buffer, size_t& offset) {
        TableSchema schema;
        schema.table_name = deserialize_string(buffer, offset);
        schema.root_page_num = deserialize_int(buffer, offset);
        int num_columns = deserialize_int(buffer, offset);
        for (int i = 0; i < num_columns; ++i) {
            schema.columns.push_back(deserialize_column(buffer, offset));
        }
        return schema;
    }
} // anonymous namespace

Pager::Pager(const std::string& filename) : filename_(filename), num_pages_(0) {
    // Open the file. If it doesn't exist, create it. If it exists, open it.
    // Do NOT truncate here, as we want to preserve existing data.
    file_stream_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_stream_.is_open()) {
        // If file didn't exist, create it.
        file_stream_.open(filename_, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file_stream_.is_open()) {
            throw std::runtime_error("Could not open or create database file: " + filename_);
        }
    }

    // Determine the number of pages
    file_stream_.seekg(0, std::ios::end);
    long file_size = file_stream_.tellg();
    num_pages_ = file_size / PAGE_SIZE;
    if (file_size % PAGE_SIZE != 0) {
        // This should ideally not happen with fixed-size pages, but handle it
        num_pages_++;
    }

    wal_ = std::make_unique<WriteAheadLog>(filename_); // Initialize WriteAheadLog
    load_schemas(); // Load schemas after opening the file
}

Pager::~Pager() {
    if (file_stream_.is_open()) {
        file_stream_.close();
    }
}

std::vector<char> Pager::read_page(int page_num) {
    if (page_num < 0 || page_num >= num_pages_) {
        throw std::out_of_range("Page number out of range");
    }

    std::vector<char> page_data(PAGE_SIZE);
    file_stream_.seekg(page_num * PAGE_SIZE);
    file_stream_.read(page_data.data(), PAGE_SIZE);
    return page_data;
}

void Pager::write_page(int page_num, const std::vector<char>& data, bool log_update) {
    if (page_num < 0) {
        throw std::out_of_range("Page number out of range");
    }
    if (data.size() != PAGE_SIZE) {
        throw std::invalid_argument("Data size must match PAGE_SIZE");
    }

    std::vector<char> old_page_data(PAGE_SIZE);
    if (log_update) {
        // Read old page data before writing new data
        if (page_num < num_pages_) { // Only read if page already exists
            file_stream_.seekg(page_num * PAGE_SIZE);
            file_stream_.read(old_page_data.data(), PAGE_SIZE);
        } else {
            // If it's a new page, old_page_data is all zeros (default constructed)
            // This is fine, as it represents an empty page before the write.
        }
        wal_->log_update(page_num, data, old_page_data); // Log the update with old and new data
    }

    if (page_num >= num_pages_) {
        // If writing to a new page, extend the file
        file_stream_.seekp(page_num * PAGE_SIZE);
        num_pages_ = page_num + 1;
    } else {
        file_stream_.seekp(page_num * PAGE_SIZE);
    }
    file_stream_.write(data.data(), PAGE_SIZE);
    file_stream_.flush(); // Ensure data is written to disk
}

int Pager::get_num_pages() const {
    return num_pages_;
}

void Pager::begin_transaction() {
    if (!wal_) {
        wal_ = std::make_unique<WriteAheadLog>(filename_ + ".wal");
    }
    wal_->start_transaction();
    wal_->log_transaction_event(wal_->get_current_transaction_id(), LogRecordType::BEGIN_TXN);
}

void Pager::commit_transaction() {
    wal_->log_transaction_event(wal_->get_current_transaction_id(), LogRecordType::COMMIT_TXN);
    wal_->commit();
}

void Pager::rollback_transaction() {
    std::string wal_filename = filename_ + ".wal";
    if (!fs::exists(wal_filename)) {
        std::cerr << "Error: No WAL file found for rollback." << std::endl;
        return;
    }

    std::fstream wal_file_stream;
    wal_file_stream.open(wal_filename, std::ios::in | std::ios::binary);
    if (!wal_file_stream.is_open()) {
        std::cerr << "Error: Could not open WAL file for rollback: " << wal_filename << std::endl;
        return;
    }

    std::vector<WalReadRecord> log_records;
    WalReadRecord record;

    while (wal_file_stream.read(reinterpret_cast<char*>(&record.type), sizeof(record.type))) {
        wal_file_stream.read(reinterpret_cast<char*>(&record.transaction_id), sizeof(record.transaction_id));
        if (record.type == UPDATE) {
            wal_file_stream.read(reinterpret_cast<char*>(&record.page_num), sizeof(record.page_num));
            wal_file_stream.read(record.new_page_data.data(), PAGE_SIZE);
            wal_file_stream.read(record.old_page_data.data(), PAGE_SIZE);
        }
        log_records.push_back(record);
    }
    wal_file_stream.close();

    bool found_begin_txn = false;
    uint32_t current_txn_id = wal_->get_current_transaction_id(); // Get current transaction ID

    for (auto it = log_records.rbegin(); it != log_records.rend(); ++it) {
        if (it->transaction_id == current_txn_id) { // Only rollback for current transaction
            if (it->type == BEGIN_TXN) {
                found_begin_txn = true;
                break;
            } else if (it->type == UPDATE) {
                this->write_page(it->page_num, it->old_page_data, false);
            }
        }
    }

    if (!found_begin_txn) {
        std::cerr << "Warning: No BEGIN TRANSACTION found in WAL for rollback for transaction ID: " << current_txn_id << std::endl;
    }

    wal_->log_transaction_event(current_txn_id, LogRecordType::ROLLBACK_TXN);
    wal_->clear_log_records(); // This will clear the WAL file
    std::cout << "Transaction rolled back. WAL file removed." << std::endl;
}

void Pager::recover() {
    std::string wal_filename = filename_ + ".wal";
    if (!fs::exists(wal_filename)) {
        return;
    }

    std::fstream wal_file_stream;
    wal_file_stream.open(wal_filename, std::ios::in | std::ios::binary);
    if (!wal_file_stream.is_open()) {
        std::cerr << "Warning: Could not open WAL file for recovery: " << wal_filename << std::endl;
        return;
    }

    LogRecordType type;
    uint32_t transaction_id;
    int page_num;
    std::vector<char> new_page_data(PAGE_SIZE);
    std::vector<char> old_page_data(PAGE_SIZE);

    std::vector<std::tuple<int, std::vector<char>>> transaction_updates;
    bool transaction_active_in_wal = false;

    while (wal_file_stream.read(reinterpret_cast<char*>(&type), sizeof(type))) {
        wal_file_stream.read(reinterpret_cast<char*>(&transaction_id), sizeof(transaction_id));
        if (type == UPDATE) {
            if (!wal_file_stream.read(reinterpret_cast<char*>(&page_num), sizeof(page_num))) {
                std::cerr << "Error during WAL recovery: Could not read page_num." << std::endl;
                break;
            }
            if (!wal_file_stream.read(new_page_data.data(), PAGE_SIZE)) {
                std::cerr << "Error during WAL recovery: Could not read new_page_data for page " << page_num << std::endl;
                break;
            }
            if (!wal_file_stream.read(old_page_data.data(), PAGE_SIZE)) {
                std::cerr << "Error during WAL recovery: Could not read old_page_data for page " << page_num << std::endl;
                break;
            }

            if (transaction_active_in_wal) {
                transaction_updates.emplace_back(page_num, old_page_data);
            }

            this->write_page(page_num, new_page_data, false);
        } else if (type == BEGIN_TXN) {
            transaction_active_in_wal = true;
            transaction_updates.clear();
        } else if (type == COMMIT_TXN) {
            transaction_active_in_wal = false;
            transaction_updates.clear();
        } else if (type == ROLLBACK_TXN) {
            for (const auto& update : transaction_updates) {
                int rollback_page_num = std::get<0>(update);
                const std::vector<char>& rollback_page_data = std::get<1>(update);
                this->write_page(rollback_page_num, rollback_page_data, false);
            }
            transaction_active_in_wal = false;
            transaction_updates.clear();
        }
    }

    if (transaction_active_in_wal) {
        std::cerr << "Warning: Incomplete transaction found during WAL recovery. Performing rollback." << std::endl;
        for (const auto& update : transaction_updates) {
            int rollback_page_num = std::get<0>(update);
            const std::vector<char>& rollback_page_data = std::get<1>(update);
            this->write_page(rollback_page_num, rollback_page_data, false);
        }
    }

    wal_file_stream.close();
    // fs::remove(wal_filename); // REMOVED THIS LINE
    std::cout << "WAL recovery complete. Log file removed." << std::endl;
}

void Pager::load_schemas() {
    if (num_pages_ <= METADATA_PAGE_NUM) {
        // No metadata page yet, so no schemas to load
        return;
    }

    std::vector<char> metadata_page = read_page(METADATA_PAGE_NUM);
    size_t offset = 0;

    // Read number of schemas
    int num_schemas = deserialize_int(metadata_page, offset);
    for (int i = 0; i < num_schemas; ++i) {
        TableSchema schema = deserialize_table_schema(metadata_page, offset);
        schemas_[schema.table_name] = schema;
    }
}

void Pager::save_schemas() {
    std::vector<char> buffer;
    serialize_int(buffer, schemas_.size()); // Number of schemas

    for (const auto& pair : schemas_) {
        serialize_table_schema(buffer, pair.second);
    }

    // Pad with zeros to fill the page
    buffer.resize(PAGE_SIZE, 0);
    write_page(METADATA_PAGE_NUM, buffer, true); // Log this update
}

const TableSchema& Pager::get_table_schema(const std::string& table_name) const {
    auto it = schemas_.find(table_name);
    if (it == schemas_.end()) {
        throw std::runtime_error("Error: Table '" + table_name + "' not found.");
    }
    return it->second;
}

const std::map<std::string, TableSchema>& Pager::get_all_schemas() const {
    return schemas_;
}

void Pager::add_schema(const TableSchema& schema) {
    schemas_[schema.table_name] = schema;
}