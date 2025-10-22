#ifndef NOVADB_PAGER_H
#define NOVADB_PAGER_H

#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <map>
#include <mutex>

class WriteAheadLog; // Forward declaration

const int PAGE_SIZE = 4096;
const int METADATA_PAGE_NUM = 0;
const int INDEX_ROOT_PAGE_NUM = 1;

struct Column {
    std::string name;
    std::string type; // e.g., "INT", "TEXT"
};

struct TableSchema {
    std::string table_name;
    std::vector<Column> columns;
    int root_page_num; // Page number where the table's data starts
};

class Pager {
public:
    Pager(const std::string& filename);
    ~Pager();

    std::vector<char> read_page(int page_num);
    void write_page(int page_num, const std::vector<char>& data, bool log_update = true);
    int get_num_pages() const;
    void recover();
    void commit_transaction();
    void rollback_transaction();
    void begin_transaction();

    void load_schemas();
    void save_schemas();
    const TableSchema& get_table_schema(const std::string& table_name) const;
    const std::map<std::string, TableSchema>& get_all_schemas() const;
    void add_schema(const TableSchema& schema);

private:
    std::string filename_;
    std::fstream file_stream_;
    int num_pages_;
    std::unique_ptr<WriteAheadLog> wal_;
    std::map<std::string, TableSchema> schemas_;
    mutable std::mutex mutex_;
};

#endif //NOVADB_PAGER_H
