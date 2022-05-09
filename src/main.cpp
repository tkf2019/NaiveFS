#include <iostream>

#include "cache.h"
#include "operation.h"
#include "utils/bitmap.h"
#include "utils/disk.h"
#include "utils/option.h"

#define OPTION(t, p) \
  { t, offsetof(naivefs::options, p), 1 }
static const struct fuse_opt option_spec[] = {
    OPTION("-h", show_help), OPTION("--help", show_help), FUSE_OPT_END};
static struct fuse_operations ops;
static void show_help(const char *progname) {
  printf("usage: %s [options] <mountpoint>\n\n", progname);
  printf(
      "File-system specific options:\n"
      "    --name=<s>          Name of the \"hello\" file\n"
      "                        (default: \"hello\")\n"
      "    --contents=<s>      Contents \"hello\" file\n"
      "                        (default \"Hello, World!\\n\")\n"
      "\n");
}

naivefs::options global_options = {.show_help = 0};

void test_cache() {
  // Test LRUCache
  naivefs::LRUCache<std::string, std::string> lru_cache(3);
  lru_cache.insert("a", "abc");
  lru_cache.insert("b", "abc");
  lru_cache.insert("c", "abc");
  std::cout << lru_cache.get("a") << std::endl;
  lru_cache.insert("d", "abc");
  std::cout << lru_cache.get("b") << std::endl;
}

void test_disk() {
  uint8_t *buf = (uint8_t *)naivefs::alloc_aligned(4096);
  memcpy(buf + 2048, "Hello World!", 13);
  naivefs::disk_open();
  int ret = naivefs::disk_write(0, 2048, buf + 2048);
  char *str = (char *)malloc(13);
  ret = naivefs::disk_read(0, 2048, buf);
  memcpy(str, buf, 13);
  std::cout << str << std::endl;
  naivefs::disk_close();
  free(buf);
  free(str);
  std::cout << ret << std::endl;
}

void test_bitmap() {
  void *buf = malloc(4096 * sizeof(int));
  memset(buf, 0, 4096 * sizeof(int));
  auto bitmap = naivefs::Bitmap(buf);
  for (int i = 0; i < 31; ++i) {
    bitmap.set(i);
  }
  bitmap.set(32);
  bitmap.set(34);
  std::cout << bitmap.find(100) << std::endl;
  free(buf);
}

int main(int argc, char *argv[]) {
  logging_open("test.log");
  test_bitmap();
  // int ret;
  // fuse_args args = FUSE_ARGS_INIT(argc, argv);
  // if (fuse_opt_parse(&args, &global_options, option_spec, NULL) == -1)
  // return 1; if (global_options.show_help) {
  //   show_help(argv[0]);
  //   assert(fuse_opt_add_arg(&args, "--help") == 0);
  //   args.argv[0][0] = '\0';
  // }
  // ops.init = naivefs::init;
  // ops.getattr = naivefs::getattr;
  // ops.readdir = naivefs::readdir;
  // ops.open = naivefs::open;
  // ops.read = naivefs::read;
  // ret = fuse_main(args.argc, args.argv, &ops, NULL);
  // fuse_opt_free_args(&args);
  return 0;
}