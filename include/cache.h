#ifndef NAIVEFS_INCLUDE_CACHE_H_
#define NAIVEFS_INCLUDE_CACHE_H_

#include <unordered_map>
#include <vector>

#include "common.h"

namespace naivefs {

template <typename K, typename V>
struct Node {
  K key;
  V value;
  Node* prev;
  Node* next;
};

template <typename K, typename V>
class LRUCache {
 public:
  LRUCache(int size)
      : entries_(new Node<K, V>[size]),
        head_(new Node<K, V>),
        tail_(new Node<K, V>) {
    for (int i = 0; i < size; ++i) {
      free_entries_.push_back(entries_ + i);
    }
    head_->prev = nullptr;
    head_->next = tail_;
    tail_->prev = head_;
    tail_->next = nullptr;
  }

  void insert(const K& key, const V& value) {
    Node<K, V>* node = map_[key];
    if (node == nullptr) {
      if (free_entries_.empty()) {
        // Remove the node not used recently
        node = tail_->prev;
        detach(node);
        map_.erase(node->key);
      } else {
        node = free_entries_.back();
        free_entries_.pop_back();
      }
      node->key = key;
      node->value = value;
      map_[key] = node;
      attach(node);
    } else {
      detach(node);
      node->value = value;
      // Insert the node next to the head
      attach(node);
    }
  }

  V get(const K& key) {
    Node<K, V>* node = map_[key];
    if (node == nullptr) {
      return V();
    } else {
      // Move the node to the head
      detach(node);
      attach(node);
      return node->value;
    }
  }

  ~LRUCache() {
    delete head_;
    delete tail_;
    delete[] entries_;
  }

 private:
  std::unordered_map<K, Node<K, V>*> map_;
  std::vector<Node<K, V>*> free_entries_;
  Node<K, V>* entries_;
  Node<K, V>*head_, *tail_;

  // TODO: multi-thread
  // Maybe one lock for one node
  inline void detach(Node<K, V>* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
  }

  inline void attach(Node<K, V>* node) {
    node->prev = head_;
    node->next = head_->next;
    head_->next = node;
    node->next->prev = node;
  }
};
}  // namespace naivefs

#endif