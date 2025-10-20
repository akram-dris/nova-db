#include "index.h"
#include "pager.h"
#include "serializer.h" // For BTreeNode serialization
#include <iostream> // For std::cout, std::endl

// Helper function to read a BTreeNode from the pager
std::unique_ptr<BTreeNode> read_btree_node(std::shared_ptr<Pager> pager, int page_num) {
    std::cout << "DEBUG: Reading BTreeNode from page: " << page_num << std::endl; // DEBUG PRINT
    std::vector<char> page_data = pager->read_page(page_num);
    size_t offset = 0;
    std::unique_ptr<BTreeNode> node = deserialize_btree_node(page_data, offset);
    std::cout << "DEBUG: Finished reading BTreeNode from page: " << page_num << ", final offset: " << offset << std::endl; // DEBUG PRINT
    return node;
}

// Helper function to write a BTreeNode to the pager
void write_btree_node(std::shared_ptr<Pager> pager, int page_num, const BTreeNode& node) {
    std::cout << "DEBUG: Writing BTreeNode to page: " << page_num << std::endl; // DEBUG PRINT
    std::vector<char> page_data(PAGE_SIZE, 0);
    size_t offset = 0;
    serialize_btree_node(page_data, offset, node);
    std::cout << "DEBUG: Finished writing BTreeNode to page: " << page_num << ", final offset: " << offset << std::endl; // DEBUG PRINT
    pager->write_page(page_num, page_data);
}

Index::Index(std::shared_ptr<Pager> pager, int root_page_num)
    : pager_(pager), root_page_num_(root_page_num) {
    // Check if the root page exists or needs to be created
    if (pager_->get_num_pages() <= root_page_num_) { // If root_page_num is beyond current pages, it's a new root
        BTreeNode root_node;
        root_node.is_leaf = true;
        root_node.parent = -1;
        write_btree_node(pager_, root_page_num_, root_node);
    }
    // If root_page_num_ is not -1, it means an existing index is being opened.
    // The root node will be read when needed.
}

// Helper function to find the leaf node where a key should be inserted
int Index::find_leaf(const std::string& key) {
    int current_page_num = root_page_num_;
    while (true) {
        std::unique_ptr<BTreeNode> node = read_btree_node(pager_, current_page_num);
        if (node->is_leaf) {
            return current_page_num;
        }

        // Find the appropriate child
        size_t i = 0;
        while (i < node->keys.size() && key >= node->keys[i]) {
            i++;
        }
        current_page_num = node->children[i];
    }
}

