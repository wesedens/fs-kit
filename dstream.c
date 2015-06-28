/*
  This file contains the code to read and write the data stream of a file.
  
  THIS CODE COPYRIGHT DOMINIC GIAMPAOLO.  NO WARRANTY IS EXPRESSED 
  OR IMPLIED.  YOU MAY USE THIS CODE AND FREELY DISTRIBUTE IT FOR
  NON-COMMERCIAL USE AS LONG AS THIS NOTICE REMAINS ATTACHED.

  FOR COMMERCIAL USE, CONTACT DOMINIC GIAMPAOLO (dbg@be.com).

  Dominic Giampaolo
  dbg@be.com
*/
#include "myfs.h"

int
myfs_init_data_stream(myfs_info *myfs, myfs_inode *mi)
{
    /* nothing to do for this simple implementation */
    return 0;
}


#define MAX_DIRECT_RANGE          (NUM_DIRECT_BLOCKS * bsize)
#define MAX_INDIRECT_RANGE        (((bsize / sizeof(fs_off_t)) * bsize) +   \
                                   MAX_DIRECT_RANGE)
#define MAX_DOUBLE_INDIRECT_RANGE (((bsize / sizeof(fs_off_t)) *            \
                                    ((bsize / sizeof(fs_off_t)) * bsize)) + \
                                   MAX_INDIRECT_RANGE)

/* this is the amount of data mapped by each set of blocks */
#define DIRECT_SIZE               (NUM_DIRECT_BLOCKS * bsize)
#define INDIRECT_SIZE             ((bsize / sizeof(fs_off_t)) * bsize)
#define DOUBLE_INDIRECT_SIZE      ((bsize / sizeof(fs_off_t)) *            \
                                   ((bsize / sizeof(fs_off_t)) * bsize))



static fs_off_t
file_pos_to_disk_addr(myfs_info *myfs, myfs_inode *mi, fs_off_t pos)
{
    int       bsize = myfs->dsb.block_size;
    int       index;
    fs_off_t  addr, offset, tmp;
    fs_off_t *block, *block2;
    
    if (pos < NUM_DIRECT_BLOCKS*bsize) {
        index = pos / bsize;
        addr = mi->data.direct[index];
        
        if (addr < 0 || addr > myfs->dsb.num_blocks) {
            myfs_die("file_pos:1: addr 0x%lx is out of range (max %ld)\n",
                     addr, myfs->dsb.num_blocks);
        }

        return addr;
    } else if (pos < MAX_INDIRECT_RANGE) {
        block = get_block(myfs->fd, mi->data.indirect, bsize);

        addr = block[(pos - MAX_DIRECT_RANGE) / bsize];

        release_block(myfs->fd, mi->data.indirect);

        if (addr < 0 || addr > myfs->dsb.num_blocks) {
            myfs_die("file_pos:1: addr 0x%lx is out of range (max %ld)\n",
                     addr, myfs->dsb.num_blocks);
        }
        
        return addr;
    } else if (pos < MAX_DOUBLE_INDIRECT_RANGE) {
        index = (int)((pos - MAX_INDIRECT_RANGE) / INDIRECT_SIZE);
        block = get_block(myfs->fd, mi->data.double_indirect, bsize);
        addr = block[index];
        release_block(myfs->fd, mi->data.double_indirect);

        tmp = addr;
        block = get_block(myfs->fd, tmp, bsize);
        addr = block[(((pos - MAX_INDIRECT_RANGE) % INDIRECT_SIZE) / bsize)];
        release_block(myfs->fd, tmp);

        if (addr < 0 || addr > myfs->dsb.num_blocks) {
            myfs_die("file_pos:1: addr 0x%lx is out of range (max %ld)\n",
                     addr, myfs->dsb.num_blocks);
        }
        
    } else {
        printf("heidy-ho... looks like it's time to implement triple "
               "indirect blocks.  have fun!\n");
        return -1;
    }
    
    return -1;
}

