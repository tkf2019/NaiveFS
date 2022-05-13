#include "cache.h"

namespace naivefs {
BlockCache::BlockCache(size_t size)
    : entries_(new Node[size]), head_(new Node), tail_(new Node), size_(size) {
  for (size_t i = 0; i < size; ++i) {
    free_entries_.push_back(entries_ + i);
  }
  head_->prev_ = nullptr;
  head_->next_ = tail_;
  tail_->prev_ = head_;
  tail_->next_ = nullptr;
}

BlockCache::~BlockCache() {
  delete head_;
  delete tail_;
  delete[] entries_;
}

void BlockCache::insert(uint32_t index, Block* block) {
  Node* node = nullptr;
  if (map_.find(index) == map_.end()) {
    if (free_entries_.empty()) {
      node = tail_->prev_;
      detach(node);
      release(node);
      map_.erase(node->index_);
    } else {
      node = free_entries_.back();
      free_entries_.pop_back();
    }
    node->index_ = index;
    node->block_ = block;
    map_[index] = node;
    attach(node);
  } else {
    node = map_[index];
    ASSERT(node->block_ == block);
    detach(node);
    attach(node);
  }
}

Block* BlockCache::get(uint32_t index) {
  Node* node = nullptr;
  if (map_.find(index) == map_.end()) {
    return nullptr;
  } else {
    node = map_[index];
    ASSERT(node->index_ == index);
    detach(node);
    attach(node);
    return node->block_;
  }
}

DentryCache::DentryCache(size_t size) : size_(1), max_size_(size) {
  alloc_new_node(&root_, "/", 1);
  root_->inode_ = ROOT_INODE;
}

DentryCache::~DentryCache() {
  release_node(root_);
  free(root_);
}

DentryCache::Node* DentryCache::insert(Node* parent, const char* name,
                                       size_t name_len, uint32_t inode) {
  DEBUG("Inserting %s,%d to dentry cache", name, name_len);

  if (parent == nullptr) parent = root_;

  Node* new_node;
  alloc_new_node(&new_node, name, name_len);
  new_node->inode_ = inode;

  if (parent->childs_ == nullptr) {
    new_node->next_ = new_node;
    parent->childs_ = new_node;
  } else {
    new_node->next_ = parent->childs_->next_;
    parent->childs_->next_ = new_node;
    parent->childs_ = new_node;
  }

  return new_node;
}

DentryCache::Node* DentryCache::lookup(Node* parent, const char* name,
                                       size_t name_len) {
  if (parent == nullptr) parent = root_;

  if (parent->childs_ == nullptr) {
    DEBUG("Looking up %s,%d: Not found (no childs)", name, name_len);
    return nullptr;
  }

  Node* ptr = parent->childs_;
  do {
    if (ptr->name_len_ == name_len &&
        strncmp(ptr->name_, name, name_len) == 0) {
      parent->childs_ = ptr;
      DEBUG("Looking up %s,%d: Found", name, name_len);
      return ptr;
    }
    ptr = ptr->next_;
  } while (ptr != parent->childs_);
  DEBUG("Looking up %s,%d: Not found", name, name_len);
  return nullptr;
}

void DentryCache::release_node(Node* parent) {
  if (parent == nullptr) return;
  Node* ptr = parent->childs_;
  if (ptr == nullptr) return;
  do {
    Node* next = ptr->next_;
    release_node(ptr);
    free(ptr);
    ptr = next;
  } while (ptr != parent->childs_);
  parent->childs_ = nullptr;
}

void DentryCache::alloc_new_node(Node** node, const char* name,
                                 size_t name_len) {
  Node* new_node = (Node*)calloc(1, sizeof(Node) + name_len);
  new_node->childs_ = new_node->next_ = nullptr;
  ASSERT(name_len + 1 <= EXT2_NAME_LEN);
  new_node->name_len_ = name_len;
  strncpy(new_node->name_, name, name_len);
  // new_node->name_[name_len] = 0;
  *node = new_node;
}
}  // namespace naivefs