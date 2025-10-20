#ifndef NOVADB_PAGER_H
#define NOVADB_PAGER_H

#include <string>
#include <vector>
#include <fstream>
#include <memory>

class WriteAheadLog; // Forward declaration

const int PAGE_SIZE = 4096;
const int METADATA_PAGE_NUM = 0;
const int INDEX_ROOT_PAGE_NUM = 1;

class Pager {
public:
    Pager(const std::string& filename);
    ~Pager();

    std::vector<char> read_page(int page_num);
    void write_page(int page_num, const std::vector<char>& data);
    int get_num_pages() const;

private:
    std::string filename_;
    std::fstream file_stream_;
    int num_pages_;
    std::unique_ptr<WriteAheadLog> wal_;
};

#endif //NOVADB_PAGER_H
