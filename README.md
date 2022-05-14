# NaiveFS
Naive file system based on fuse.
#### About FUSE

- File System in Userspace: the most widely used user-space file system framework.
- Provides simple API and easy to understand its internal architecture and implementation details.
- FUSE's kernel module registers a fuse file-system driver and a /dev/fuse block device (like a shared space between user daemon and kernel module for exchanging requests and responses).
- VFS will route file requests from userspace to registered fuse kernel driver. The kernel driver allocates a FUSE request structure and puts it in a FUSE queue. Process wait for the request handled by user daemon of fuse. Processing the request might require re-entering the kernel again:
  - In our implementation, we have to handle requests by accessing virtual disk `/dev/disk` to simulate real file system driver. So one request from user processes will at least trap into kernel 3 times: 1. user process submits request; 2. fuse user daemon (our file system) handles virtual disk; 3. fuse user daemon submits response.
  - The user process will be suspended until all the processes above is finished.
  - Crash may happens at any point of these processes, so we need to manage consistency and persistency.
- User-kernel protocol:
  - handle requests from FUSE's kernel driver which have a direct mapping to traditional VFS operations;
  - Request structure:
    - type
    - sequence number
    - node ID: an unsigned 64-bit integer
- FUSE request types:
  - Special：
    - [x] `INIT`： user space and kernel negotiate: 1. the protocol version they will operate on; 2. the set of mutually supported capabilities; 3. various parametter settings.
    - [ ] `DESTROY`： the daemon is expected to perform all necessary cleanups. No more requests will come from the kernel for this session and subsequent reads from /dev/fuse will return 0.
    - [ ] `INTERRUPT`： emitted by kernel if any previously sent requests are no longer needed (e.g., blocked READ is terminated)
  - Metadata:
    - [ ] `LOOKUP`: path-to-inode translation.
    - [ ] `FORGET`: the daemon deallocate any corresponding data structures when inode is removed from dcache.
    - [ ] `BATCH_FORGET`: forget multiple inodes with a single request.
    - CREATE:
    - UNLINK:
    - LINK:
    - RENAME:
    - RENAME2:
    - [ ] `OPEN`: the daemon has a chance to optionally assign a 64-bit file handle to the opened file.
    - [ ] `RELEASE`: no more references to a previously opened file.
    - STATFS:
    - FSYNC:
    - [ ] `FLUSH`: generated every time an opened file is closed.
    - [ ] `ACCESS`: generated when the kernel evaluates if a user process has premission to access a file. (default_premissions option will delegate permission checks back to kernel, thus no ACCESS request will be generated)
  - Data: (yfzcsc)
    - READ:
    - WRITE:
  - Attributes: (yfzcsc)
    - GETATTR:
    - SETATTR:
  - Extended Attributes: (yfzcsc)
    - SETXATTR:
    - GETXATTR:
    - LISTXATTR:
    - REMOVEXATTR:
  - Symlinks: (yfzcsc)
    - SYMLINK:
    - READLINK:
  - Directory: (yfzcsc)
    - MKDIR:
    - RMDIR:
    - [ ] `OPENDIR`: the daemon assigns a 64-bit file handle to the opened directory. 
    - [ ] `RELEASEDIR`: no more references to a previously opened directory. 
    - READDIR: 
    - [ ] `READDIRPLUS`: returns one or more directory entries like READDIR, but including metadata information for each entry, which allows the kernel to pre-fill its inode cache.
    - FSYNCDIR:
  - Locking:
    - GETLK:
    - SETLK:
    - SETLKW:
  - Misc: (yfzcsc)
    - BMAP:
    - FALLOCATE:
    - MKNOD:
    - LOCTL:
    - POLL:
    - NOTIFY_REPLY:
- FUSE request queues:
  - interrupts: INTERRUPT, 
  - forgets: FORGET (selected fairly with non-FORGET requests)
  - pending: synchronous requests (e.g., Metadata)
  - processing: the oldest pending request is moved to user space and the processing queue
  - background: asynchronous requests (read requests and write requests if the writeback cache is enabled)

#### Linux User API

- [stat](https://man7.org/linux/man-pages/man2/lstat.2.html)
- [fallocate](https://man7.org/linux/man-pages/man2/fallocate.2.html)
#### Backgrounds

- Currently developing and running on linux (version 5.13.0) in 20.04.1 LTS Ubuntu desktop.
- Build target: `cargo run` in root directory.

#### TODO List

- [ ] A makefile for whole project. Thus we can run tests and build targets in different environments.

#### Requirements

- Cache in memory is limited to smaller than 1GB.
- 
