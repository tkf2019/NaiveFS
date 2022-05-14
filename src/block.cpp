#include "block.h"

namespace naivefs {

void SuperBlock::init_super_block() {
  switch (super_->s_state) {
    case FSState::UNINIT: {
      DEBUG("Find unintialized file system");

      super_->s_log_block_size = LOG_BLOCK_SIZE;
      super_->s_blocks_per_group = TOTAL_BLOCKS_PER_GROUP;
      super_->s_inodes_per_group = INODES_PER_GROUP;
      // 1 inode bitmap, 1 block bitmap, 1 inode table block, no data blocks
      super_->s_blocks_count = 3;
      super_->s_inodes_count = 1;  // 1 root inode
      super_->s_first_ino = ROOT_INODE;
      super_->s_inode_size = sizeof(ext2_inode);
      // set state to normal
      super_->s_state = FSState::NORMAL;

      // init first block group
      ext2_group_desc* desc =
          (ext2_group_desc*)(data_ + sizeof(ext2_super_block));
      desc->bg_inode_bitmap = BLOCK_SIZE;
      desc->bg_block_bitmap = desc->bg_inode_bitmap + BLOCK_SIZE;
      desc->bg_inode_table = desc->bg_block_bitmap + BLOCK_SIZE;
      desc->bg_free_blocks_count = BLOCKS_PER_GROUP;
      // root inode has been allocated
      desc->bg_free_inodes_count = INODES_PER_GROUP - 1;
      desc->bg_used_dirs_count = 0;
      desc_table_.push_back(desc);

      // Why flush needed?
      flush();
      break;
    }
    case FSState::NORMAL: {
      // Super block info
      INFO("BLOCK SIZE: %i", block_size());
      INFO("BLOCK GROUP SIZE: %i", block_group_size());
      INFO("N BlOCK GROUPS: %i", num_block_groups());
      INFO("INODE SIZE: %i", inode_size());
      INFO("INODES PER GROUP: %i", inodes_per_group());

      ext2_group_desc* ptr =
          (ext2_group_desc*)(data_ + sizeof(ext2_super_block));
      for (uint32_t i = 0; i < num_block_groups(); ++i) {
        desc_table_.push_back(ptr++);
      }
      break;
    }
    default: {
      ERR("Unknown file system state: %i", super_->s_state);
      abort();
    }
  }
}

int64_t BitmapBlock::alloc_new() {
  int64_t i = bitmap_.find(BLOCK_SIZE * sizeof(uint8_t));
  if (i < 0) {
    ERR("Failed to alloc new item");
    return i;
  }
  bitmap_.set(i);
  return i;
}

BlockGroup::BlockGroup(ext2_group_desc* desc) : desc_(desc) {
  ASSERT(desc != nullptr);

  INFO("BLOCK BITMAP OFFSET: 0x%x", desc->bg_block_bitmap);
  INFO("INODE BITMAP OFFSET: 0x%x", desc->bg_inode_bitmap);
  INFO("INODE TABLE OFFSET: 0x%x", desc->bg_inode_table);
  INFO("FREE BLOCKS COUNT: %i", desc->bg_free_blocks_count);
  INFO("FREE INODES COUNT: %i", desc->bg_free_inodes_count);

  block_bitmap_ = new BitmapBlock(desc->bg_block_bitmap);
  inode_bitmap_ = new BitmapBlock(desc->bg_inode_bitmap);
}

BlockGroup::~BlockGroup() {
  delete block_bitmap_;
  delete inode_bitmap_;
  for (auto item : inode_table_) {
    delete item.second;
  }
}

void BlockGroup::flush() {
  ASSERT(block_bitmap_ != nullptr);
  block_bitmap_->flush();
  ASSERT(inode_bitmap_ != nullptr);
  inode_bitmap_->flush();
  for (auto item : inode_table_) {
    ASSERT(item.second != nullptr);
    item.second->flush();
  }
}

bool BlockGroup::get_inode(uint32_t index, ext2_inode** inode) {
  // invalid inode
  if (!inode_bitmap_->test(index)) {
    WARNING("Inode has not been allocated in the bitmap!");
    return false;
  }
  uint32_t block_index = index / INODES_PER_BLOCK;
  uint32_t block_inner_index = index % INODES_PER_BLOCK;

  // lazy read
  if (inode_table_.find(block_index) == inode_table_.end()) {
    inode_table_[block_index] =
        new InodeTableBlock(inode_block_offset(block_index));
  }
  *inode = inode_table_[block_index]->get(block_inner_index);
  return true;
}

bool BlockGroup::get_block(uint32_t index, Block** block) {
  // invalid block
  if (!block_bitmap_->test(index)) {
    WARNING("Block has not been allocated in the bitmap!");
    return false;
  }
  *block = new Block(data_block_offset(index));
  return true;
}

bool BlockGroup::alloc_inode(ext2_inode** inode, uint32_t* index, bool dir) {
  int ret = inode_bitmap_->alloc_new();
  if (ret == -1) return false;

  // update block group descriptor
  desc_->bg_free_inodes_count--;
  if (dir) desc_->bg_used_dirs_count++;

  if (index != nullptr) *index = ret;
  return get_inode(ret, inode);
}

bool BlockGroup::alloc_block(Block** block, uint32_t* index) {
  int ret = block_bitmap_->alloc_new();
  if (ret == -1) return false;

  // update block group descriptor
  desc_->bg_free_blocks_count--;

  if (index != nullptr) *index = ret;
  *block = new Block(data_block_offset(ret), true);
  return true;
}

off_t BlockGroup::inode_block_offset(uint32_t inode_block_index) {
  return block_bitmap_->offset() + (inode_block_index + 1) * BLOCK_SIZE;
}

off_t BlockGroup::data_block_offset(uint32_t data_block_index) {
  return block_bitmap_->offset() + NUM_INODE_TABLE_BLOCKS * BLOCK_SIZE +
         data_block_index * BLOCK_SIZE;
}
}  // namespace naivefs