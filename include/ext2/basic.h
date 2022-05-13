#ifndef EXT2_BASIC_H
#define EXT2_BASIC_H

#include <stdint.h>

#define __le64 uint64_t
#define __le32 uint32_t
#define __le16 uint16_t
#define __u64 uint64_t
#define __u32 uint32_t
#define __u16 uint16_t
#define __u8 uint8_t

/*
 * Mount flags
 */
#define EXT2_MOUNT_OLDALLOC 0x000002 /* Don't use the new Orlov allocator */
#define EXT2_MOUNT_GRPID 0x000004    /* Create files with directory's group */
#define EXT2_MOUNT_DEBUG 0x000008    /* Some debugging messages */
#define EXT2_MOUNT_ERRORS_CONT 0x000010  /* Continue on errors */
#define EXT2_MOUNT_ERRORS_RO 0x000020    /* Remount fs ro on errors */
#define EXT2_MOUNT_ERRORS_PANIC 0x000040 /* Panic on errors */
#define EXT2_MOUNT_MINIX_DF 0x000080     /* Mimics the Minix statfs */
#define EXT2_MOUNT_NOBH 0x000100         /* No buffer_heads */
#define EXT2_MOUNT_NO_UID32 0x000200     /* Disable 32-bit UIDs */
#define EXT2_MOUNT_XATTR_USER 0x004000   /* Extended user attributes */
#define EXT2_MOUNT_POSIX_ACL 0x008000    /* POSIX Access Control Lists */
#define EXT2_MOUNT_XIP 0x010000          /* Obsolete, use DAX */
#define EXT2_MOUNT_USRQUOTA 0x020000     /* user quota */
#define EXT2_MOUNT_GRPQUOTA 0x040000     /* group quota */
#define EXT2_MOUNT_RESERVATION 0x080000  /* Preallocation */
#define EXT2_MOUNT_DAX 0x100000          /* Direct Access */

#endif
