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
 *  Defined i_mode values
 */
#define EXT2_S_IFSOCK 0xC000 /* socket */
#define EXT2_S_IFLINK 0xA000 /* synbolic link */
#define EXT2_S_IFREG 0x8000  /* regular file */
#define EXT2_S_IFBLK 0x6000  /* block device */
#define EXT2_S_IFDIR 0x4000  /* directory */
#define EXT2_S_IFCHR 0x2000  /* character device */
#define EXT2_S_IFIFO 0x1000  /* fifo */
#define EXT2_S_ISUID 0x0800  /* set process user id */
#define EXT2_S_ISGID 0x0400  /* set process group id */
#define EXT2_S_ISVTX 0x0200  /* sticky bit */
#define EXT2_S_IRUSR 0x0100  /* user read */
#define EXT2_S_IWUSR 0x0080  /* user write */
#define EXT2_S_IXUSR 0x0040  /* user execute */
#define EXT2_S_IRGRP 0x0020  /* group read */
#define EXT2_S_IWGRP 0x0010  /* group write */
#define EXT2_S_IXGRP 0x0008  /* group execute */
#define EXT2_S_IROTH 0x0004  /* others read */
#define EXT2_S_IWOTH 0x0002  /* others write */
#define EXT2_S_IXOTH 0x0001  /* others execute */

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
