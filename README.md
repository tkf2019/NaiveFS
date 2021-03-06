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
- FUSE request queues:
  - interrupts: INTERRUPT, 
  - forgets: FORGET (selected fairly with non-FORGET requests)
  - pending: synchronous requests (e.g., Metadata)
  - processing: the oldest pending request is moved to user space and the processing queue
  - background: asynchronous requests (read requests and write requests if the writeback cache is enabled)

#### [Linux User API](https://man7.org/linux/man-pages/man2)
#### TODO List

- [x] A makefile for whole project. Thus we can run tests and build targets in different environments.

- FUSE operations:
  - Special：
    - [x] `INIT`
    - [x] `DESTROY`
  - Metadata:
    - [x] `OPEN`
    - [x] `CREATE`
    - [ ] `STATFS`
    - [x] `LINK`
    - [x] `UNLINK`
    - [x] `RELEASE`
    - [ ] `ACCESS`
    - [x] `CHMOD`
    - [ ] `CHOWN`
    - [x] `TRUNCATE`
    - [x] `UTIMENS`
    - [x] `RENAME`
  - Data: (yfzcsc)
    - [x] `READ`
    - [x] `WRITE`
    - [x] `FLUSH`
    - [x] `FSYNC`
    - [ ] `COPY_FILE_RANGE` (not necessary)
    - [ ] `WRITE_BUF` (not necessary)
    - [ ] `READ_BUF` (not necessary)
  - Attributes: (yfzcsc)
    - [x] `GETATTR`
    - [ ] `SETATTR`
  - Extended Attributes: (yfzcsc)
    - [ ] `SETXATTR` (not necessary)
    - [ ] `GETXATTR` (not necessary)
    - [ ] `LISTXATTR` (not necessary)
    - [ ] `REMOVEXATTR` (not necessary)
  - Symlinks: (yfzcsc)
    - [x] `SYMLINK`
    - [x] `READLINK`
  - Directory: (yfzcsc)
    - [x] `MKDIR`
    - [x] `RMDIR`
    - [ ] `OPENDIR`  (not necessary)
    - [ ] `RELEASEDIR` (not necessary)
    - [x] `READDIR` 
    - [ ] `FSYNCDIR` (not necessary)
  - Locking:
    - [ ] `LOCK`
    - [ ] `FLOCK`
  - Misc: (yfzcsc)
    - [ ] `BMAP`
    - [ ] `FALLOCATE`
    - [ ] `MKNOD`
    - [ ] `LOCTL`
    - [ ] `POLL`

#### Run

```shell
bash run.sh
```