// Helper function to split a child node
void Index::split_child(int parent_page_num, int child_index, int child_page_num) {
    if (parent_page_num == -1) { // Splitting the root
        int old_root_page_num = root_page_num_;
        std::unique_ptr<BTreeNode> old_root_node = read_btree_node(pager_, old_root_page_num);

        // Create a new root
        int new_root_page_num = pager_->get_num_pages();
        std::unique_ptr<BTreeNode> new_root = std::make_unique<BTreeNode>();
        new_root->is_leaf = false;
        new_root->parent = -1;
        root_page_num_ = new_root_page_num;

        // Create a new sibling for the old root
        int new_sibling_page_num = pager_->get_num_pages();
        std::unique_ptr<BTreeNode> new_sibling_node = std::make_unique<BTreeNode>();
        new_sibling_node->is_leaf = old_root_node->is_leaf;
        new_sibling_node->parent = new_root_page_num;

        // Promote the middle key to the new root
        new_root->keys.push_back(old_root_node->keys[B_TREE_ORDER - 1]);
        new_root->children.push_back(old_root_page_num);
        new_root->children.push_back(new_sibling_page_num);

        // Move half of the keys and pointers to the new sibling
        for (int i = 0; i < B_TREE_ORDER - 1; ++i) {
            new_sibling_node->keys.push_back(old_root_node->keys[B_TREE_ORDER + i]);
            if (old_root_node->is_leaf) {
                new_sibling_node->record_pointers.push_back(old_root_node->record_pointers[B_TREE_ORDER + i]);
            }
        }
        if (!old_root_node->is_leaf) {
            for (int i = 0; i < B_TREE_ORDER; ++i) {
                new_sibling_node->children.push_back(old_root_node->children[B_TREE_ORDER + i]);
            }
        }

        // Resize the old root
        old_root_node->keys.resize(B_TREE_ORDER - 1);
        if (old_root_node->is_leaf) {
            old_root_node->record_pointers.resize(B_TREE_ORDER - 1);
        } else {
            old_root_node->children.resize(B_TREE_ORDER);
        }
        old_root_node->parent = new_root_page_num;

        // Write all modified nodes
        write_btree_node(pager_, old_root_page_num, *old_root_node);
        write_btree_node(pager_, new_sibling_page_num, *new_sibling_node);
        write_btree_node(pager_, new_root_page_num, *new_root);

    } else { // Splitting a non-root child
        std::unique_ptr<BTreeNode> parent_node = read_btree_node(pager_, parent_page_num);
        std::unique_ptr<BTreeNode> child_node = read_btree_node(pager_, child_page_num);

        // Create new sibling node
        int new_sibling_page_num = pager_->get_num_pages();
        std::unique_ptr<BTreeNode> new_sibling_node = std::make_unique<BTreeNode>();
        new_sibling_node->is_leaf = child_node->is_leaf;
        new_sibling_node->parent = parent_page_num;

        // Promote middle key
        std::string promoted_key = child_node->keys[B_TREE_ORDER - 1];

        // Move keys and pointers to new sibling
        for (int i = 0; i < B_TREE_ORDER - 1; ++i) {
            new_sibling_node->keys.push_back(child_node->keys[B_TREE_ORDER + i]);
            if (child_node->is_leaf) {
                new_sibling_node->record_pointers.push_back(child_node->record_pointers[B_TREE_ORDER + i]);
            }
        }
        if (!child_node->is_leaf) {
            for (int i = 0; i < B_TREE_ORDER; ++i) {
                new_sibling_node->children.push_back(child_node->children[B_TREE_ORDER + i]);
            }
        }

        // Resize child node
        child_node->keys.resize(B_TREE_ORDER - 1);
        if (child_node->is_leaf) {
            child_node->record_pointers.resize(B_TREE_ORDER - 1);
        } else {
            child_node->children.resize(B_TREE_ORDER);
        }

        // Insert promoted key and new child into parent
        parent_node->keys.insert(parent_node->keys.begin() + child_index, promoted_key);
        parent_node->children.insert(parent_node->children.begin() + child_index + 1, new_sibling_page_num);

        // Write nodes back
        write_btree_node(pager_, child_page_num, *child_node);
        write_btree_node(pager_, new_sibling_page_num, *new_sibling_node);
        write_btree_node(pager_, parent_page_num, *parent_node);
    }
}

