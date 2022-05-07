#include <iostream>

#include "operation.h"
#include "option.h"

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

int main(int argc, char *argv[]) {
  int ret;
  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  if (fuse_opt_parse(&args, &global_options, option_spec, NULL) == -1) return 1;
  if (global_options.show_help) {
    show_help(argv[0]);
    assert(fuse_opt_add_arg(&args, "--help") == 0);
    args.argv[0][0] = '\0';
  }
  ops.init = naivefs::init;
  ops.getattr = naivefs::getattr;
  ops.readdir = naivefs::readdir;
  ops.open = naivefs::open;
  ops.read = naivefs::read;
  ret = fuse_main(args.argc, args.argv, &ops, NULL);
  fuse_opt_free_args(&args);
  return ret;
}