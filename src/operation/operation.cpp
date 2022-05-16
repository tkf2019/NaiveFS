#include "operation.h"
namespace naivefs {
int FileStatus::next_block() {
  INFO("next_block: %d", block_id_in_file_);
  if (block_id_in_file_ <= IBLOCK_11) {
    // The first 12 blocks
    block_id_in_file_++;
    if (block_id_in_file_ <= IBLOCK_11) {
      block_id_ = inode_cache_->cache_->i_block[block_id_in_file_];
    } else {
      indirect_block_[0] = IndirectBlockPtr(inode_cache_->cache_->i_block[12]);
      if (!indirect_block_[0].seek(0, block_id_)) return -EINVAL;
    }
  } else if (block_id_in_file_ <= IBLOCK_12) {
    // The first indirect block
    block_id_in_file_++;
    if (block_id_in_file_ <= IBLOCK_12) {
      if (!indirect_block_[0].seek(block_id_in_file_ - IBLOCK_11 - 1, block_id_)) return -EINVAL;
    } else {
      indirect_block_[0] = IndirectBlockPtr(inode_cache_->cache_->i_block[13]);
      if (!indirect_block_[0].seek(0, indirect_block_[1].id_)) return -EINVAL;
      if (!indirect_block_[1].seek(0, block_id_)) return -EINVAL;
    }
  } else if (block_id_in_file_ <= IBLOCK_13) {
    // The double indirect block
    block_id_in_file_++;
    if (block_id_in_file_ <= IBLOCK_13) {
      uint32_t first_id = (block_id_in_file_ - IBLOCK_12 - 1) / (BLOCK_SIZE / 4);
      uint32_t second_id = (block_id_in_file_ - IBLOCK_12 - 1) % (BLOCK_SIZE / 4);
      if (second_id == 0) {
        if (!indirect_block_[0].seek(first_id, indirect_block_[1].id_)) return -EINVAL;
        if (!indirect_block_[1].seek(0, block_id_)) return -EINVAL;
      } else if (!indirect_block_[1].seek(second_id, block_id_))
        return -EINVAL;
    } else {
      indirect_block_[0] = IndirectBlockPtr(inode_cache_->cache_->i_block[14]);
      if (!indirect_block_[0].seek(0, indirect_block_[1].id_)) return -EINVAL;
      if (!indirect_block_[1].seek(0, indirect_block_[2].id_)) return -EINVAL;
      if (!indirect_block_[2].seek(0, block_id_)) return -EINVAL;
    }
  } else if (block_id_in_file_ <= IBLOCK_14) {
    // The triple indirect block
    block_id_in_file_++;
    uint32_t first_id = (block_id_in_file_ - IBLOCK_13 - 1) / ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4));
    uint32_t second_id = (block_id_in_file_ - IBLOCK_13 - 1) % ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4)) / (BLOCK_SIZE / 4);
    uint32_t third_id = (block_id_in_file_ - IBLOCK_13 - 1) % (BLOCK_SIZE / 4);
    if (third_id == 0) {
      if (second_id == 0) {
        if (!indirect_block_[0].seek(first_id, indirect_block_[1].id_)) return -EINVAL;
        if (!indirect_block_[1].seek(second_id, indirect_block_[2].id_)) return -EINVAL;
        if (!indirect_block_[2].seek(third_id, block_id_)) return -EINVAL;
      } else {
        if (!indirect_block_[1].seek(second_id, indirect_block_[2].id_)) return -EINVAL;
        if (!indirect_block_[2].seek(third_id, block_id_)) return -EINVAL;
      }
    } else if (!indirect_block_[2].seek(third_id, block_id_))
      return -EINVAL;
  } else
    return -EINVAL;
  return 0;
}

