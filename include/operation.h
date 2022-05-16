#ifndef NAIVEFS_INCLUDE_FUSEOP_H_
#define NAIVEFS_INCLUDE_FUSEOP_H_

#define FUSE_USE_VERSION 31
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <mutex>
#include <shared_mutex>

#include "ext2/inode.h"
#include "filesystem.h"
#include "utils/logging.h"
#include "utils/option.h"

namespace naivefs {

extern FileSystem *fs;

/**
 * @brief FileStatus: The status of the file handle
 *
 * Recall: i_block[0...11] is direct map
 * https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout i_block[12] is
 * indirect block, i.e. file blocks 12...BLOCK_SIZE / 4 + 11 i_block[13] is
 * double-indirect block, i.e. file blocks BLOCK_SIZE / 4 + 12...(BLOCK_SIZE /
 * 4) ^ 2 + (BLOCK_SIZE / 4) + 11 i_block[14] is trible-indirect block, i.e.
 * file blocks (BLOCK_SIZE / 4) ^ 2 + (BLOCK_SIZE / 4) + 12...(BLOCK_SIZE / 4) ^
 * 3 + (BLOCK_SIZE / 4) ^ 2 + (BLOCK_SIZE / 4) + 12
 *
 * inode is 4B.
 */

constexpr uint32_t IBLOCK_11 = 11;
constexpr uint32_t IBLOCK_12 = BLOCK_SIZE / 4 + 11;
constexpr uint32_t IBLOCK_13 = (BLOCK_SIZE / 4) * (BLOCK_SIZE / 4) + IBLOCK_12;
constexpr uint32_t IBLOCK_14 = (BLOCK_SIZE / 4) * (BLOCK_SIZE / 4) * (BLOCK_SIZE / 4) + IBLOCK_13;  // 1074791436

template <typename T>
class FSListPtr {
 public:
  T value_;
  FSListPtr<T> *prev_;
  FSListPtr<T> *next_;
  FSListPtr() : value_() { prev_ = next_ = nullptr; }
  FSListPtr(const T &value) : value_(value) { prev_ = next_ = nullptr; }
};

// not thread-safe
template <typename T>
class FSList {
 public:
  FSListPtr<T> *head_;
  FSList() { head_ = new FSListPtr<T>(); }
  ~FSList() {
    for (auto nxt = head_->next_;; nxt = nxt->next_) {
      delete head_;
      head_ = nxt;
      if (nxt == nullptr) break;
    }
  }
  FSListPtr<T> *ins(const T &value) {
    auto nw = new FSListPtr<T>(value);
    nw->prev_ = head_;
    nw->next_ = head_->next_;
    head_->next_ = nw;
    if(nw->next_) nw->next_->prev_ = nw;
    return nw;
  }
  void iter(const std::function<void(T &)>& func) {
    for (auto nxt = head_->next_; nxt; nxt = nxt->next_) func(nxt->value_);
  }
  void del(FSListPtr<T> *ptr) {
    if (ptr->next_) ptr->next_->prev_ = ptr->prev_;
    ptr->prev_->next_ = ptr->next_;
    delete ptr;
  }
  int empty() {
    return head_->next == nullptr;
  }
};

class FileStatus;
class InodeCache {
 public:
  uint32_t inode_id_;               // this inode
  uint32_t cnts_;
  ext2_inode cache_[1];             // cache of the inode
  std::shared_mutex inode_rwlock_;  // if a file is opened by many processes, we
                                    // use this to ensure atomicity.
  FSList<FileStatus*> vec;                       // when inode cache is changed in a critical section, other process
                                    // must update their cache.
  explicit InodeCache(uint32_t inode_id) : inode_id_(inode_id) { cnts_ = 0; }
  ~InodeCache() {}
  void lock_shared() { inode_rwlock_.lock_shared(); }
  void unlock_shared() { inode_rwlock_.unlock_shared(); }
  void lock() { inode_rwlock_.lock(); }
  void unlock() { inode_rwlock_.unlock(); }
  int copy() {
    ext2_inode *inode;
    if (!fs->get_inode(inode_id_, &inode)) return -EIO;
    memcpy(cache_, inode, sizeof(ext2_inode));
    return 0;
  }
  bool init() { return copy() == 0; }
  void upd_all();
  void del(FSListPtr<FileStatus*>* ptr) {vec.del(ptr);}
  FSListPtr<FileStatus*>* ins(FileStatus* ptr) {return vec.ins(ptr);}
  int commit() {
    ext2_inode *inode;
    INFO("inode cache commit %d", inode_id_);
    if (!fs->get_inode(inode_id_, &inode)) return -EIO;
    memcpy(inode, cache_, sizeof(ext2_inode));
    return 0;
  }
};

class FileStatus {
 public:
  class IndirectBlockPtr {
   public:
    uint32_t id_;
    IndirectBlockPtr() { id_ = 0; }
    IndirectBlockPtr(uint32_t indirect_block_id) : id_(indirect_block_id) {}
    bool seek(off_t off, uint32_t &block_id) {
      Block *blk;
      if (!fs->get_block(id_, &blk)) return false;
      block_id = (reinterpret_cast<uint32_t *>(blk->get()))[off];
      return true;
    }
  };
  InodeCache *inode_cache_;
  FSListPtr<FileStatus*>* fslist_ptr_;
  bool cache_update_flag_;
  uint32_t block_id_;                   // current block
  uint32_t block_id_in_file_;           // i.e. current offset / BLOCK_SIZE
  IndirectBlockPtr indirect_block_[3];  // if indirect_blocks are using, we record each level
  std::shared_mutex rwlock;             // lock the FileStatus itself.
  FileStatus() {
    inode_cache_ = nullptr;
    cache_update_flag_ = false;
    block_id_ = 0;
    block_id_in_file_ = 0;
    memset(indirect_block_, 0, sizeof(indirect_block_));
  }
  ~FileStatus() {
    INFO("~FileStatus");
    ASSERT(fslist_ptr_ != nullptr);
    inode_cache_->lock();
    inode_cache_->del(fslist_ptr_);
    inode_cache_->unlock();
  }
  bool is_one_block(off_t off) { return off / BLOCK_SIZE == block_id_in_file_; }
  bool check_size(off_t off, size_t counts) { return counts + off > (size_t)inode_cache_->cache_->i_size; }
  size_t file_size() { return inode_cache_->cache_->i_size; }

