/*
  This header contains the main definition of the file system.  This
  includes the definitions of inodes, the super block, etc.
  
  
  THIS CODE COPYRIGHT DOMINIC GIAMPAOLO.  NO WARRANTY IS EXPRESSED 
  OR IMPLIED.  YOU MAY USE THIS CODE AND FREELY DISTRIBUTE IT FOR
  NON-COMMERCIAL USE AS LONG AS THIS NOTICE REMAINS ATTACHED.

  FOR COMMERCIAL USE, CONTACT DOMINIC GIAMPAOLO (dbg@be.com).

  Dominic Giampaolo
  dbg@be.com
*/
#ifndef _MYFS_H
#define _MYFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "compat.h"
#include "fsproto.h"

#include "lock.h"
#include "cache.h"
#include "bitvector.h"


typedef my_ino_t inode_addr;


#if OFF_T_SIZE == 4
#define NUM_DIRECT_BLOCKS   20
#elif OFF_T_SIZE == 8
#define NUM_DIRECT_BLOCKS   8
#endif

/*
    The size of this structure directly influences the size
    of the myfs_inode struct.  The size of the myfs_inode
    struct must be a power of two in size. Therefore the 
    number of direct blocks is chosen so that everything 
    just works out.
*/  
typedef struct data_stream
{
    fs_off_t direct[NUM_DIRECT_BLOCKS];
    fs_off_t indirect;
    fs_off_t double_indirect;
    fs_off_t size;
} data_stream;



typedef struct myfs_inode_etc {  /* these fields are needed when in memory */
    lock   lock;                 /* guard access to this inode */
    char  *contents;             /* contents of a directory */
    int    counter;
} myfs_inode_etc;


#define MAX_INODE_ACCESS  1024


/* 
    The inode struct should also be a power of two in size.
    Currently it is 128 bytes in size.  Keep that in mind if
    you decide to change it or the data_stream struct above.
*/
typedef struct myfs_inode
{
    int32           magic1;
    int32           uid;
    int32           gid;
    int32           mode;
    inode_addr      inode_num;
    int32           flags;
    time_t          create_time;
    time_t          last_modified_time;

    myfs_inode_etc *etc;                    /* only used in memory */

    data_stream     data;
} myfs_inode;


#define INODE_MAGIC1      0x496e6f64  /* 'Inod' */

#define INODE_IN_USE      0x00000001
#define ATTR_INODE        0x00000004  /* this inode refers to an attribute */
#define INODE_LOGGED      0x00000008  /* log all i/o to this inode's data */
#define INODE_DELETED     0x00000010  /* this inode has been deleted */

#define PERMANENT_FLAGS   0x0000ffff  /* mask for permanent flags */

#define INODE_WAS_WRITTEN 0x00010000  /* this inode has had its data written */

#define CHECK_INODE(mi)    {                                        \
    if ((mi)->magic1 != INODE_MAGIC1)                               \
        myfs_die("inode @ 0x%lx: magic is wrong (0x%x) at %s:%d\n",  \
                (ulong)(mi), (mi)->magic1, __FILE__, __LINE__);     \
    if (mi->etc == NULL)                                            \
        myfs_die("inode @ 0x%lx: null etc ptr at %s:%d\n",           \
                (ulong)(mi), __FILE__, __LINE__);                   \
    if (mi->data.size < 0)                                          \
        myfs_die("inode @ 0x%lx: size is less than zero %s:%d\n",    \
                (ulong)(mi), __FILE__, __LINE__);                   \
}


typedef struct myfs_dirent {    /* these are stored, packed, in a directory */
    my_ino_t inum;
    uchar    name_len;
    char     name[1];
} myfs_dirent;


typedef struct dir_cookie {
    char      *curptr;
    fs_off_t   index;
    int        counter;
} dir_cookie;