int FileStatus::seek(uint32_t new_block_id_in_file) {
  if (new_block_id_in_file <= IBLOCK_12) {
    bf_seek(new_block_id_in_file);
  } else if (new_block_id_in_file <= IBLOCK_13) {
    uint32_t old_first_id = (block_id_in_file_ - IBLOCK_12 - 1) / (BLOCK_SIZE / 4);
    uint32_t old_second_id = (block_id_in_file_ - IBLOCK_12 - 1) % (BLOCK_SIZE / 4);
    uint32_t new_first_id = (new_block_id_in_file - IBLOCK_12 - 1) / (BLOCK_SIZE / 4);
    uint32_t new_second_id = (new_block_id_in_file - IBLOCK_12 - 1) % (BLOCK_SIZE / 4);
    block_id_in_file_ = new_block_id_in_file;
    if (new_first_id == old_first_id && new_second_id == old_second_id) {
      return 0;
    } else if (new_first_id == old_first_id) {
      if (!indirect_block_[1].seek(new_second_id, block_id_)) return -EINVAL;
    } else
      return bf_seek(new_block_id_in_file);
  } else if (new_block_id_in_file <= IBLOCK_14) {
    uint32_t old_first_id = (block_id_in_file_ - IBLOCK_13 - 1) / ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4));
    uint32_t old_second_id = (block_id_in_file_ - IBLOCK_13 - 1) % ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4)) / (BLOCK_SIZE / 4);
    uint32_t old_third_id = (block_id_in_file_ - IBLOCK_13 - 1) % (BLOCK_SIZE / 4);
    uint32_t new_first_id = (new_block_id_in_file - IBLOCK_13 - 1) / ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4));
    uint32_t new_second_id = (new_block_id_in_file - IBLOCK_13 - 1) % ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4)) / (BLOCK_SIZE / 4);
    uint32_t new_third_id = (new_block_id_in_file - IBLOCK_13 - 1) % (BLOCK_SIZE / 4);
    block_id_in_file_ = new_block_id_in_file;
    if (new_first_id == old_first_id && new_second_id == old_second_id && new_third_id == old_third_id) {
      return 0;
    } else if (new_first_id == old_first_id && new_second_id == old_second_id) {
      if (!indirect_block_[2].seek(new_second_id, block_id_)) return -EINVAL;
    } else if (new_first_id == old_first_id) {
      if (!indirect_block_[1].seek(new_second_id, indirect_block_[2].id_)) return -EINVAL;
      if (!indirect_block_[2].seek(new_second_id, block_id_)) return -EINVAL;
    } else
      return bf_seek(new_block_id_in_file);
  }
  return 0;
}

int FileStatus::bf_seek(uint32_t new_block_id_in_file) {
  block_id_in_file_ = new_block_id_in_file;
  if (block_id_in_file_ <= IBLOCK_11) {
    // the 12 direct blocks
    block_id_ = inode_cache_->cache_->i_block[block_id_in_file_];
  } else if (block_id_in_file_ <= IBLOCK_12) {
    // The first indirect block
    uint32_t first_id = block_id_in_file_ - IBLOCK_11 - 1;
    indirect_block_[0] = IndirectBlockPtr(inode_cache_->cache_->i_block[12]);
    if (!indirect_block_[0].seek(first_id, block_id_)) return -EINVAL;
  } else if (block_id_in_file_ <= IBLOCK_13) {
    // The double indirect block
    uint32_t first_id = (block_id_in_file_ - IBLOCK_12 - 1) / (BLOCK_SIZE / 4);
    uint32_t second_id = (block_id_in_file_ - IBLOCK_12 - 1) % (BLOCK_SIZE / 4);
    indirect_block_[0] = IndirectBlockPtr(inode_cache_->cache_->i_block[13]);
    if (!indirect_block_[0].seek(first_id, indirect_block_[1].id_)) return -EINVAL;
    if (!indirect_block_[1].seek(second_id, block_id_)) return -EINVAL;
  } else if (block_id_in_file_ <= IBLOCK_14) {
    // The triple indirect block
    uint32_t first_id = (block_id_in_file_ - IBLOCK_13 - 1) / ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4));
    uint32_t second_id = (block_id_in_file_ - IBLOCK_13 - 1) % ((BLOCK_SIZE / 4) * (BLOCK_SIZE / 4)) / (BLOCK_SIZE / 4);
    uint32_t third_id = (block_id_in_file_ - IBLOCK_13 - 1) % (BLOCK_SIZE / 4);
    indirect_block_[0] = IndirectBlockPtr(inode_cache_->cache_->i_block[14]);
    if (!indirect_block_[0].seek(first_id, indirect_block_[1].id_)) return -EINVAL;
    if (!indirect_block_[1].seek(second_id, indirect_block_[2].id_)) return -EINVAL;
    if (!indirect_block_[2].seek(third_id, block_id_)) return -EINVAL;
  } else
    return -EINVAL;
  return 0;
}