  /**
   * @brief next_block: get the next block of block_id_in_file_, and
   * block_id_in_file_ += 1
   * @brief seek: given a new value of block_id_in_file_, seek block_id_ using
   * old values indirect_block_
   * @brief bf_seek: seek, but don't use any old values
   * They are not atomic.
   *
   *
   * @return int, 0 if success, else a negative integer
   */

  int next_block();
  int seek(uint32_t new_block_id_in_file);
  int bf_seek(uint32_t new_block_id_in_file);

  /**
   * @brief update indirect_block_ by bf_seek
   *
   * @return int
   */

  int _upd_cache() {
    if (cache_update_flag_) {
      cache_update_flag_ = false;
      return bf_seek(block_id_in_file_);
    }
    return 0;
  }

  /**
   * @brief init the file pointers
   *
   */

  void init_seek() { cache_update_flag_ = true; }

  /**
   * @brief copy_to_buf copy the file to the buf. this function works under
   * writer lock, because it changes file pointer. It works under reader lock of
   * inode_rwlock, because it only reads inode data.
   *
   * @param buf
   * @param offset
   * @param size
   * @return int the number of bytes
   *
   *
   */
  int copy_to_buf(char *buf, size_t offset, size_t size);

  /**
   * @brief write buf to the file. the function works under rwlock because it
   * changes file pointers, and we also lock inode_rwlock, because it changes
   * file metadata. We don't ensure that reading, writing a file by processes
   * will occur in an expected order. We don't ensure that RW are not performed
   * at one data block at the same time. But we ensure that operations on inode
   * metadata are serializable. We ensure that operations on FileStatus itself
   * are serializable.
   *
   * if append_flag is true then offset is set to the end of the file at
   * beginning.
   *
   * @param buf
   * @param offset
   * @param size
   * @return int
   */
  int write(const char *buf, size_t offset, size_t size, bool append_flag = false);

