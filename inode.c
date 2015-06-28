/*
  This file contains the routines that manipulate inodes. 
  
  THIS CODE COPYRIGHT DOMINIC GIAMPAOLO.  NO WARRANTY IS EXPRESSED 
  OR IMPLIED.  YOU MAY USE THIS CODE AND FREELY DISTRIBUTE IT FOR
  NON-COMMERCIAL USE AS LONG AS THIS NOTICE REMAINS ATTACHED.

  FOR COMMERCIAL USE, CONTACT DOMINIC GIAMPAOLO (dbg@be.com).

  Dominic Giampaolo
  dbg@be.com
*/
#include "myfs.h"

int
myfs_create_inodes(myfs_info *myfs)
{
    int       bsize = myfs->dsb.block_size;
    fs_off_t  map_start;
    fs_off_t  inodes_start;
    fs_off_t  num_inodes;
    fs_off_t  num_map_blocks;
    fs_off_t  num_inode_blocks;
    fs_off_t  i;
    void     *block;

    /* we allocate 1 inode for every 4 disk blocks */
    num_inodes       = (myfs->dsb.num_blocks >> 2);
    num_inode_blocks = (num_inodes * sizeof(myfs_inode)) / bsize;
    num_map_blocks   = (num_inode_blocks / 8 / bsize) + 1;

    printf("num inodes %ld num_map_blocks %ld num_inode blocks %ld\n",
           num_inodes, num_map_blocks, num_inode_blocks);
    
    if (pre_allocate_blocks(myfs, &num_map_blocks, &map_start) != 0) {
        printf("failed to allocate inode map blocks\n");
        return ENOSPC;
    }

    if (map_start != myfs->bbm.num_bitmap_blocks + 1)
        printf("that's weird... map_start == %ld, not %ld\n",
               map_start, myfs->bbm.num_bitmap_blocks);


    if (pre_allocate_blocks(myfs, &num_inode_blocks, &inodes_start) != 0) {
        printf("failed to allocate inode data blocks\n");
        return ENOSPC;
    }
    
    if (inodes_start != map_start+num_map_blocks)
        printf("that's weird inode_start == %ld not %ld\n", inodes_start,
               map_start+num_map_blocks);
    
    
    /* now we go through and zero out the inode map and the inode table */
    block = get_tmp_blocks(myfs, 1);
    memset(block, 0, bsize);
    
    for(i=0; i < num_map_blocks; i++) {
        if (write_blocks(myfs, map_start+i, block, 1) != 1) {
            printf("error writing inode map block %ld\n", map_start+i);
            break;
        }
    }

    for(i=0; i < num_inode_blocks; i++) {
        if (write_blocks(myfs, inodes_start+i, block, 1) != 1) {
            printf("error writing inode data block %ld\n", inodes_start+i);
            break;
        }
    }

    free_tmp_blocks(myfs, block, 1);

    myfs->dsb.num_inodes           = num_inodes;
    myfs->dsb.inodes_start         = inodes_start;
    myfs->dsb.num_inode_blocks     = num_inode_blocks;
    myfs->dsb.inode_map_start      = map_start;
    myfs->dsb.num_inode_map_blocks = num_map_blocks;

    myfs->inode_map.bits    = calloc(1, num_map_blocks * bsize);
    myfs->inode_map.numbits = num_inodes;
    if (myfs->inode_map.bits == NULL) {
        printf("couldn't allocate space for the in memory inode map\n");
        return ENOMEM;
    }

    /* allocate inode 0 so no one else gets (to enable better error checks) */
    GetFreeRangeOfBits(&myfs->inode_map, 1, NULL);
    write_blocks(myfs, map_start, myfs->inode_map.bits, 1);

    return 0;
}