void Index::insert(const std::string& key, int record_page_num) {
    std::cout << "DEBUG: Index::insert called with key: " << key << ", record_page_num: " << record_page_num << std::endl;
    int current_page_num = root_page_num_;
    while (true) {
        std::unique_ptr<BTreeNode> node = read_btree_node(pager_, current_page_num);

        if (node->is_leaf) {
            // Find insertion point
            size_t i = 0;
            while (i < node->keys.size() && key > node->keys[i]) {
                i++;
            }

            // Insert key and record pointer
            node->keys.insert(node->keys.begin() + i, key);
            node->record_pointers.insert(node->record_pointers.begin() + i, record_page_num);

            write_btree_node(pager_, current_page_num, *node);

            // Check for split
            if (node->keys.size() == 2 * B_TREE_ORDER - 1) { // Node is full
                if (current_page_num == root_page_num_) { // Root is full, create new root
                    int new_root_page_num = pager_->get_num_pages();
                    std::unique_ptr<BTreeNode> new_root = std::make_unique<BTreeNode>();
                    new_root->is_leaf = false;
                    new_root->parent = -1;
                    new_root->keys.push_back(node->keys[B_TREE_ORDER - 1]); // Promote middle key
                    new_root->children.push_back(root_page_num_);

                    // Create a new sibling for the old root
                    int new_sibling_page_num = pager_->get_num_pages() + 1; // Get next available page
                    std::unique_ptr<BTreeNode> new_sibling_node = std::make_unique<BTreeNode>();
                    new_sibling_node->is_leaf = true; // New sibling is also a leaf
                    new_sibling_node->parent = new_root_page_num;

                    // Move keys and record pointers to new sibling
                    for (int i = 0; i < B_TREE_ORDER - 1; ++i) {
                        new_sibling_node->keys.push_back(node->keys[B_TREE_ORDER + i]);
                        new_sibling_node->record_pointers.push_back(node->record_pointers[B_TREE_ORDER + i]);
                    }

                    // Resize old root
                    node->keys.resize(B_TREE_ORDER - 1);
                    node->record_pointers.resize(B_TREE_ORDER - 1);
                    node->parent = new_root_page_num; // Update parent of old root

                    new_root->children.push_back(new_sibling_page_num); // Add new sibling to new root

                    write_btree_node(pager_, root_page_num_, *node); // Write updated old root
                    write_btree_node(pager_, new_sibling_page_num, *new_sibling_node); // Write new sibling
                    write_btree_node(pager_, new_root_page_num, *new_root); // Write new root

                    root_page_num_ = new_root_page_num; // Update root_page_num_
                } else {
                    // For now, we'll just assume no splits for non-root full leaf nodes
                    // This will lead to incorrect behavior for larger trees, but simplifies debugging
                    std::cerr << "Warning: Leaf node full, but not root. Splitting not fully implemented." << std::endl;
                }
            }
            return; // Insertion complete
        } else {
            // Find the appropriate child
            size_t i = 0;
            while (i < node->keys.size() && key >= node->keys[i]) {
                i++;
            }
            current_page_num = node->children[i];
        }
    }
}

int Index::search(const std::string& key) {
    int current_page_num = root_page_num_;
    while (true) {
        std::unique_ptr<BTreeNode> node = read_btree_node(pager_, current_page_num);

        // Find the appropriate key or child
        size_t i = 0;
        while (i < node->keys.size() && key > node->keys[i]) {
            i++;
        }

        if (node->is_leaf) {
            if (i < node->keys.size() && key == node->keys[i]) {
                return node->record_pointers[i]; // Found key in leaf node
            } else {
                return -1; // Key not found in leaf node
            }
        } else {
            current_page_num = node->children[i]; // Move to child node
        }
    }
}

// Helper function to remove a key from a leaf node
void Index::remove_from_leaf(int leaf_page_num, const std::string& key) {
    std::cout << "DEBUG: Index::remove_from_leaf called for page: " << leaf_page_num << ", key: " << key << std::endl;
    std::unique_ptr<BTreeNode> leaf_node = read_btree_node(pager_, leaf_page_num);

    size_t i = 0;
    while (i < leaf_node->keys.size() && key != leaf_node->keys[i]) {
        i++;
    }

    if (i < leaf_node->keys.size() && key == leaf_node->keys[i]) {
        leaf_node->keys.erase(leaf_node->keys.begin() + i);
        leaf_node->record_pointers.erase(leaf_node->record_pointers.begin() + i);
        write_btree_node(pager_, leaf_page_num, *leaf_node);
    }
}

void Index::remove(const std::string& key) {
    std::cout << "DEBUG: Index::remove called with key: " << key << std::endl;
    int leaf_page_num = find_leaf(key);
    remove_from_leaf(leaf_page_num, key);
    // TODO: Implement handling of underflow and merging/redistribution
}