int FileStatus::copy_to_buf(char* buf, size_t offset, size_t size) {
  std::unique_lock<std::shared_mutex> lck(rwlock);
  std::shared_lock<std::shared_mutex> lck_inode(inode_cache_->inode_rwlock_);
  size_t isize = file_size();
  if (offset >= isize) return 0;
  int _err_ret = _upd_cache();
  if (_err_ret) return _err_ret;

  _err_ret = seek(offset / BLOCK_SIZE);
  if (_err_ret) return _err_ret;
  Block* blk;
  size_t ret = 0;
  size_t csz = std::min(size, BLOCK_SIZE - (size_t)offset % BLOCK_SIZE);
  if (!fs->get_block(block_id_, &blk, false, offset % BLOCK_SIZE, buf, csz)) return -EINVAL;
  // memcpy(buf, blk->get());
  ret += csz, size -= csz, offset += csz, buf += csz;
  INFO("copy_to_buf: ret: %llu, size: %llu, offset: %llu", ret, size, offset);
  while (size) {
    if (offset >= isize) return ret;
    _err_ret = next_block();
    csz = std::min(size, std::min((size_t)isize - (size_t)offset, (size_t)BLOCK_SIZE));
    if (_err_ret) return _err_ret;
    if (!fs->get_block(block_id_, &blk, false, 0, buf, csz)) return -EINVAL;
    // memcpy(buf, blk->get(), csz);
    ret += csz, size -= csz, offset += csz, buf += csz;
  }
  return ret;
}