  int append(const char *buf, size_t offset, size_t size) { return write(buf, offset, size, true); }
};

class OpManager {
 public:
  /**
   * @brief Get the cache object by inode_id
   *
   * @param inode_id
   * @return InodeCache*
   */
  OpManager() {}
  ~OpManager() {
    for (auto &[_, ptr] : st_) delete ptr;
  }
  InodeCache *get_cache(uint32_t inode_id) {
    std::unique_lock<std::shared_mutex> lck(m_);
    INFO("OpManager get_cache inode_id: %d", inode_id);
    if (!st_.count(inode_id)) {
      auto ic = new InodeCache(inode_id);
      INFO("OpManager create new cache");
      if (!ic->init()) {
        delete ic;
        return nullptr;
      }
      st_[inode_id] = ic;
    }
    auto ret = st_[inode_id];
    ret->cnts_++;
    return ret;
  }
  /**
   * @brief update the list in the cache object
   *
   * @param fd
   * @param inode_id
   */
  void upd_cache(FileStatus *fd, uint32_t inode_id) {
    std::unique_lock<std::shared_mutex> lck(m_);
    auto it = st_.find(inode_id);
    if (it == st_.end()) return;
    std::unique_lock<std::shared_mutex> lck_ic(it->second->inode_rwlock_);
    fd->fslist_ptr_ = it->second->vec.ins(fd);
  }
  /**
   * @brief try to release an InodeCache object
   * 
   * @param inode_id 
   * @return int 
   */
  int rel_cache(uint32_t inode_id) {
    std::unique_lock<std::shared_mutex> lck(m_);
    auto it = st_.find(inode_id);
    int ret = 0;
    if (it == st_.end()) return 0;
    it->second->lock();
    if(!--it->second->cnts_) {
      INFO("rel cache success");
      ret = it->second->commit();
      it->second->unlock();
      delete it->second;
      st_.erase(it);
    } else {
      INFO("rel cache: cache cnts %d", it->second->cnts_);
      it->second->unlock();
    }
    INFO("rel cache returns %d", ret);
    return ret;
  }

