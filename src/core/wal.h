#ifndef NOVADB_WAL_H
#define NOVADB_WAL_H

#include <string>
#include <vector>
#include <memory>
#include <fstream> // Include fstream

class Pager;

enum LogRecordType {
    UPDATE,
    BEGIN_TXN,
    COMMIT_TXN,
    ROLLBACK_TXN
};

struct LogRecord {
    LogRecordType type;
    uint32_t transaction_id;
    int page_num;
    std::vector<char> new_page_data;
    std::vector<char> old_page_data; // Store old page data for rollback
};

class WriteAheadLog {
public:
    WriteAheadLog(const std::string& db_filename);
    ~WriteAheadLog();

    void log_update(int page_num, const std::vector<char>& new_page_data, const std::vector<char>& old_page_data);
    void commit();

    uint32_t start_transaction();
    void log_transaction_event(uint32_t transaction_id, LogRecordType type);
    uint32_t get_current_transaction_id() const;
    void clear_log_records();
    void flush_log_to_disk();

private:
    std::string log_filename_;
    std::fstream log_file_stream_;
    uint32_t current_transaction_id_;
};

#endif //NOVADB_WAL_H
