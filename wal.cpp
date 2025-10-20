#include "wal.h"
#include "pager.h"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

WriteAheadLog::WriteAheadLog(const std::string& db_filename) {
    if (db_filename.length() >= 4 && db_filename.substr(db_filename.length() - 4) == ".wal") {
        log_filename_ = db_filename;
    } else {
        log_filename_ = db_filename + ".wal";
    }
    log_file_stream_.open(log_filename_, std::ios::out | std::ios::binary | std::ios::app);
    if (!log_file_stream_.is_open()) {
        throw std::runtime_error("Could not open or create WAL file: " + log_filename_);
    }
}

WriteAheadLog::~WriteAheadLog() {
    if (log_file_stream_.is_open()) {
        log_file_stream_.close();
    }
    // The log file is removed when the transaction is committed.
    // If the destructor is called before the transaction is committed,
    // it means the transaction was aborted, and the log file should be removed.
    fs::remove(log_filename_);
}

void WriteAheadLog::log_update(int page_num, const std::vector<char>& page_data) {
    // Write page number
    log_file_stream_.write(reinterpret_cast<const char*>(&page_num), sizeof(page_num));
    // Write page data
    log_file_stream_.write(page_data.data(), PAGE_SIZE);
    log_file_stream_.flush();
}

void WriteAheadLog::commit() {
    if (log_file_stream_.is_open()) {
        log_file_stream_.close();
    }
    fs::remove(log_filename_);
}

void WriteAheadLog::recover() {
    // Not implemented yet
}
