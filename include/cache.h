#ifndef NAIVEFS_INCLUDE_CACHE_H_
#define NAIVEFS_INCLUDE_CACHE_H_

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "block.h"
#include "common.h"

namespace naivefs {

class BlockCache {
  struct Node {
    bool dirty_;
    uint32_t index_;
    Block* block_;
    Node* prev_;
    Node* next_;
  };

 public:
  BlockCache(size_t size);

  ~BlockCache();

  void insert(uint32_t index, Block* block);

  Block* get(uint32_t index);

  void modify(uint32_t index);

 private:
  inline void detach(Node* node) {
    node->prev_->next_ = node->next_;
    node->next_->prev_ = node->prev_;
  }

  inline void release(Node* node) {
    if (node->dirty_) {
      // write back modified block
      node->block_->flush();
      // release the memory
      delete node->block_;
    }
  }

  inline void attach(Node* node) {
    node->prev_ = head_;
    node->next_ = head_->next_;
    head_->next_ = node;
    node->next_->prev_ = node;
  }

 private:
  std::unordered_map<uint32_t, Node*> map_;
  std::vector<Node*> free_entries_;
  Node* entries_;
  Node *head_, *tail_;
  size_t size_;
};

class DentryCache {
 public:
  struct Node {
    uint32_t inode_;
    uint8_t name_len_;
    Node* childs_;
    Node* next_;
    char name_[]; /* File name, up to EXT2_NAME_LEN */
  };

  DentryCache(size_t size);

  ~DentryCache();

  /* Inserts a node as a childs of a given parent.  The parent is updated to
   * point the newly inserted childs as the first childs.  We return the new
   * entry so that further entries can be inserted.
   *
   *      [0]                  [0]
   *       /        ==>          \
   *      /         ==>           \
   * .->[1]->[2]-.       .->[1]->[3]->[2]-.
   * `-----------´       `----------------´
   */
  Node* insert(Node* parent, const char* name, size_t name_len, uint32_t inode);

  /* Lookup a cache entry for a given file name.  Return value is a struct
   * pointer that can be used to both obtain the inode number and insert further
   * child entries. */
  Node* lookup(Node* parent, const char* name, size_t name_len);

  void release_node(Node* parent);

  void alloc_new_node(Node** node, const char* name, size_t name_len);

 private:
  Node* root_;
  size_t size_;
  size_t max_size_;
};  // namespace naivefs

}  // namespace naivefs

#endif