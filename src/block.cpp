#include "block.h"

namespace naivefs {

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
  return block_bitmap_->offset() + NUM_INODE_BLOCKS * BLOCK_SIZE +
         data_block_index * BLOCK_SIZE;
}
}  // namespace naivefs