 private:
  std::map<uint32_t, InodeCache *> st_;
  std::shared_mutex m_;
};

extern OpManager *opm;
FileStatus *_fuse_trans_info(struct fuse_file_info *fi);
bool _check_permission(mode_t mode, int read, int write, int exec, gid_t gid, uid_t uid);
bool _check_user(uid_t mode, uid_t uid, int read, int write, int exec);
/**
 * The file system operations:
 *
 * Most of these should work very similarly to the well known UNIX
 * file system operations.  A major exception is that instead of
 * returning an error in 'errno', the operation should return the
 * negated error value (-errno) directly.
 *
 * All methods are optional, but some are essential for a useful
 * filesystem (e.g. getattr).  Open, flush, release, fsync, opendir,
 * releasedir, fsyncdir, access, create, truncate, lock, init and
 * destroy are special purpose methods, without which a full featured
 * filesystem can still be implemented.
 *
 * In general, all methods are expected to perform any necessary
 * permission checking. However, a filesystem may delegate this task
 * to the kernel by passing the `default_permissions` mount option to
 * `fuse_new()`. In this case, methods will only be called if
 * the kernel's permission check has succeeded.
 *
 * Almost all operations take a path which can be of any length.
 */

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored. The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given. In that case it is passed to userspace,
 * but libfuse and the kernel will still assign a different
 * inode for internal use (called the "nodeid").
 *
 * `fi` will always be NULL if the file is not currently open, but
 * may also be NULL if the file is open.
 */
int fuse_getattr(const char *, struct stat *, struct fuse_file_info *fi);

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.	If the linkname is too long to fit in the
 * buffer, it should be truncated.	The return value should be 0
 * for success.
 */
int fuse_readlink(const char *, char *, size_t);

/** Create a file node
 *
 * This is called for creation of all non-directory, non-symlink
 * nodes.  If the filesystem defines a create() method, then for
 * regular files that will be called instead.
 */
int fuse_mknod(const char *, mode_t, dev_t);

/** Create a directory
 *
 * Note that the mode argument may not have the type specification
 * bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
 * correct directory type bits use  mode|S_IFDIR
 * */
int fuse_mkdir(const char *, mode_t);

/** Remove a file */
int fuse_unlink(const char *);

/** Remove a directory */
int fuse_rmdir(const char *);

/** Create a symbolic link */
int fuse_symlink(const char *, const char *);

/** Rename a file
 *
 * *flags* may be `RENAME_EXCHANGE` or `RENAME_NOREPLACE`. If
 * RENAME_NOREPLACE is specified, the filesystem must not
 * overwrite *newname* if it exists and return an error
 * instead. If `RENAME_EXCHANGE` is specified, the filesystem
 * must atomically exchange the two files, i.e. both must
 * exist and neither may be deleted.
 */
int fuse_rename(const char *, const char *, unsigned int flags);

/** Create a hard link to a file */
int fuse_link(const char *, const char *);

/** Change the permission bits of a file
 *
 * `fi` will always be NULL if the file is not currently open, but
 * may also be NULL if the file is open.
 */
int fuse_chmod(const char *, mode_t, struct fuse_file_info *fi);

/** Change the owner and group of a file
 *
 * `fi` will always be NULL if the file is not currently open, but
 * may also be NULL if the file is open.
 *
 * Unless FUSE_CAP_HANDLE_KILLPRIV is disabled, this method is
 * expected to reset the setuid and setgid bits.
 */
int fuse_chown(const char *, uid_t, gid_t, struct fuse_file_info *fi);

/** Change the size of a file
 *
 * `fi` will always be NULL if the file is not currently open, but
 * may also be NULL if the file is open.
 *
 * Unless FUSE_CAP_HANDLE_KILLPRIV is disabled, this method is
 * expected to reset the setuid and setgid bits.
 */
int fuse_truncate(const char *, off_t, struct fuse_file_info *fi);

/** Open a file
 *
 * Open flags are available in fi->flags. The following rules
 * apply.
 *
 *  - Creation (O_CREAT, O_EXCL, O_NOCTTY) flags will be
 *    filtered out / handled by the kernel.
 *
 *  - Access modes (O_RDONLY, O_WRONLY, O_RDWR, O_EXEC, O_SEARCH)
 *    should be used by the filesystem to check if the operation is
 *    permitted.  If the ``-o default_permissions`` mount option is
 *    given, this check is already done by the kernel before calling
 *    open() and may thus be omitted by the filesystem.
 *
 *  - When writeback caching is enabled, the kernel may send
 *    read requests even for files opened with O_WRONLY. The
 *    filesystem should be prepared to handle this.
 *
 *  - When writeback caching is disabled, the filesystem is
 *    expected to properly handle the O_APPEND flag and ensure
 *    that each write is appending to the end of the file.
 *
 *  - When writeback caching is enabled, the kernel will
 *    handle O_APPEND. However, unless all changes to the file
 *    come through the kernel this will not work reliably. The
 *    filesystem should thus either ignore the O_APPEND flag
 *    (and let the kernel handle it), or return an error
 *    (indicating that reliably O_APPEND is not available).
 *
 * Filesystem may store an arbitrary file handle (pointer,
 * index, etc) in fi->fh, and use this in other all other file
 * operations (read, write, flush, release, fsync).
 *
 * Filesystem may also implement stateless file I/O and not store
 * anything in fi->fh.
 *
 * There are also some flags (direct_io, keep_cache) which the
 * filesystem may set in fi, to change the way the file is opened.
 * See fuse_file_info structure in <fuse_common.h> for more details.
 *
 * If this request is answered with an error code of ENOSYS
 * and FUSE_CAP_NO_OPEN_SUPPORT is set in
 * `fuse_conn_info.capable`, this is treated as success and
 * future calls to open will also succeed without being send
 * to the filesystem process.
 *
 */
int fuse_open(const char *, struct fuse_file_info *);

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.	 An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 */
int fuse_read(const char *, char *, size_t, off_t, struct fuse_file_info *);

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.	 An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Unless FUSE_CAP_HANDLE_KILLPRIV is disabled, this method is
 * expected to reset the setuid and setgid bits.
 */
int fuse_write(const char *, const char *, size_t, off_t, struct fuse_file_info *);

/** Get file system statistics
 *
 * The 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 */
int fuse_statfs(const char *, struct statvfs *);

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor, as opposed to
 * release which is called on the close of the last file descriptor for
 * a file.  Under Linux, errors returned by flush() will be passed to
 * userspace as errors from close(), so flush() is a good place to write
 * back any cached dirty data. However, many applications ignore errors
 * on close(), and on non-Linux systems, close() may succeed even if flush()
 * returns an error. For these reasons, filesystems should not assume
 * that errors returned by flush will ever be noticed or even
 * delivered.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().  This happens if more than one file descriptor refers to an
 * open file handle, e.g. due to dup(), dup2() or fork() calls.  It is
 * not possible to determine if a flush is final, so each flush should
 * be treated equally.  Multiple write-flush sequences are relatively
 * rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will be called at any
 * particular point.  It may be called more times than expected, or not
 * at all.
 *
 * [close]:
 * http://pubs.opengroup.org/onlinepubs/9699919799/functions/close.html
 */
int fuse_flush(const char *, struct fuse_file_info *);

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file handle.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 */
int fuse_release(const char *, struct fuse_file_info *);

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 */
int fuse_fsync(const char *, int, struct fuse_file_info *);

/** Set extended attributes */
int fuse_setxattr(const char *, const char *, const char *, size_t, int);

/** Get extended attributes */
int fuse_getxattr(const char *, const char *, char *, size_t);

/** List extended attributes */
int fuse_listxattr(const char *, char *, size_t);

/** Remove extended attributes */
int fuse_removexattr(const char *, const char *);

/** Open directory
 *
 * Unless the 'default_permissions' mount option is given,
 * this method should check if opendir is permitted for this
 * directory. Optionally opendir may also return an arbitrary
 * filehandle in the fuse_file_info structure, which will be
 * passed to readdir, releasedir and fsyncdir.
 */
int fuse_opendir(const char *, struct fuse_file_info *);

/** Read directory
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * When FUSE_READDIR_PLUS is not set, only some parameters of the
 * fill function (the fuse_fill_dir_t parameter) are actually used:
 * The file type (which is part of stat::st_mode) is used. And if
 * fuse_config::use_ino is set, the inode (stat::st_ino) is also
 * used. The other fields are ignored when FUSE_READDIR_PLUS is not
 * set.
 */
int fuse_readdir(const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *, enum fuse_readdir_flags);

/** Release directory
 *
 * If the directory has been removed after the call to opendir, the
 * path parameter will be NULL.
 */
int fuse_releasedir(const char *, struct fuse_file_info *);

/** Synchronize directory contents
 *
 * If the directory has been removed after the call to opendir, the
 * path parameter will be NULL.
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 */
int fuse_fsyncdir(const char *, int, struct fuse_file_info *);

/**
 * Initialize filesystem
 *
 * The return value will passed in the `private_data` field of
 * `struct fuse_context` to all file operations, and as a
 * parameter to the destroy() method. It overrides the initial
 * value provided to fuse_main() / fuse_new().
 */
void *fuse_init(struct fuse_conn_info *conn, struct fuse_config *cfg);

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 */
void fuse_destroy(void *private_data);

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 */
int fuse_access(const char *, int);

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 */
int fuse_create(const char *, mode_t, struct fuse_file_info *);

/**
 * Perform POSIX file locking operation
 *
 * The cmd argument will be either F_GETLK, F_SETLK or F_SETLKW.
 *
 * For the meaning of fields in 'struct flock' see the man page
 * for fcntl(2).  The l_whence field will always be set to
 * SEEK_SET.
 *
 * For checking lock ownership, the 'fuse_file_info->owner'
 * argument must be used.
 *
 * For F_GETLK operation, the library will first check currently
 * held locks, and if a conflicting lock is found it will return
 * information without calling this method.	 This ensures, that
 * for local locks the l_pid field is correctly filled in.	The
 * results may not be accurate in case of race conditions and in
 * the presence of hard links, but it's unlikely that an
 * application would rely on accurate GETLK results in these
 * cases.  If a conflicting lock is not found, this method will be
 * called, and the filesystem may fill out l_pid by a meaningful
 * value, or it may leave this field zero.
 *
 * For F_SETLK and F_SETLKW the l_pid field will be set to the pid
 * of the process performing the locking operation.
 *
 * Note: if this method is not implemented, the kernel will still
 * allow file locking to work locally.  Hence it is only
 * interesting for network filesystems and similar.
 */
int fuse_lock(const char *, struct fuse_file_info *, int cmd, struct flock *);

/**
 * Change the access and modification times of a file with
 * nanosecond resolution
 *
 * This supersedes the old utime() interface.  New applications
 * should use this.
 *
 * `fi` will always be NULL if the file is not currently open, but
 * may also be NULL if the file is open.
 *
 * See the utimensat(2) man page for details.
 */
int fuse_utimens(const char *, const struct timespec tv[2], struct fuse_file_info *fi);

/**
 * Map block index within file to block index within device
 *
 * Note: This makes sense only for block device backed filesystems
 * mounted with the 'blkdev' option
 */
int fuse_bmap(const char *, size_t blocksize, uint64_t *idx);

#if FUSE_USE_VERSION < 35
int fuse_ioctl(const char *, int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data);
#else
/**
 * Ioctl
 *
 * flags will have FUSE_IOCTL_COMPAT set for 32bit ioctls in
 * 64bit environment.  The size and direction of data is
 * determined by _IOC_*() decoding of cmd.  For _IOC_NONE,
 * data will be NULL, for _IOC_WRITE data is out area, for
 * _IOC_READ in area and if both are set in/out area.  In all
 * non-NULL cases, the area is of _IOC_SIZE(cmd) bytes.
 *
 * If flags has FUSE_IOCTL_DIR then the fuse_file_info refers to a
 * directory file handle.
 *
 * Note : the unsigned long request submitted by the application
 * is truncated to 32 bits.
 */
int (*ioctl)(const char *, unsigned int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data);
#endif

/**
 * Poll for IO readiness events
 *
 * Note: If ph is non-NULL, the client should notify
 * when IO readiness events occur by calling
 * fuse_notify_poll() with the specified ph.
 *
 * Regardless of the number of times poll with a non-NULL ph
 * is received, single notification is enough to clear all.
 * Notifying more times incurs overhead but doesn't harm
 * correctness.
 *
 * The callee is responsible for destroying ph with
 * fuse_pollhandle_destroy() when no longer in use.
 */
int fuse_poll(const char *, struct fuse_file_info *, struct fuse_pollhandle *ph, unsigned *reventsp);

/** Write contents of buffer to an open file
 *
 * Similar to the write() method, but data is supplied in a
 * generic buffer.  Use fuse_buf_copy() to transfer data to
 * the destination.
 *
 * Unless FUSE_CAP_HANDLE_KILLPRIV is disabled, this method is
 * expected to reset the setuid and setgid bits.
 */
int fuse_write_buf(const char *, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *);

/** Store data from an open file in a buffer
 *
 * Similar to the read() method, but data is stored and
 * returned in a generic buffer.
 *
 * No actual copying of data has to take place, the source
 * file descriptor may simply be stored in the buffer for
 * later data transfer.
 *
 * The buffer must be allocated dynamically and stored at the
 * location pointed to by bufp.  If the buffer contains memory
 * regions, they too must be allocated using malloc().  The
 * allocated memory will be freed by the caller.
 */
int fuse_read_buf(const char *, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *);
/**
 * Perform BSD file locking operation
 *
 * The op argument will be either LOCK_SH, LOCK_EX or LOCK_UN
 *
 * Nonblocking requests will be indicated by ORing LOCK_NB to
 * the above operations
 *
 * For more information see the flock(2) manual page.
 *
 * Additionally fi->owner will be set to a value unique to
 * this open file.  This same value will be supplied to
 * ->release() when the file is released.
 *
 * Note: if this method is not implemented, the kernel will still
 * allow file locking to work locally.  Hence it is only
 * interesting for network filesystems and similar.
 */
int fuse_flock(const char *, struct fuse_file_info *, int op);

/**
 * Allocates space for an open file
 *
 * This function ensures that required space is allocated for specified
 * file.  If this function returns success then any subsequent write
 * request to specified range is guaranteed not to fail because of lack
 * of space on the file system media.
 */
int fuse_fallocate(const char *, int, off_t, off_t, struct fuse_file_info *);

/**
 * Copy a range of data from one file to another
 *
 * Performs an optimized copy between two file descriptors without the
 * additional cost of transferring data through the FUSE kernel module
 * to user space (glibc) and then back into the FUSE filesystem again.
 *
 * In case this method is not implemented, applications are expected to
 * fall back to a regular file copy.   (Some glibc versions did this
 * emulation automatically, but the emulation has been removed from all
 * glibc release branches.)
 */
ssize_t fuse_copy_file_range(const char *path_in, struct fuse_file_info *fi_in, off_t offset_in, const char *path_out, struct fuse_file_info *fi_out,
                             off_t offset_out, size_t size, int flags);

/**
 * Find next data or hole after the specified offset
 */
off_t fuse_lseek(const char *, off_t off, int whence, struct fuse_file_info *);

}  // namespace naivefs

#endif