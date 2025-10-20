#ifndef NOVADB_WAL_H
#define NOVADB_WAL_H

#include <string>
#include <vector>
#include <memory>
#include <fstream> // Include fstream

class Pager;

enum LogRecordType {
    UPDATE,
    COMMIT
};

struct LogRecord {
    LogRecordType type;
    int page_num;
    std::vector<char> page_data;
};

class WriteAheadLog {
public:
    WriteAheadLog(const std::string& db_filename);
    ~WriteAheadLog();

    void log_update(int page_num, const std::vector<char>& page_data);
    void commit();
    void recover();

private:
    std::string log_filename_;
    std::fstream log_file_stream_;
};

#endif //NOVADB_WAL_H
