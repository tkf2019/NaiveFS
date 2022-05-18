#ifndef NAIVEFS_INCLUDE_OPTION_H_
#define NAIVEFS_INCLUDE_OPTION_H_

#include <string>

namespace naivefs {
struct options {
  // const char *filename;
  // const char *contents;
  int show_help;
  int begin_check;
  int init_flag;
  const char* password;
};
extern options global_options;
}  // namespace naivefs

#endif