int
myfs_read_data_stream(myfs_info *myfs, myfs_inode *mi,
                           fs_off_t pos, char *buf, size_t *_len)
{
    int       i, offset, bsize = myfs->dsb.block_size;
    size_t    len = *_len, amt;
    fs_off_t  addr;
    char     *block;
    
    if (len == 0)    /* just a quick check */
        return 0;

    if (pos < 0)
        pos = 0;
    
    if (pos > mi->data.size) {
        *_len = 0;
        return 0;
    }

    *_len = 0;    /* this will now count up */
    
    if (pos + len > mi->data.size)
        len = mi->data.size - pos;

    addr = file_pos_to_disk_addr(myfs, mi, pos);
    if (addr < 0)
        return EINVAL;

    if ((pos % bsize) != 0) { /* then we have to work up to a block boundary */
        block = get_block(myfs->fd, addr, bsize);
        if (block == NULL)
            return EINVAL;

        offset = (pos % bsize);
        if (len < (bsize - offset))
            amt = len;
        else
            amt = (bsize - offset);

        memcpy(buf, &block[offset], amt);

        buf   += amt;
        pos   += amt;
        *_len += amt;

        release_block(myfs->fd, addr);
    }

    /* this is the main data reading loop */
    for(; *_len < len; *_len+=amt) {
        addr = file_pos_to_disk_addr(myfs, mi, pos);
        if (addr < 0)
            return EINVAL;

        block = get_block(myfs->fd, addr, bsize);
        if (block == NULL)
            return EINVAL;

        if ((len - *_len) < bsize)
            amt = len - *_len;
        else
            amt = bsize;

        memcpy(buf, block, amt);

        buf   += amt;
        pos   += amt;

        release_block(myfs->fd, addr);
    }

    return 0;
}


static int
grow_dstream(myfs_info *myfs, myfs_inode *mi, fs_off_t new_size)
{
    int       bsize = myfs->dsb.block_size;
    int       index, err;
    fs_off_t  i, max_index = bsize / sizeof(fs_off_t);
    fs_off_t  addr, offset;
    fs_off_t  cur_size_rounded, new_size_rounded, num_blocks_needed;
    fs_off_t  nblocks;
    fs_off_t *block, *block2;
    
    if (new_size > MAX_DOUBLE_INDIRECT_RANGE)
        return E2BIG;

    /* round up the current file size to the next block boundary */
    cur_size_rounded = (mi->data.size + bsize - 1) & ~(bsize - 1);

    /* round up the new file size to the next block boundary */
    new_size_rounded = (new_size + bsize - 1) & ~(bsize - 1);

    if (cur_size_rounded == new_size_rounded) {  /* can grow in-place */
        mi->data.size = new_size;
        
        return 0;
    }

    num_blocks_needed = ((new_size_rounded - cur_size_rounded) / bsize);

    i = 0;
    mi->data.size = cur_size_rounded;

    if (cur_size_rounded < DIRECT_SIZE) {   /* grow the direct blocks first */

        /* use the rounded size to calculate the index to start growing at */
        index = cur_size_rounded / bsize;

        if (mi->data.direct[index] != 0) {
            myfs_die("grow_dstream: mi (inum %ld): direct[%d] == %ld, not "
                     "zero!\n", mi->inode_num, index, mi->data.direct[index]);
        }

        for(; i < num_blocks_needed && index < NUM_DIRECT_BLOCKS; i++,index++){
            nblocks = 1;
            err = myfs_allocate_blocks(myfs, mi->inode_num, &nblocks, &addr,
                                       EXACT_ALLOCATION);
            if (err != 0)
                return err;
            
            if (addr < 0)
                return ENOSPC;

            mi->data.direct[index]  = addr;
            mi->data.size          += bsize;
        }

        if (mi->data.size >= new_size) {   /* all done! */
            mi->data.size = new_size;
            return 0;
        }
    }

    /* now see if we have to grow the indirect range */
    if (mi->data.size >= MAX_DIRECT_RANGE &&
        mi->data.size < MAX_INDIRECT_RANGE) {
        
        if (mi->data.indirect == 0) {
            nblocks = 1;
            err = myfs_allocate_blocks(myfs, mi->inode_num, &nblocks, &addr,
                                       EXACT_ALLOCATION);
            if (err != 0)
                return err;
            
            if (addr < 0)
                return ENOSPC;

            mi->data.indirect = addr;
            block = get_empty_block(myfs->fd, addr, bsize);
        } else {
            block = get_block(myfs->fd, mi->data.indirect, bsize);
        }

        index = (mi->data.size - MAX_DIRECT_RANGE) / bsize;
        for(; i < num_blocks_needed && index < max_index; i++, index++) {
            nblocks = 1;
            err = myfs_allocate_blocks(myfs, mi->inode_num, &nblocks, &addr,
                                       EXACT_ALLOCATION);
            if (err != 0)
                return err;
            
            if (addr < 0)
                return ENOSPC;
            
            block[index]   = addr;
            mi->data.size += bsize;
        }

        mark_blocks_dirty(myfs->fd, mi->data.indirect, 1);
        release_block(myfs->fd, mi->data.indirect);

        if (mi->data.size >= new_size) {   /* all done! */
            mi->data.size = new_size;

            return 0;
        }
    }
    
    if (mi->data.size >= MAX_INDIRECT_RANGE) {
        /* XXXdbg -- growing double indirect blocks! */
        printf("grow the double indirect blocks....\n");
        return E2BIG;
    }

    return -1;
}


