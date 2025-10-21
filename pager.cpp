#include "pager.h"
#include "wal.h" // Include wal.h
#include <stdexcept>
#include <filesystem>
#include <iostream> // Added for std::cerr and std::cout

namespace fs = std::filesystem;

Pager::Pager(const std::string& filename) : filename_(filename), num_pages_(0) {
    // Open the file in binary mode for reading and writing
    file_stream_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);

    if (!file_stream_.is_open()) {
        // If file didn't exist, create it with trunc
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

    if (log_update) {
        wal_->log_update(page_num, data); // Log the update
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

void Pager::recover() {
    std::string wal_filename = filename_ + ".wal";
    if (!fs::exists(wal_filename)) {
        // No WAL file, no recovery needed
        return;
    }

    std::fstream wal_file_stream;
    wal_file_stream.open(wal_filename, std::ios::in | std::ios::binary);
    if (!wal_file_stream.is_open()) {
        std::cerr << "Warning: Could not open WAL file for recovery: " << wal_filename << std::endl;
        return;
    }

    LogRecordType type;
    int page_num;
    std::vector<char> page_data(PAGE_SIZE);

    while (wal_file_stream.read(reinterpret_cast<char*>(&type), sizeof(type))) {
        if (type == UPDATE) {
            if (!wal_file_stream.read(reinterpret_cast<char*>(&page_num), sizeof(page_num))) {
                std::cerr << "Error during WAL recovery: Could not read page_num." << std::endl;
                break;
            }
            if (!wal_file_stream.read(page_data.data(), PAGE_SIZE)) {
                std::cerr << "Error during WAL recovery: Could not read page_data for page " << page_num << std::endl;
                break;
            }
            // Apply the update to the main database file without logging it again
            this->write_page(page_num, page_data, false); // Pass false to prevent re-logging
        } else if (type == COMMIT) {
            // For now, COMMIT just means the transaction was successful.
            // No specific action needed here other than continuing to process.
        }
    }

    wal_file_stream.close();
    fs::remove(wal_filename); // Delete WAL file after successful recovery
    std::cout << "WAL recovery complete. Log file removed." << std::endl;
}