typedef struct myfs_super_block        /* the super block as it is on disk */
{
    char         name[IDENT_NAME_LENGTH];
    int32        magic1;
    int32        fs_byte_order;

    uint32       block_size;           /* in bytes */
    uint32       block_shift;          /* block_size == (1 << block_shift) */

    fs_off_t     num_blocks;
    fs_off_t     used_blocks;

    fs_off_t     num_inodes;           /* total # of inodes on this volume */
    fs_off_t     inode_map_start;      /* start of allocated inode map */
    fs_off_t     num_inode_map_blocks; /* # of inode map blocks */
    fs_off_t     inodes_start;         /* start of actual inodes */
    fs_off_t     num_inode_blocks;     /* # of blocks used by inodes */

    int32        magic2;
    int          flags;

    inode_addr   root_inum;            /* inode # of the root directory */

    fs_off_t     journal_start;        /* start address of the journal */
    fs_off_t     journal_length;       /* # of blocks in the journal */

    fs_off_t     log_start;            /* block # of the first active entry */
    fs_off_t     log_end;              /* block # of the end of the log */

    int32        magic3;
} myfs_super_block;


#define MYFS_CLEAN   0x434c454e        /* 'CLEN', for flags field */ 
#define MYFS_DIRTY   0x44495254        /* 'DIRT', for flags field */ 


typedef struct block_bitmap
{
    BitVector  *bv;
    fs_off_t    num_bitmap_blocks;
} block_bitmap;


#define NUM_TMP_BLOCKS   16

typedef struct tmp_blocks
{
    struct tmp_blocks *next;
    char              *data;
    char              *block_ptrs[NUM_TMP_BLOCKS];
} tmp_blocks;


typedef struct log_list {
    fs_off_t         start, end;
    struct log_list *next;
} log_list;


struct j_entry;

typedef struct myfs_info
{
    nspace_id        nsid;
    myfs_super_block dsb;     /* as read from disk */

    block_bitmap     bbm;     /* keeps track of which blocks are allocated */
    sem_id           bbm_sem;

    BitVector        inode_map;  /* keeps track which inodes are allocated */

    sem_id           sem;     /* guard access to this structure */

    int              fd;      /* the device we're using */
    int              flags;   /* read-only, etc */

    /* size in bytes of the underlying device block size */
    uint             dev_block_size;

    /* multiply by this to get the device block # from the fs block # */
    fs_off_t         dev_block_conversion;

    myfs_inode      *root_dir;

    sem_id           log_sem;        /* guards access to log structures */
    fs_off_t         cur_log_end;    /* where we are in the log currently */
    struct j_entry  *cur_je;         /* the current transaction handle */
    long             active_lh;      /* true if cur_lh is active */
    thread_id        log_owner;      /* current owner of log_sem */
    int              blocks_per_le;  /* # of blocks per log entry */
    fs_off_t        *log_block_nums; /* used to call set_blocks_info() */
    bigtime_t        last_log;       /* time of last entry (for flushing) */

    /* this is a list of log transactions that have completed out of order */
    log_list        *completed_log_entries;


    sem_id           tmp_blocks_sem;
    tmp_blocks      *tmp_blocks;
} myfs_info;



#define SUPER_BLOCK_MAGIC1   0x4d594653    /* MYFS */
#define SUPER_BLOCK_MAGIC2   0xce0169f9
#define SUPER_BLOCK_MAGIC3   0x53424c4b    /* SBLK */

#define MYFS_BIG_ENDIAN      0x42494745    /* BIGE */
#define MYFS_LITTLE_ENDIAN   0x45474942    /* EGIB */

#define MAX_READERS          1000000       /* max # of concurrent readers */

/* flags for the myfs_info flags field */
#define FS_READ_ONLY         0x00000001

/* how many free blocks are there on a volume */
#define NUM_FREE_BLOCKS(x) ((x)->dsb.num_blocks - (x)->dsb.used_blocks)


#include "mount.h"
#include "bitmap.h"
#include "journal.h"
#include "inode.h"
#include "dstream.h"
#include "dir.h"
#include "file.h"
#include "io.h"
#include "util.h"
#include "initfs.h"


#endif /* _MYFS_H */
