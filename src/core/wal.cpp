#include "wal.h"
#include "pager.h"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

WriteAheadLog::WriteAheadLog(const std::string& db_filename) : current_transaction_id_(0) {
    if (db_filename.length() >= 4 && db_filename.substr(db_filename.length() - 4) == ".wal") {
        log_filename_ = db_filename;
    } else {
        log_filename_ = db_filename + ".wal";
    }
    log_file_stream_.open(log_filename_, std::ios::out | std::ios::binary | std::ios::trunc); // Open in truncate mode to ensure a fresh WAL file
    if (!log_file_stream_.is_open()) {
        throw std::runtime_error("Could not open or create WAL file: " + log_filename_);
    }
}

WriteAheadLog::~WriteAheadLog() {
    if (log_file_stream_.is_open()) {
        log_file_stream_.close();
    }
}

uint32_t WriteAheadLog::start_transaction() {
    current_transaction_id_++;
    return current_transaction_id_;
}

uint32_t WriteAheadLog::get_current_transaction_id() const {
    return current_transaction_id_;
}

void WriteAheadLog::log_update(int page_num, const std::vector<char>& new_page_data, const std::vector<char>& old_page_data) {

    LogRecordType type = UPDATE;

    log_file_stream_.write(reinterpret_cast<const char*>(&type), sizeof(type));

    log_file_stream_.write(reinterpret_cast<const char*>(&current_transaction_id_), sizeof(current_transaction_id_));

    log_file_stream_.write(reinterpret_cast<const char*>(&page_num), sizeof(page_num));

    log_file_stream_.write(new_page_data.data(), PAGE_SIZE);

    log_file_stream_.write(old_page_data.data(), PAGE_SIZE);

    log_file_stream_.flush();

}

void WriteAheadLog::log_transaction_event(uint32_t transaction_id, LogRecordType type) {
    log_file_stream_.write(reinterpret_cast<const char*>(&type), sizeof(type));
    log_file_stream_.write(reinterpret_cast<const char*>(&transaction_id), sizeof(transaction_id));
    log_file_stream_.flush();
}

void WriteAheadLog::commit() {
    // The WAL file should not be deleted here. It should be deleted after a successful recovery or explicit rollback.
    // In a real system, this would involve more complex logic like
    // flushing to disk, checkpointing, etc.
    // For now, we just ensure the log is flushed.
    log_file_stream_.flush(); // Ensure all pending writes are to disk
}

void WriteAheadLog::clear_log_records() {
    // This function is no longer needed as log_records_ is removed.
    // However, if it's called, it should probably truncate the WAL file.
    // For now, we'll just leave it empty or remove calls to it.
    // Since it's a public method, it's better to implement it to truncate the file.
    log_file_stream_.close();
    fs::remove(log_filename_);
    log_file_stream_.open(log_filename_, std::ios::out | std::ios::binary | std::ios::app);
    if (!log_file_stream_.is_open()) {
        throw std::runtime_error("Could not re-open WAL file after clearing: " + log_filename_);
    }
}
