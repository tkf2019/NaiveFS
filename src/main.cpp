#include <iostream>

#include "cache.h"
#include "operation.h"
#include "utils/bitmap.h"
#include "utils/disk.h"
#include "utils/option.h"
#include "utils/path.h"

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

void test_path() {
  naivefs::Path path("/A/SasdfB/C/D/E/");
  for (auto name : path) {
    std::cout << std::string(name.first, name.second) << std::endl;
  }
}

int main(int argc, char *argv[]) {
  logging_open("test.log");
  // test_bitmap();
  // test_disk();
  // test_path();
  int ret;
  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  if (fuse_opt_parse(&args, &global_options, option_spec, NULL) == -1) return 1;
  if (global_options.show_help) {
    show_help(argv[0]);
    assert(fuse_opt_add_arg(&args, "--help") == 0);
    args.argv[0][0] = '\0';
  }
  ops.init = naivefs::fuse_init;
  ops.getattr = naivefs::fuse_getattr;
  ops.readdir = naivefs::fuse_readdir;
  ops.open = naivefs::fuse_open;
  ops.read = naivefs::fuse_read;
  ret = fuse_main(args.argc, args.argv, &ops, NULL);
  fuse_opt_free_args(&args);
  return ret;
}