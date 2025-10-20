#ifndef NOVADB_PAGER_H
#define NOVADB_PAGER_H

#include <string>
#include <fstream>
#include <vector>

// Define page size (e.g., 4KB)
const int PAGE_SIZE = 4096;

class Pager {
public:
    Pager(const std::string& filename);
    ~Pager();

    // Read a page from the file
    std::vector<char> read_page(int page_num);

    // Write a page to the file
    void write_page(int page_num, const std::vector<char>& data);

    // Get the number of pages in the file
    int get_num_pages() const;

private:
    std::fstream file_stream_;
    std::string filename_;
    int num_pages_;
};

#endif //NOVADB_PAGER_H