static void
shrink_double_indirect(myfs_info *myfs, myfs_inode *mi,
                       fs_off_t new_size_rounded)
{
    int       bsize = myfs->dsb.block_size, free_block, free_block2;
    fs_off_t  i, j, max_index = bsize / sizeof(fs_off_t);
    fs_off_t *block, *block2;

    block = get_block(myfs->fd, mi->data.double_indirect, bsize);
    if (block == NULL) {
        myfs_die("error getting double indirect block %ld for inode %ld\n",
                 mi->data.double_indirect, mi->inode_num);
    }

    if (new_size_rounded < MAX_INDIRECT_RANGE) {
        i = 0;
        j = 0;
    } else {
        i = ((new_size_rounded-MAX_INDIRECT_RANGE) / INDIRECT_SIZE);
        j = ((new_size_rounded-MAX_INDIRECT_RANGE) % INDIRECT_SIZE) /bsize;
    }

    free_block  = (i == 0);
    free_block2 = (j == 0);
        
    for(; i < max_index; i++) {
        if (block[i] == 0)
            break;
                
        block2 = get_block(myfs->fd, block[i], bsize);
        if (block2) {
            for (; j < max_index; j++) {
                if (myfs_free_blocks(myfs, block2[j], 1) != 0)
                    printf("1: error freeing double indirect block %ld "
                           "for inode %ld\n", block2[j],mi->inode_num);
            }
            
            release_block(myfs->fd, block[i]);
        }
        
        if (free_block2) {
            if (myfs_free_blocks(myfs, block[i], 1) != 0)
                printf("1: error freeing double indirect block %ld "
                       "for inode %ld\n", block[i], mi->inode_num);
        }
        
        j = 0;                /* for the next time through the loop */
        free_block2 = 1;
    }
        
    release_block(myfs->fd, mi->data.double_indirect);
    
    if (free_block) {
        if (myfs_free_blocks(myfs, mi->data.double_indirect, 1) != 0)
            printf("3: error freeing double indirect block %ld for "
                   "inode %ld\n",mi->data.double_indirect,mi->inode_num);
        mi->data.double_indirect = 0;
    }
}


