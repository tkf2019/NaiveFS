#ifndef NAIVEFS_INCLUDE_PATH_H_
#define NAIVEFS_INCLUDE_PATH_H_

#include <fcntl.h>
#include <string.h>

#include <vector>

#include "common.h"
#include "utils/logging.h"

namespace naivefs {

class Path {
#define SEPARATOR(__c) (__c == '/')

 public:
  Path(const char* path) : valid_(true), path_(path) {
    ASSERT(strlen(path) > 0);
    if (!SEPARATOR(path[0])) {
      valid_ = false;
      return;
    }
    const char* ptr = path_;
    off_t curr_off = 0;
    while (*ptr != '\0') {
      if (SEPARATOR(*ptr)) ptr++, curr_off++;
      // skip trailing separator
      if (*ptr == '\0') break;
      off_t name_off = curr_off;
      size_t name_len = 0;
      while (!SEPARATOR(ptr[name_len]) && ptr[name_len]) {
        name_len++;
      }
      if (name_len == 0) valid_ = false;
      name_list_.push_back(std::make_pair(path_ + name_off, name_len));
      ptr += name_len, curr_off += name_len;
    }
  }

  Path(const Path& path, size_t slice)
      : valid_(path.valid_), path_(path.path_) {
    name_list_ = std::vector<std::pair<const char*, size_t>>(
        path.name_list_.begin(), path.name_list_.begin() + slice);
  }

  auto begin() const noexcept { return name_list_.begin(); }

  auto end() const noexcept { return name_list_.end(); }

  std::pair<const char*, size_t> get(int i) const noexcept {
    return name_list_[i];
  }

  inline size_t size() const noexcept { return name_list_.size(); }

  inline bool empty() const noexcept { return name_list_.empty(); }

  inline bool valid() const noexcept { return valid_; }

  inline auto back() const noexcept {
    if (name_list_.size() >= 1)
      return name_list_[name_list_.size() - 1];
    else
      return std::make_pair("", (size_t)0);
  }

  inline const char* path() const noexcept { return path_; }

 private:
  bool valid_;
  const char* path_;
  std::vector<std::pair<const char*, size_t>> name_list_;
};
}  // namespace naivefs

#endif