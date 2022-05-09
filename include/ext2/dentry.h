#ifndef EXT2_DENTRY_H
#define EXT2_DENTRY_H

#include "basic.h"

#define EXT2_NAME_LEN 255

/*
 * The new version of the directory entry.  Since EXT2 structures are
 * stored in intel byte order, and the name_len field could never be
 * bigger than 255 chars, it's safe to reclaim the extra byte for the
 * file_type field.
 */
struct ext2_dir_entry_2 {
  __le32 inode;   /* Inode number */
  __le16 rec_len; /* Directory entry length */
  __u8 name_len;  /* Name length */
  __u8 file_type;
  char name[EXT2_NAME_LEN]; /* File name, up to EXT2_NAME_LEN */
};

#endif
