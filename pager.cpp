#include "pager.h"
#include "wal.h" // Include wal.h
#include <stdexcept>
#include <filesystem>

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

void Pager::write_page(int page_num, const std::vector<char>& data) {
    if (page_num < 0) {
        throw std::out_of_range("Page number out of range");
    }
    if (data.size() != PAGE_SIZE) {
        throw std::invalid_argument("Data size must match PAGE_SIZE");
    }

    wal_->log_update(page_num, data); // Log the update

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
