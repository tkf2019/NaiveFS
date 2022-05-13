#include "operation.h"
namespace naivefs {
int FileStatus::next_block() {
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
    uint32_t first_id = block_id_ - IBLOCK_11 - 1;
    indirect_block_[0] = IndirectBlockPtr(inode_cache_->cache_->i_block[12]);
    if (!indirect_block_[0].seek(first_id, block_id_)) return -EINVAL;
  } else if (block_id_in_file_ <= IBLOCK_13) {
    // The double indirect block
    uint32_t first_id = (block_id_ - IBLOCK_12 - 1) / (BLOCK_SIZE / 4);
    uint32_t second_id = (block_id_ - IBLOCK_12 - 1) % (BLOCK_SIZE / 4);
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
  if (!fs->get_block(block_id_, &blk)) return -EINVAL;
  size_t ret = 0;
  size_t csz = std::min(size, BLOCK_SIZE - (size_t)offset % BLOCK_SIZE);
  memcpy(buf, blk->get() + offset % BLOCK_SIZE, csz);
  ret += csz, size -= csz, offset += csz;
  while (size) {
    if (offset >= isize) return ret;
    _err_ret = next_block();
    if (_err_ret) return _err_ret;
    if (!fs->get_block(block_id_, &blk)) return -EINVAL;
    csz = std::min(size, std::min((size_t)isize - (size_t)offset, (size_t)BLOCK_SIZE));
    memcpy(buf, blk->get(), csz);
    ret += csz, size -= csz, offset += csz;
  }
  return ret;
}

int FileStatus::write(char* buf, size_t offset, size_t size, bool append_flag) {
  std::unique_lock<std::shared_mutex> lck(rwlock);
  inode_cache_->lock_shared();
  size_t isize = file_size();
  if (offset >= isize) {
    inode_cache_->unlock_shared();
    return 0;
  }
  int _err_ret = _upd_cache();
  if (_err_ret) {
    inode_cache_->unlock_shared();
    return _err_ret;
  }
  if (append_flag) offset = inode_cache_->cache_->i_size;
  _err_ret = seek(offset / BLOCK_SIZE);
  if (_err_ret) {
    inode_cache_->unlock_shared();
    return _err_ret;
  }
  if (offset + size > isize) {
    // Now we need to modify the inode.
    inode_cache_->unlock_shared();
    std::unique_lock<std::shared_mutex> inode_lck(inode_cache_->inode_rwlock_);
    isize = file_size();
    if (offset >= isize) return 0;
    _err_ret = seek(offset / BLOCK_SIZE);
    if (_err_ret) return _err_ret;

    Block* blk;
    if (!fs->get_block(block_id_, &blk)) return -EINVAL;
    size_t ret = 0;
    size_t csz = std::min(size, BLOCK_SIZE - (size_t)offset % BLOCK_SIZE);
    memcpy(blk->get() + offset % BLOCK_SIZE, buf, csz);
    ret += csz, size -= csz, offset += csz, inode_cache_->cache_->i_size += csz;

    while (size) {
      if (offset >= isize) {
        uint32_t index;
        if (!fs->alloc_block(&blk, &index, inode_cache_->cache_)) return ret;
      } else {
        next_block();
        if (!fs->get_block(block_id_, &blk)) return -EINVAL;
      }
      csz = std::min(size, (size_t)BLOCK_SIZE);
      memcpy(blk->get(), buf, csz);
      ret += csz, size -= csz, offset += csz, inode_cache_->cache_->i_size += csz;
    }

    inode_cache_->upd_all();

    return ret;

  } else {
    Block* blk;
    if (!fs->get_block(block_id_, &blk)) return -EINVAL;
    size_t ret = 0;
    size_t csz = std::min(size, BLOCK_SIZE - (size_t)offset % BLOCK_SIZE);
    memcpy(blk->get() + offset % BLOCK_SIZE, buf, csz);
    ret += csz, size -= csz, offset += csz;
    while (size) {
      if (offset >= isize) return ret;
      _err_ret = next_block();
      if (_err_ret) {
        inode_cache_->unlock_shared();
        return _err_ret;
      }
      if (!fs->get_block(block_id_, &blk)) return -EINVAL;
      csz = std::min(size, (size_t)BLOCK_SIZE);
      memcpy(blk->get(), buf, csz);
      ret += csz, size -= csz, offset += csz;
    }
    inode_cache_->unlock_shared();
    return ret;
  }
  return 0;
}
void InodeCache::upd_all() {
  // in critical section
  for (const auto& a : vec) a->cache_update_flag_ = true;
}

FileStatus* _fuse_trans_info(struct fuse_file_info* fi) { return reinterpret_cast<FileStatus*>(fi->fh); }


}  // namespace naivefs