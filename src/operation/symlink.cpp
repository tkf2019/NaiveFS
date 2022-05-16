#include "operation.h"

namespace naivefs {

extern std::shared_mutex _big_lock;
int fuse_symlink(const char *src, const char *dst) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  INFO("SYMLINK %s, %s", src, dst);

  ext2_inode *inode;
  RetCode ret = fs->inode_lookup(src, &inode);
  if (ret) return Code2Errno(ret);

  uint32_t inode_id;
  ret = fs->inode_create(dst, &inode, &inode_id, S_IFLNK);
  if (ret) return Code2Errno(ret);

  size_t src_len = strlen(src);

  if (src_len <= sizeof(ext2_inode::i_block)) {
    memcpy(inode->i_block, src, src_len);
  } else {
    Block *block;
    uint32_t block_id;
    if (!fs->alloc_block(&block, &block_id, inode))
      return Code2Errno(FS_ALLOC_ERR);
    fs->write_block(block, block_id, src, src_len);
  }

  return 0;
}

int fuse_readlink(const char *path, char *buf, size_t size) {
  std::unique_lock<std::shared_mutex> __lck(_big_lock);
  INFO("READLINK %s", path);

  ext2_inode *inode;
  uint32_t inode_id;
  RetCode ret = fs->inode_lookup(path, &inode, &inode_id);
  if (ret) return Code2Errno(ret);

  if (inode->i_blocks == 0) {
    memcpy(buf, inode->i_block,
           std::min(size, strlen(reinterpret_cast<char *>(inode->i_block))));
  } else {
    Block *block;
    if (!fs->get_block(inode->i_block[0], &block))
      return Code2Errno(FS_NOT_FOUND);
    memcpy(buf, block->get(),
           std::min(size, strlen(reinterpret_cast<char *>(block->get()))));
  }
  return 0;
}
}  // namespace naivefs