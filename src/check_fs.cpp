#include <set>

#include "block.h"
#include "filesystem.h"
#include "utils/disk.h"
#include "utils/option.h"

#define FUSE_USE_VERSION 31
#include <fuse.h>

#include "crypto.h"

namespace naivefs {
// Recall the structure: [Super Block](Block Group)[[Inode Bitmap][Block Bitmap][Inode Table*1024][Data Block*32768]]
extern Auth* auth;
extern FileSystem* fs;

const char super_block_string[] = "See you ultraman, someday somewhere!";

const int FSC_ABORT = 1;
const int FSC_SUCC = 0;
const int FSC_ERR = 2;

static std::vector<ext2_group_desc> desc_table_;

#define FSC_ASSERT(Z)    \
  do {                   \
    int err = Z;         \
    if (err) return err; \
  } while (0)

static int check_num(const char* name, size_t x, size_t L, size_t R) {
  if (x < L || x > R) return printf("%s: %llu violates the range [%llu, %llu]. abort.\n", name, x, L, R), FSC_ABORT;
  return FSC_SUCC;
}

static int check_sb(ext2_super_block* sb) {
  auto data_ = Block(0);
  sb = (ext2_super_block*)data_.get();
  auto super_ = sb;
  switch (super_->s_state) {
    case FSState::UNINIT: {
      printf("unintialized file system. abort.\n");
      return FSC_ABORT;
    }
    case FSState::NORMAL: {
      if (memcmp(super_->s_auth_string, super_block_string, sizeof(super_block_string))) {
        printf("Password wrong, unauthorized access. abort.\n");
        return FSC_ABORT;
      }
      // Super block info

      printf("BLOCK SIZE: %i\n", ((uint64_t)1) << (super_->s_log_block_size + 10));
      FSC_ASSERT(check_num("BLOCK SIZE", ((uint64_t)1) << (super_->s_log_block_size + 10), BLOCK_SIZE, BLOCK_SIZE));
      printf("BLOCK GROUP SIZE: %i\n", super_->s_blocks_per_group);
      FSC_ASSERT(check_num("BLOCK GROUP SIZE", super_->s_blocks_per_group, BLOCKS_PER_GROUP, BLOCKS_PER_GROUP));
      uint32_t block_n = (super_->s_blocks_count + super_->s_blocks_per_group - 1) / super_->s_blocks_per_group;
      uint32_t inode_n = (super_->s_inodes_count + super_->s_inodes_per_group - 1) / super_->s_inodes_per_group;
      uint32_t n = std::max(inode_n, block_n);
      n = n ? n : 1;
      printf("N BlOCK GROUPS: %i\n", n);
      printf("INODE SIZE: %i\n", super_->s_inode_size);
      FSC_ASSERT(check_num("INODE SIZE", super_->s_inode_size, sizeof(ext2_inode), sizeof(ext2_inode)));
      printf("INODES PER GROUP: %i\n", super_->s_inodes_per_group);
      FSC_ASSERT(check_num("INODES PER GROUP", super_->s_inodes_per_group, INODES_PER_GROUP, INODES_PER_GROUP));
      FSC_ASSERT(check_num("FIRST INODE", super_->s_first_ino, ROOT_INODE, ROOT_INODE));
      FSC_ASSERT(check_num("BLOCK COUNT", super_->s_blocks_count, 0, ~0u));
      FSC_ASSERT(check_num("INODE COUNT", super_->s_inodes_count, 1, ~0u));

      ext2_group_desc* ptr = (ext2_group_desc*)(data_.get() + sizeof(ext2_super_block));
      for (uint32_t i = 0; i < n; ++i) {
        desc_table_.push_back(ptr[i]);
      }
      break;
    }
    default: {
      printf("Unknown file system state: %i. abort.\n", super_->s_state);
      return FSC_ABORT;
    }
  }
  return FSC_SUCC;
}

static std::set<std::pair<size_t, size_t>> st;

static int check_group(size_t id, ext2_group_desc* desc) {
  printf("%llu-th BLOCK GROUP.\n", id);

  printf("BLOCK BITMAP OFFSET: 0x%x\n", desc->bg_block_bitmap);
  printf("INODE BITMAP OFFSET: 0x%x\n", desc->bg_inode_bitmap);
  printf("INODE TABLE OFFSET: 0x%x\n", desc->bg_inode_table);
  printf("FREE BLOCKS COUNT: %i\n", desc->bg_free_blocks_count);
  printf("FREE INODES COUNT: %i\n", desc->bg_free_inodes_count);
  FSC_ASSERT(check_num("FREE BLOCKS COUNT", desc->bg_free_blocks_count, 0, 32768));
  FSC_ASSERT(check_num("FREE INODES COUNT", desc->bg_free_blocks_count, 0, 32768));
  st.insert(std::make_pair(desc->bg_block_bitmap, desc->bg_block_bitmap + BLOCK_SIZE));
  st.insert(std::make_pair(desc->bg_inode_bitmap, desc->bg_inode_bitmap + BLOCK_SIZE));
  size_t lst = 0;
  for (auto a : st)
    if (a.first < lst) {
      printf("Bitmap range conflict. abort.\n");
      return FSC_ABORT;
    } else
      lst = a.second;

  return FSC_SUCC;
}

static std::set<uint32_t> files;

static int dfs(size_t dep, size_t inode) {
  if (dep > 4096) {
    printf("Too deep. abort.\n");
    return FSC_ABORT;
  }
  if (files.count(inode)) {
    printf("Filesystem conflict. abort.\n");
    return FSC_ABORT;
  }
  files.insert(inode);
  ext2_inode* _;
  if (!fs->get_inode(inode, &_)) {
    printf("Get inode failed. abort.\n");
    return FSC_ABORT;
  }
  if (!S_ISDIR(_->i_mode)) return FSC_SUCC;
  printf("enter directory.\n");
  int err = FSC_SUCC;
  fs->visit_inode_blocks(_, [&err, dep](__attribute__((unused)) uint32_t index, Block* block) {
    DentryBlock dentry_block(block);
    for (const auto& dentry : *dentry_block.get()) {
      if (dentry->name_len) {
        printf("directory entry: (%d, inode_id %d) %s.\n", dentry->name_len, dentry->inode, std::string(dentry->name, dentry->name_len).c_str());
        err = !err ? dfs(dep + 1, dentry->inode) : err;
      }
    }
    return false;
  });
  printf("exit directory.\n");
  return err;
}

int check_filesystem() {
  st.clear();
  files.clear();
  int err = disk_open();
  if (err) return printf("Failed to open %s: %s", DISK_NAME, strerror(err)), err;
  auth = new Auth(global_options.password);
  ext2_super_block sb;
  err = check_sb(&sb);
  if (err) return err;

  for (size_t i = 0; i < desc_table_.size(); i++) {
    err = check_group(i, &desc_table_[i]);
    if (err) return err;
  }

  fs = new FileSystem();

  FSC_ASSERT(dfs(0, ROOT_INODE));

  delete fs;
  delete auth;


  err = disk_close();
  if (err) return printf("Failed to close %s: %s", DISK_NAME, strerror(err)), err;
  printf("file system valid.\n");
  return 0;
}
}  // namespace naivefs