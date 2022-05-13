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
  Path(char* path) : path_(path) {
    ASSERT(strlen(path) > 0);
    const char* ptr = path_;
    off_t curr_off = 0;
    while (*ptr != '\0') {
      if (SEPARATOR(*ptr)) ptr++, curr_off++;
      // skip trailing separator
      if (*ptr == '\0') break;
      off_t name_off = curr_off;
      size_t name_len = 0;
      while (!SEPARATOR(ptr[name_len]) && ptr[name_len]) name_len++;
      name_list_.push_back(std::make_pair(path_ + name_off, name_len));
      ptr += name_len, curr_off += name_len;
    }
  }

  auto begin() const noexcept { return name_list_.begin(); }

  auto end() const noexcept { return name_list_.end(); }

  std::pair<char*, size_t> get(int i) const noexcept { return name_list_[i]; }

  inline size_t size() const noexcept { return name_list_.size(); }

  inline bool empty() const noexcept { return name_list_.empty(); }

 private:
  char* path_;

  std::vector<std::pair<char*, size_t>> name_list_;
};
}  // namespace naivefs

#endif