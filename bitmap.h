#ifndef _BITMAP_H
#define _BITMAP_H

#include "bitvector.h"

int  myfs_create_storage_map(myfs_info *myfs);
int  myfs_init_storage_map(myfs_info *myfs);
void myfs_shutdown_storage_map(myfs_info *myfs);

int  myfs_allocate_blocks(myfs_info *myfs, fs_off_t start_hint,
                          fs_off_t *num_blocks,fs_off_t *start_addr,int flags);
int  pre_allocate_blocks(myfs_info *myfs,fs_off_t *num_blocks,fs_off_t *start_addr);
int  myfs_free_blocks(myfs_info *myfs, fs_off_t start, fs_off_t len);
int  myfs_check_blocks(myfs_info *myfs, fs_off_t start,fs_off_t len,int state);

/*
 * these are the flags for the last argument to myfs_allocate_blocks()
 */
#define EXACT_ALLOCATION    1
#define LOOSE_ALLOCATION    2
#define TRY_HARD_ALLOCATION 3

#endif  /* _BITMAP_H */

