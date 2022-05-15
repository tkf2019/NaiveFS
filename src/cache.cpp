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

void BlockCache::flush() {
  for (auto& node : map_) {
    if (node.second->dirty_) {
      node.second->block_->flush();
      node.second->dirty_ = false;
    }
  }
}

void BlockCache::insert(uint32_t index, Block* block) {
  DEBUG("[BlockCache] Inserting block %u", index);
  Node* node = nullptr;
  auto iter = map_.find(index);
  if (iter == map_.end()) {
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
    node = iter->second;
    ASSERT(node->block_ == block);
    detach(node);
    attach(node);
  }
}

Block* BlockCache::get(uint32_t index) {
  DEBUG("[BlockCache] Getting block %u", index);
  auto iter = map_.find(index);
  if (iter == map_.end()) {
    return nullptr;
  } else {
    Node* node = iter->second;
    ASSERT(node->index_ == index);
    detach(node);
    attach(node);
    return node->block_;
  }
}

void BlockCache::remove(uint32_t index) {
  DEBUG("[BlockCache] Removing block %u", index);
  auto iter = map_.find(index);
  if (iter == map_.end()) {
    return;
  } else {
    Node* node = iter->second;
    ASSERT(index == node->index_);
    detach(node);
    release(node);
    map_.erase(node->index_);
  }
}

void BlockCache::modify(uint32_t index) {
  if (map_.find(index) == map_.end()) return;
  map_[index]->dirty_ = true;
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
  DEBUG("[DentryCache] Inserting dentry %s,%u",
        std::string(name, name_len).c_str(), inode);

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
    DEBUG("[DentryCache] Looking up %s: Not found (no child)",
          std::string(name, name_len).c_str());
    return nullptr;
  }

  Node* ptr = parent->childs_;
  do {
    if (ptr->name_len_ == name_len &&
        strncmp(ptr->name_, name, name_len) == 0) {
      parent->childs_ = ptr;
      DEBUG("[DentryCache] Looking up %s: Found",
            std::string(name, name_len).c_str());
      return ptr;
    }
    ptr = ptr->next_;
  } while (ptr != parent->childs_);
  DEBUG("[DentryCache] Looking up %s: Not found (no match)",
        std::string(name, name_len).c_str());
  return nullptr;
}

void DentryCache::remove(Node* parent, const char* name, size_t name_len) {
  if (parent == nullptr) parent = root_;

  if (parent->childs_ == nullptr) {
    DEBUG("[DentryCache] Removing %s: Not found (no child)",
          std::string(name, name_len).c_str());
    return;
  }

  Node* ptr = parent->childs_->next_;
  Node* prev = ptr;
  do {
    if (ptr->name_len_ == name_len &&
        strncmp(ptr->name_, name, name_len) == 0) {
      DEBUG("[DentryCache] Removing %s: Found",
            std::string(name, name_len).c_str());
      parent->childs_ = (ptr == ptr->next_) ? nullptr : ptr->next_;
      prev->next_ = ptr->next_;
      release_node(ptr);
      free(ptr);
      return;
    }
    prev = ptr;
    ptr = ptr->next_;
  } while (ptr != parent->childs_);
  DEBUG("[DentryCache] Removing %s: Not found (no match)",
        std::string(name, name_len).c_str());
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