int FileStatus::write(const char* buf, size_t offset, size_t size, bool append_flag) {
  std::unique_lock<std::shared_mutex> lck(rwlock);
  inode_cache_->lock_shared();
  size_t isize = file_size();
  /*
  INFO("Begin to write, %d, %d", isize, offset);
  if (offset > isize) {
    inode_cache_->unlock_shared();
    return 0;
  }*/
  // It seems that offset can > isize
  int _err_ret = _upd_cache();
  if (_err_ret) {
    inode_cache_->unlock_shared();
    INFO("write: error: %d", _err_ret);
    return _err_ret;
  }
  if (append_flag) offset = isize;
  INFO("Begin to write, now block: %u(%u), write offset: %llu\n", block_id_, block_id_in_file_, offset);
  if (offset + size > isize) {
    // Now we need to modify the inode.
    inode_cache_->unlock_shared();
    std::unique_lock<std::shared_mutex> inode_lck(inode_cache_->inode_rwlock_);
    isize = file_size();
    if (append_flag) offset = isize;

    Block* blk;
    while (inode_cache_->cache_->i_size + BLOCK_SIZE - inode_cache_->cache_->i_size % BLOCK_SIZE < offset) {
      uint32_t index;
      if (!fs->alloc_block(&blk, &index, inode_cache_->cache_)) return 0;
      inode_cache_->cache_->i_size += BLOCK_SIZE - inode_cache_->cache_->i_size % BLOCK_SIZE;
    }
    if (inode_cache_->cache_->i_size % BLOCK_SIZE == 0) {
      uint32_t index;
      if (!fs->alloc_block(&blk, &index, inode_cache_->cache_)) return 0;
    }
    _err_ret = seek(offset / BLOCK_SIZE);
    if (_err_ret) return _err_ret;
    INFO("write: seek success");

    // write is dirty
    // since get_block...memcpy(blk->get()) is not atomic (but we can assume this when the number of threads is small, and cache is big although),
    size_t ret = 0;
    size_t csz = std::min(size, BLOCK_SIZE - (size_t)offset % BLOCK_SIZE);
    if (!fs->get_block(block_id_, &blk, true, offset % BLOCK_SIZE, buf + ret, csz)) return -EINVAL;
    INFO("write read first block");
    ret += csz, size -= csz, offset += csz, inode_cache_->cache_->i_size = std::max((size_t)inode_cache_->cache_->i_size, (size_t)offset);

    while (size) {
      csz = std::min(size, (size_t)BLOCK_SIZE);
      if (offset >= inode_cache_->cache_->i_size) {
        INFO("write: need allocation");
        uint32_t _;
        if (!fs->alloc_block(&blk, &_, inode_cache_->cache_)) return ret;
        if (next_block()) {
          WARNING("write: EIO");
          return -EIO;
        }
        if (!fs->get_block(block_id_, &blk, true, 0, buf + ret, csz)) {
          WARNING("write: EIO");
          return -EIO;
        }
      } else {
        INFO("write: don't need allocation");
        if (next_block()) {
          WARNING("write: EIO");
          return -EIO;
        }
        if (!fs->get_block(block_id_, &blk, true, 0, buf + ret, csz)) {
          WARNING("write: EIO");
          return -EIO;
        }
      }
      // memcpy(blk->get(), buf + ret, csz);
      INFO("write read blocks");
      ret += csz, size -= csz, offset += csz, inode_cache_->cache_->i_size = std::max((size_t)inode_cache_->cache_->i_size, (size_t)offset);
    }

    INFO("write: upd_All");
    inode_cache_->upd_all();
    INFO("write: end");

    return ret;

  } else {
    INFO("write: overlap");
    _err_ret = seek(offset / BLOCK_SIZE);
    if (_err_ret) {
      inode_cache_->unlock_shared();
      return _err_ret;
    }
    Block* blk;
    size_t ret = 0;
    size_t csz = std::min(size, BLOCK_SIZE - (size_t)offset % BLOCK_SIZE);
    if (!fs->get_block(block_id_, &blk, true, offset % BLOCK_SIZE, buf + ret, csz)) {
      inode_cache_->unlock_shared();
      return -EINVAL;
    }
    // memcpy(blk->get() + offset % BLOCK_SIZE, buf + ret, csz);
    ret += csz, size -= csz, offset += csz;
    while (size) {
      csz = std::min(size, (size_t)BLOCK_SIZE);
      if (offset >= isize) {
        inode_cache_->unlock_shared();
        return ret;
      }
      _err_ret = next_block();
      if (_err_ret) {
        inode_cache_->unlock_shared();
        return _err_ret;
      }
      if (!fs->get_block(block_id_, &blk, true, 0, buf + ret, csz)) {
        inode_cache_->unlock_shared();
        return -EINVAL;
      }
      // memcpy(blk->get());
      ret += csz, size -= csz, offset += csz;
    }
    inode_cache_->unlock_shared();
    return ret;
  }
  return 0;
}
void InodeCache::upd_all() {
  vec.iter([](FileStatus*& ptr) { ptr->cache_update_flag_ = true; });
  // in critical section
  // for (const auto& a : vec) a->cache_update_flag_ = true;
}

FileStatus* _fuse_trans_info(struct fuse_file_info* fi) { return reinterpret_cast<FileStatus*>(fi->fh); }

bool _check_permission(mode_t mode, int read, int write, int exec, gid_t gid, uid_t uid) {
  auto current_user = fuse_get_context();
  if (current_user->uid == 0) return true;  // super user
  if (current_user->gid == gid) {
    bool flag = true;
    flag &= (mode & S_IRGRP) || !read;
    flag &= (mode & S_IWGRP) || !write;
    flag &= (mode & S_IXGRP) || !exec;
    if (flag) return true;
  }
  if (current_user->uid == uid) {
    bool flag = true;
    flag &= (mode & S_IRUSR) || !read;
    flag &= (mode & S_IWUSR) || !write;
    flag &= (mode & S_IXUSR) || !exec;
    if (flag) return true;
  }
  bool flag = true;
  flag &= (mode & S_IROTH) || !read;
  flag &= (mode & S_IWOTH) || !write;
  flag &= (mode & S_IXOTH) || !exec;
  return flag;
}

bool _check_user(uid_t mode, uid_t uid, int read, int write, int exec) {
  auto current_user = fuse_get_context();
  // owner or super
  if (current_user->uid == uid) {
    bool flag = true;
    flag &= (mode & S_IRUSR) || !read;
    flag &= (mode & S_IWUSR) || !write;
    flag &= (mode & S_IXUSR) || !exec;
    if (flag) return true;
  }
  if (current_user->uid == 0) return true;
  return false;
}

}  // namespace naivefs