#ifndef NAIVEFS_INCLUDE_CACHE_H_
#define NAIVEFS_INCLUDE_CACHE_H_

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
  BlockCache(size_t size)
      : entries_(new Node[size]),
        head_(new Node),
        tail_(new Node),
        size_(size) {
    for (size_t i = 0; i < size; ++i) {
      free_entries_.push_back(entries_ + i);
    }
    head_->prev_ = nullptr;
    head_->next_ = tail_;
    tail_->prev_ = head_;
    tail_->next_ = nullptr;
  }

  void insert(uint32_t index, Block* block) {
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

  Block* get(uint32_t index) {
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
}  // namespace naivefs

#endif