static int
shrink_dstream(myfs_info *myfs, myfs_inode *mi, fs_off_t new_size)
{
    int       bsize = myfs->dsb.block_size, free_block, free_block2;
    fs_off_t  i, j, max_index = bsize / sizeof(fs_off_t);
    fs_off_t  addr, offset;
    fs_off_t  cur_size_rounded, new_size_rounded;
    fs_off_t *block, *block2;
    
    if (new_size > MAX_DOUBLE_INDIRECT_RANGE)
        return E2BIG;

    /* round up the current file size to the next block boundary */
    cur_size_rounded = (mi->data.size + bsize - 1) & ~(bsize - 1);

    /* round up the new file size to the next block boundary */
    new_size_rounded = (new_size - 1 + bsize - 1) & ~(bsize - 1);

    if (cur_size_rounded == new_size_rounded) {  /* can shrink in-place */
        mi->data.size = new_size;
        return 0;
    }


    /*
       start trimming the fat at the double indirect blocks and work
       backwards from there.
    */   
    if (new_size_rounded < MAX_DOUBLE_INDIRECT_RANGE &&
        mi->data.double_indirect) {

        shrink_double_indirect(myfs, mi, new_size_rounded);
    }
    
    /* now see if we have to trim off indirect blocks */
    if (new_size_rounded < MAX_INDIRECT_RANGE && mi->data.indirect) {
        if (new_size_rounded > MAX_DIRECT_RANGE)
            i = (new_size_rounded - MAX_DIRECT_RANGE) / bsize;
        else
            i = 0;
        
        free_block = (i == 0);

        block = get_block(myfs->fd, mi->data.indirect, bsize);
        for(; i < max_index; i++) {
            if (block[i] == 0)
                break;

            if (myfs_free_blocks(myfs, block[i], 1) != 0)
                printf("1: error freeing indirect block %ld for inode %ld\n",
                       block[i], mi->inode_num);
        }

        release_block(myfs->fd, mi->data.indirect);

        if (free_block) {
            if (myfs_free_blocks(myfs, mi->data.indirect, 1) != 0)
                printf("1: error freeing indirect block %ld for inode %ld\n",
                       mi->data.indirect, mi->inode_num);

            mi->data.indirect = 0;
        }
    }
    
    /* now we trim the indirect blocks */
    if (new_size_rounded < DIRECT_SIZE) {
        i = new_size_rounded / bsize;

        for(; i < NUM_DIRECT_BLOCKS; i++){
            if (mi->data.direct[i] == 0)
                break;
            
            if (myfs_free_blocks(myfs, mi->data.direct[i], 1) != 0)
                printf("error free'ing direct data block %ld\n",
                       mi->data.direct[i]);

            mi->data.direct[i] = 0;
        }
    }

    mi->data.size = new_size;

    return 0;
}



int
myfs_write_data_stream(myfs_info *myfs, myfs_inode *mi,
                           fs_off_t pos, const char *buf, size_t *_len)
{
    int       i, offset, bsize = myfs->dsb.block_size, err;
    size_t    len = *_len, amt;
    fs_off_t  addr;
    char     *block;
    
    if (len == 0)    /* just a quick check */
        return 0;

    if (pos < 0)
        pos = 0;
    
    if (pos + len > mi->data.size) {
        err = grow_dstream(myfs, mi, pos + len);
        if (err) {
            printf("grow dstream failed!\n");
            return err;
        }

        mi->last_modified_time = time(NULL);
        update_inode(myfs, mi);
        write_super_block(myfs);
    }

    *_len = 0;    /* this will now count up */
    

    addr = file_pos_to_disk_addr(myfs, mi, pos);
    if (addr < 0)
        return EINVAL;

    if ((pos % bsize) != 0) { /* then we have to work up to a block boundary */
        block = get_block(myfs->fd, addr, bsize);
        if (block == NULL)
            return EINVAL;

        offset = (pos % bsize);
        if (len < (bsize - offset))
            amt = len;
        else
            amt = (bsize - offset);

        memcpy(&block[offset], buf, amt);

        buf   += amt;
        pos   += amt;
        *_len += amt;

        mark_blocks_dirty(myfs->fd, addr, 1);
        release_block(myfs->fd, addr);
    }

    /* this is the main data writing loop */
    for(; *_len < len; *_len+=amt) {
        addr = file_pos_to_disk_addr(myfs, mi, pos);
        if (addr < 0)
            return EINVAL;

        block = get_block(myfs->fd, addr, bsize);
        if (block == NULL)
            return EINVAL;

        if ((len - *_len) < bsize)
            amt = len - *_len;
        else
            amt = bsize;

        memcpy(block, buf, amt);

        buf   += amt;
        pos   += amt;

        mark_blocks_dirty(myfs->fd, addr, 1);
        release_block(myfs->fd, addr);
    }

    mi->last_modified_time = time(NULL);
    update_inode(myfs, mi);

    return 0;
}

int
myfs_set_file_size(myfs_info *myfs, myfs_inode *mi, fs_off_t new_size)
{
    int err = 0;
    
    if (new_size == mi->data.size)
        return 0;

    if (new_size < mi->data.size)
        err = shrink_dstream(myfs, mi, new_size);
    else
        err = grow_dstream(myfs, mi, new_size);
            
    if (err == 0) {
        mi->last_modified_time = time(NULL);
        update_inode(myfs, mi);

        write_super_block(myfs);
    }
        
    return err;
}

int
myfs_free_data_stream(myfs_info *myfs, myfs_inode *mi)
{
    shrink_dstream(myfs, mi, 0);
    write_super_block(myfs);
    
    return 0;
}
