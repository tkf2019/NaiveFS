#ifndef _AES__H__
#define _AES__H__
#include "AES.h"
#include <cstdio>

namespace naivefs {
class Auth {
 public:
  explicit Auth(const char* s);

  ~Auth();

  void read(unsigned char* s, size_t len);

  unsigned char* write(unsigned char* s, size_t len);

  bool flag() {return flag_;}

 private:
  unsigned char* key_;
  AES aes_;
  bool flag_;
};
}  // namespace naivefs


#endif