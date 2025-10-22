#ifndef NOVADB_INDEX_H
#define NOVADB_INDEX_H

#include <vector>
#include <string>
#include <memory>

const int B_TREE_ORDER = 3;

class Pager;

struct BTreeNode {
    bool is_leaf;
    std::vector<std::string> keys;
    std::vector<int> children;
    std::vector<int> record_pointers;
    int parent;

    BTreeNode() : is_leaf(true), parent(-1) {}
};

class Index {
public:
    Index(std::shared_ptr<Pager> pager, int root_page_num);

    void insert(const std::string& key, int record_page_num);
    int search(const std::string& key);
    void remove(const std::string& key);
    std::vector<int> get_all_record_pages();

private:
    std::shared_ptr<Pager> pager_;
    int root_page_num_;

    void split_child(int parent_page_num, int child_index);
};

#endif //NOVADB_INDEX_H
