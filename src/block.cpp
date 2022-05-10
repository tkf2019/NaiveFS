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
      break;
    }
  }
}

BlockGroup::BlockGroup(ext2_group_desc* desc) {
  ASSERT(desc != nullptr);
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

bool BlockGroup::get_inode(uint32_t index, ext2_inode** inode) {
  // invalid inode
  if (!inode_bitmap_->test(index)) return false;

  uint32_t block_index = index / INODE_PER_BLOCK;
  uint32_t block_inner_index = index % INODE_PER_BLOCK;

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
  if (!block_bitmap_->test(index)) return false;

  *block = new Block(data_block_offset(index));
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