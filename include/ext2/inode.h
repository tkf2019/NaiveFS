#ifndef EXT2_INODE_H
#define EXT2_INODE_H

#include "basic.h"

/*
 * Constants relative to the data blocks
 */
#define EXT2_NDIR_BLOCKS 12
#define EXT2_IND_BLOCK EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK (EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK (EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS (EXT2_TIND_BLOCK + 1)

/*
 * Structure of an inode on the disk
 */
struct ext2_inode {
  __le16 i_mode;        /* File mode */
  __le16 i_uid;         /* Low 16 bits of Owner Uid */
  __le32 i_size;        /* Size in bytes */
  __le32 i_atime;       /* Access time */
  __le32 i_ctime;       /* Creation time */
  __le32 i_mtime;       /* Modification time */
  __le32 i_dtime;       /* Deletion Time */
  __le16 i_gid;         /* Low 16 bits of Group Id */
  __le16 i_links_count; /* Links count */
  __le32 i_blocks;      /* Blocks count */
  __le32 i_flags;       /* File flags */
  union {
    struct {
      __le32 l_i_reserved1;
    } linux1;
    struct {
      __le32 h_i_translator;
    } hurd1;
    struct {
      __le32 m_i_reserved1;
    } masix1;
  } osd1;                        /* OS dependent 1 */
  __le32 i_block[EXT2_N_BLOCKS]; /* Pointers to blocks */
  __le32 i_generation;           /* File version (for NFS) */
  __le32 i_file_acl;             /* File ACL */
  __le32 i_dir_acl;              /* Directory ACL */
  __le32 i_faddr;                /* Fragment address */
  union {
    struct {
      __u8 l_i_frag;  /* Fragment number */
      __u8 l_i_fsize; /* Fragment size */
      __u16 i_pad1;
      __le16 l_i_uid_high; /* these 2 fields    */
      __le16 l_i_gid_high; /* were reserved2[0] */
      __u32 l_i_reserved2;
    } linux2;
    struct {
      __u8 h_i_frag;  /* Fragment number */
      __u8 h_i_fsize; /* Fragment size */
      __le16 h_i_mode_high;
      __le16 h_i_uid_high;
      __le16 h_i_gid_high;
      __le32 h_i_author;
    } hurd2;
    struct {
      __u8 m_i_frag;  /* Fragment number */
      __u8 m_i_fsize; /* Fragment size */
      __u16 m_pad1;
      __u32 m_i_reserved2[2];
    } masix2;
  } osd2; /* OS dependent 2 */
};

#endif
