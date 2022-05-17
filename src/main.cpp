#include <string.h>

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
  naivefs::disk_write(0, 2048, buf + 2048);
  char *str = (char *)malloc(13);
  naivefs::disk_read(0, 2048, buf);
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
  free(buf);
}

void test_path() {
  naivefs::Path path("/home/test.txt");
  for (auto name : path) {
    std::cout << std::string(name.first, name.second) << std::endl;
  }
}

void test_dentry_cache() {
  naivefs::DentryCache *cache = new naivefs::DentryCache(100);

  auto node1 = cache->insert(nullptr, "A", 1, 1);
  auto node2 = cache->insert(nullptr, "B", 1, 2);
  cache->lookup(nullptr, "A", 1);
  auto node3 = cache->insert(node1, "C", 1, 3);
  cache->insert(node3, "D", 1, 4);
  cache->insert(node2, "E", 1, 5);
  cache->lookup(node3, "D", 1);
  cache->lookup(node3, "E", 1);
  cache->lookup(node1, "C", 1);
  delete cache;
}
using namespace std;
void test_filesystem() {
  naivefs::disk_open();
  naivefs::FileSystem *fs = new naivefs::FileSystem();
  ext2_inode *home_inode;
  INFO("%d", fs->inode_create("/home", &home_inode, S_IFDIR));
  ext2_inode *test_inode;
  INFO("%d", fs->inode_create("/home/test.txt", &test_inode, S_IFREG));
  // ext2_inode *test2_inode;
  // INFO("%d", fs->inode_create("/home/test2.txt", &test2_inode, S_IFREG));
  // ext2_inode *home2_inode;
  // INFO("%d", fs->inode_create("/home/tmp", &home2_inode, S_IFDIR));
  // ext2_inode *test3_inode;
  // INFO("%d", fs->inode_create("/home/tmp/test.txt", &test3_inode, S_IFREG));
  // INFO("%d", fs->inode_lookup("/home/tmp/test.txt", &test3_inode));
  // INFO("%d", fs->inode_unlink("/home/tmp"));
  // INFO("%d", fs->inode_lookup("/home/tmp/test.txt", &test3_inode));
  // // ext2_inode *d_test2_inode;
  // INFO("%d", fs->inode_link("/home/test2.txt", "/home/test3.txt"));
  // INFO("%d", fs->inode_unlink("/home/test2.txt"));
  // INFO("%d", fs->inode_lookup("/home/test2.txt", &test2_inode));
  // INFO("%d", fs->inode_create("/home/test2.txt", &test2_inode, S_IFREG));
  // INFO("%d", fs->inode_create("/home/tmp2", &home2_inode, S_IFDIR));
  // INFO("%d", fs->inode_unlink("/home"));
  // for (int i = 0; i < 4096; ++i) {
  //   ext2_inode *dir;
  //   fs->inode_create((std::string("/home/dir") +
  //   std::to_string(i)).c_str(),
  //                    &dir, true);
  // }
  // for (int i = 0; i < 4096; ++i) {
  //   ext2_inode *dir;
  //   fs->inode_lookup((std::string("/home/dir") +
  //   std::to_string(i)).c_str(),
  //                    &dir);
  // }
  for (int i = 0; i < 32768; ++i) {
    naivefs::Block *block;
    uint32_t index;
    fs->alloc_block(&block, &index, test_inode);
  }
  delete fs;
  naivefs::disk_close();
}

int main(int argc, char *argv[]) {
  logging_open("test.log");
  INFO("log begin");

  // test_filesystem();

  int ret;
  fuse_args args = FUSE_ARGS_INIT(argc, argv);
  if (fuse_opt_parse(&args, &global_options, option_spec, NULL) == -1) return 1;
  if (global_options.show_help) {
    show_help(argv[0]);
    assert(fuse_opt_add_arg(&args, "--help") == 0);
    args.argv[0][0] = '\0';
  }
  ops.init = naivefs::fuse_init;
  ops.create = naivefs::fuse_create;
  ops.getattr = naivefs::fuse_getattr;
  ops.readdir = naivefs::fuse_readdir;
  ops.open = naivefs::fuse_open;
  ops.read = naivefs::fuse_read;
  ops.write = naivefs::fuse_write;
  ops.mkdir = naivefs::fuse_mkdir;
  ops.rmdir = naivefs::fuse_rmdir;
  ops.link = naivefs::fuse_link;
  ops.unlink = naivefs::fuse_unlink;
  ops.destroy = naivefs::fuse_destroy;
  ops.rename = naivefs::fuse_rename;
  ops.access = naivefs::fuse_access;
  ops.release = naivefs::fuse_release;
  ops.fsync = naivefs::fuse_fsync;
  ops.chmod = naivefs::fuse_chmod;
  ops.symlink = naivefs::fuse_symlink;
  ops.readlink = naivefs::fuse_readlink;
  ops.truncate = naivefs::fuse_truncate;
  ops.utimens = naivefs::fuse_utimens;
  ops.flush = naivefs::fuse_flush;
  ops.chown = naivefs::fuse_chown;
  ret = fuse_main(args.argc, args.argv, &ops, NULL);
  fuse_opt_free_args(&args);
  return ret;
}