#include "index.h"
#include "pager.h"
#include "serializer.h"
#include <iostream>

// Helper function to read a BTreeNode from the pager
std::unique_ptr<BTreeNode> read_btree_node(std::shared_ptr<Pager> pager, int page_num) {
    std::vector<char> page_data = pager->read_page(page_num);
    size_t offset = 0;
    return deserialize_btree_node(page_data, offset);
}

// Helper function to write a BTreeNode to the pager
void write_btree_node(std::shared_ptr<Pager> pager, int page_num, const BTreeNode& node) {
    std::vector<char> page_data(PAGE_SIZE, 0);
    size_t offset = 0;
    serialize_btree_node(page_data, offset, node);
    pager->write_page(page_num, page_data);
}

Index::Index(std::shared_ptr<Pager> pager, int root_page_num)
    : pager_(pager), root_page_num_(root_page_num) {
    if (pager_->get_num_pages() <= root_page_num_) {
        BTreeNode root_node;
        root_node.is_leaf = true;
        write_btree_node(pager_, root_page_num_, root_node);
    }
}

void Index::split_child(int parent_page_num, int child_index) {
    std::unique_ptr<BTreeNode> parent_node = read_btree_node(pager_, parent_page_num);
    int child_page_num = parent_node->children[child_index];
    std::unique_ptr<BTreeNode> child_node = read_btree_node(pager_, child_page_num);

    int new_sibling_page_num = pager_->get_num_pages();
    BTreeNode new_sibling_node;
    new_sibling_node.is_leaf = child_node->is_leaf;
    new_sibling_node.parent = parent_page_num;

    // For B_TREE_ORDER = 3, a full node has 5 keys (2*3-1)
    // Split point: middle key is at index 2
    int mid = B_TREE_ORDER - 1; // Index 2 for order 3
    std::string middle_key = child_node->keys[mid];

    if (child_node->is_leaf) {
        // For leaf nodes: Keep middle key in left child, copy all keys >= middle to right
        // Left child keeps keys[0..mid], right gets keys[mid..end]
        new_sibling_node.keys.assign(child_node->keys.begin() + mid, child_node->keys.end());
        child_node->keys.resize(mid);

        new_sibling_node.record_pointers.assign(child_node->record_pointers.begin() + mid, 
                                                 child_node->record_pointers.end());
        child_node->record_pointers.resize(mid);
    } else {
        // For internal nodes: Promote middle key, split around it
        // Left child gets keys[0..mid-1] and children[0..mid]
        // Right child gets keys[mid+1..end] and children[mid+1..end]
        new_sibling_node.keys.assign(child_node->keys.begin() + mid + 1, child_node->keys.end());
        child_node->keys.resize(mid);

        new_sibling_node.children.assign(child_node->children.begin() + mid + 1, 
                                          child_node->children.end());
        child_node->children.resize(mid + 1);

        // Update parent pointers for moved children
        for (int moved_child : new_sibling_node.children) {
            std::unique_ptr<BTreeNode> moved_child_node = read_btree_node(pager_, moved_child);
            moved_child_node->parent = new_sibling_page_num;
            write_btree_node(pager_, moved_child, *moved_child_node);
        }
    }

    // Insert middle key into parent
    parent_node->keys.insert(parent_node->keys.begin() + child_index, middle_key);
    parent_node->children.insert(parent_node->children.begin() + child_index + 1, new_sibling_page_num);

    write_btree_node(pager_, parent_page_num, *parent_node);
    write_btree_node(pager_, child_page_num, *child_node);
    write_btree_node(pager_, new_sibling_page_num, new_sibling_node);
}

void Index::insert(const std::string& key, int record_page_num) {
    int current_page_num = root_page_num_;
    std::unique_ptr<BTreeNode> current_node = read_btree_node(pager_, current_page_num);

    if (current_node->keys.size() == 2 * B_TREE_ORDER - 1) {
        int new_root_page_num = pager_->get_num_pages();
        BTreeNode new_root;
        new_root.is_leaf = false;
        new_root.children.push_back(root_page_num_);
        write_btree_node(pager_, new_root_page_num, new_root);

        current_node->parent = new_root_page_num;
        write_btree_node(pager_, root_page_num_, *current_node);

        split_child(new_root_page_num, 0);
        root_page_num_ = new_root_page_num;
        current_page_num = root_page_num_;
    }

    while (true) {
        current_node = read_btree_node(pager_, current_page_num);
        if (current_node->is_leaf) {
            size_t i = 0;
            while (i < current_node->keys.size() && key > current_node->keys[i]) {
                i++;
            }
            current_node->keys.insert(current_node->keys.begin() + i, key);
            current_node->record_pointers.insert(current_node->record_pointers.begin() + i, record_page_num);
            write_btree_node(pager_, current_page_num, *current_node);
            return;
        } else {
            size_t i = 0;
            while (i < current_node->keys.size() && key > current_node->keys[i]) {
                i++;
            }
            int child_page_num = current_node->children[i];
            std::unique_ptr<BTreeNode> child_node = read_btree_node(pager_, child_page_num);
            if (child_node->keys.size() == 2 * B_TREE_ORDER - 1) {
                split_child(current_page_num, i);
                current_node = read_btree_node(pager_, current_page_num);
                if (key > current_node->keys[i]) {
                    child_page_num = current_node->children[i + 1];
                }
            }
            current_page_num = child_page_num;
        }
    }
}

int Index::search(const std::string& key) {
    int current_page_num = root_page_num_;
    while (true) {
        std::unique_ptr<BTreeNode> node = read_btree_node(pager_, current_page_num);
        size_t i = 0;
        while (i < node->keys.size() && key > node->keys[i]) {
            i++;
        }

        if (i < node->keys.size() && key == node->keys[i]) {
            if (node->is_leaf) {
                return node->record_pointers[i];
            } else {
                current_page_num = node->children[i + 1];
            }
        } else {
            if (node->is_leaf) {
                return -1; // Not found
            } else {
                current_page_num = node->children[i];
            }
        }
    }
}

void Index::remove(const std::string& key) {
    // Not implemented yet
}

void get_all_record_pages_from_node(std::shared_ptr<Pager> pager, int page_num, std::vector<int>& record_pages) {
    std::unique_ptr<BTreeNode> node = read_btree_node(pager, page_num);
    if (node->is_leaf) {
        for (int record_page : node->record_pointers) {
            record_pages.push_back(record_page);
        }
    } else {
        for (int child_page : node->children) {
            get_all_record_pages_from_node(pager, child_page, record_pages);
        }
    }
}

std::vector<int> Index::get_all_record_pages() {
    std::vector<int> record_pages;
    get_all_record_pages_from_node(pager_, root_page_num_, record_pages);
    return record_pages;
}