int
myfs_init_inodes(myfs_info *myfs)
{
    int bsize = myfs->dsb.block_size;
    int amt;
    
    myfs->inode_map.bits    = calloc(1, myfs->dsb.num_inode_map_blocks*bsize);
    myfs->inode_map.numbits = myfs->dsb.num_inodes;
    if (myfs->inode_map.bits == NULL)
        return ENOMEM;

    amt = read_blocks(myfs, myfs->dsb.inode_map_start,
                      myfs->inode_map.bits, myfs->dsb.num_inode_map_blocks);
    if (amt != myfs->dsb.num_inode_map_blocks) {
        printf("failed to read the inode map!\n");
        return EINVAL;
    }

    return 0;
}


void
myfs_shutdown_inodes(myfs_info *myfs)
{
    free(myfs->inode_map.bits);
    myfs->inode_map.bits    = NULL;
    myfs->inode_map.numbits = 0;
}


myfs_inode *
myfs_allocate_inode(myfs_info *myfs, myfs_inode *parent, int mode)
{
    int         bsize = myfs->dsb.block_size;
    int         offset;
    char        tmp[IDENT_NAME_LENGTH];
    char       *block;
    inode_addr  ia;
    myfs_inode *mi;

    mi = (myfs_inode *)calloc(1, sizeof(myfs_inode));
    if (mi == NULL)
        return NULL;

    mi->etc = calloc(1, sizeof(myfs_inode_etc));
    if (mi->etc == NULL) {
        free(mi);
        return NULL;
    }

    ia = GetFreeRangeOfBits(&myfs->inode_map, 1, NULL);
    if (ia < 0) {
        free(mi->etc);
        free(mi);
        printf("no inodes left!\n");
        return NULL;
    }
    
    mi->magic1      = INODE_MAGIC1;
    mi->uid         = getuid();
    mi->gid         = getgid();
    mi->mode        = mode;
    mi->inode_num   = ia;
    mi->create_time = time(NULL);
    mi->last_modified_time = mi->create_time;

    sprintf(tmp, "ino#%ld", ia);
    new_lock(&mi->etc->lock, tmp);

    /*
      now we write the updated inode map back to disk.  we only write
      the changed inode map block.  the inode itself will be written
      later in update_inode().
    */
    block = (char *)myfs->inode_map.bits;
    offset = (ia / 8 / bsize);
    if (offset >= myfs->dsb.num_inode_map_blocks) {
        myfs_die("inode map offset %d is too big (max %ld)\n",
                 offset, myfs->dsb.num_inode_map_blocks);
    }
    
    write_blocks(myfs, myfs->dsb.inode_map_start+offset,
                 &block[offset*bsize], 1);


    return mi;
}


int
myfs_free_inode(myfs_info *myfs, inode_addr ia)
{
    int   bsize = myfs->dsb.block_size;
    int   offset;
    char *block;
    
    UnSetBV(&myfs->inode_map, ia);
    
    /* now update the on-disk inode map. */
    block = (char *)myfs->inode_map.bits;
    offset = (ia / 8 / bsize);
    write_blocks(myfs, myfs->dsb.inode_map_start+offset,
                 &block[offset*bsize], 1);


    return 0;
}


int
update_inode(myfs_info *myfs, myfs_inode *mi)
{
    int       bsize = myfs->dsb.block_size, offset;
    char     *block;
    fs_off_t  addr; 
    
    addr = myfs->dsb.inodes_start + ((mi->inode_num*sizeof(myfs_inode))/bsize);
    offset = (mi->inode_num % (bsize / sizeof(myfs_inode)))*sizeof(myfs_inode);
    
    if (addr > myfs->dsb.inodes_start + myfs->dsb.num_inode_blocks)
        myfs_die("error updating inode %ld: addr %ld out of range (max %ld)\n",
                 mi->inode_num,
                 addr,
                 myfs->dsb.inodes_start + myfs->dsb.num_inode_blocks);

    block = get_block(myfs->fd, addr, bsize);
    
    memcpy(&block[offset], mi, sizeof(myfs_inode));
    mark_blocks_dirty(myfs->fd, addr, 1);

    release_block(myfs->fd, addr);

    return 0;
}
