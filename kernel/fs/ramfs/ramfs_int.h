/* SPDX-License-Identifier: BSD-2-Clause */

#pragma once
#include <tilck/common/basic_defs.h>
#include <tilck/common/string_util.h>

#include <tilck/kernel/fs/vfs.h>
#include <tilck/kernel/sched.h>
#include <tilck/kernel/kmalloc.h>
#include <tilck/kernel/errno.h>
#include <tilck/kernel/list.h>
#include <tilck/kernel/user.h>
#include <tilck/kernel/sync.h>
#include <tilck/kernel/rwlock.h>
#include <tilck/kernel/datetime.h>
#include <tilck/kernel/bintree.h>
#include <tilck/kernel/paging.h>

#include <dirent.h> // system header

struct ramfs_inode;
typedef struct ramfs_inode ramfs_inode;

enum ramfs_entry {
   RAMFS_FILE,
   RAMFS_DIRECTORY,
   RAMFS_SYMLINK,
};

typedef struct {

   bintree_node node;
   off_t offset;                  /* MUST BE divisible by PAGE_SIZE */
   void *vaddr;

} ramfs_block;

#define RAMFS_ENTRY_MAX_LEN (256 - sizeof(bintree_node) - sizeof(void *))

typedef struct {

   bintree_node node;
   struct ramfs_inode *inode;
   char name[RAMFS_ENTRY_MAX_LEN];

} ramfs_entry;

struct ramfs_inode {

   /*
    * Inode's ref-count is number of file handles currently pointing to this
    * inode.
    */
   REF_COUNTED_OBJECT;

   int inode;
   enum ramfs_entry type;
   nlink_t nlink;
   mode_t mode;                        /* permissions + special flags */
   rwlock_wp rwlock;
   off_t fsize;                        /* file size in bytes */
   size_t blocks_count;                /* count of page-size blocks */
   struct ramfs_inode *parent_dir;

   union {
      ramfs_block *blocks_tree_root;   /* valid when type == RAMFS_FILE */
      ramfs_entry *entries_tree_root;  /* valid when type == RAMFS_DIRECTORY */
      const char *path;                /* valid when type == RAMFS_SYMLINK */
   };

   time_t ctime;
   time_t mtime;
};

typedef struct {

   /* fs_handle_base */
   FS_HANDLE_BASE_FIELDS

   /* ramfs-specific fields */
   ramfs_inode *inode;
   off_t pos;

} ramfs_handle;

typedef struct {

   rwlock_wp rwlock;

   int next_inode_num;
   ramfs_inode *root;

} ramfs_data;

typedef struct {

   ramfs_inode *i;         // both the entry and the inode are required because
   ramfs_entry *e;         // the root dir has no entry.
   ramfs_inode *idir;
   const char *last_comp;

} ramfs_resolved_path;