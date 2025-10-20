#ifndef NOVADB_INDEX_H
#define NOVADB_INDEX_H

#include <vector>
#include <string>
#include <memory>

const int B_TREE_ORDER = 3; // Max keys = 2*ORDER - 1, Max children = 2*ORDER

// Forward declarations
class Pager; // Assuming Pager is available

// B-Tree Node structure
struct BTreeNode {
    bool is_leaf;
    std::vector<std::string> keys; // Keys for comparison
    std::vector<int> children;     // Page numbers of child nodes (for internal nodes)
    std::vector<int> record_pointers; // Page numbers of records (for leaf nodes)
    int parent; // Page number of parent node

    BTreeNode() : is_leaf(true), parent(-1) {}
};

// Index structure
class Index {
public:
    Index(std::shared_ptr<Pager> pager, int root_page_num);

    // Basic B-Tree operations
    void insert(const std::string& key, int record_page_num);
    int search(const std::string& key); // Returns record_page_num or -1 if not found
    void remove(const std::string& key);

private:
    std::shared_ptr<Pager> pager_;
    int root_page_num_; // Page number of the root node

    // Helper functions for B-Tree operations
    int find_leaf(const std::string& key);
    void split_child(int parent_page_num, int child_index, int child_page_num);
    void remove_from_leaf(int leaf_page_num, const std::string& key);
    // ... (will be implemented in index.cpp)
};

#endif //NOVADB_INDEX_H
