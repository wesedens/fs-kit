/*
  This file contains some low level wrapper routines that manage IO for
  the file system as well as handle reading and writing the super block.
  
  THIS CODE COPYRIGHT DOMINIC GIAMPAOLO.  NO WARRANTY IS EXPRESSED 
  OR IMPLIED.  YOU MAY USE THIS CODE AND FREELY DISTRIBUTE IT FOR
  NON-COMMERCIAL USE AS LONG AS THIS NOTICE REMAINS ATTACHED.

  FOR COMMERCIAL USE, CONTACT DOMINIC GIAMPAOLO (dbg@be.com).

  Dominic Giampaolo
  dbg@be.com
*/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include "myfs.h"


ssize_t
read_blocks(myfs_info *myfs, fs_off_t block_num, void *block, size_t nblocks)
{
    ssize_t ret;

/* printf("R 0x%x 0x%x\n", block_num, nblocks); */

    if (block_num >= myfs->dsb.num_blocks) {
        printf("Yikes! tried to read block %ld but there are only %ld\n",
               block_num, myfs->dsb.num_blocks);
        return -1;
    }

    if (block_num < myfs->dsb.num_blocks &&
        block_num + nblocks > myfs->dsb.num_blocks) {

        printf("Yikes! tried to read %d blocks starting at %ld but only have "
               "%ld\n", nblocks, block_num, myfs->dsb.num_blocks);
        return -1;
    }

    ret = cached_read(myfs->fd,
                      block_num,
                      block,
                      nblocks,
                      myfs->dsb.block_size);

    if (ret == 0)
        return nblocks;
    else
        return -1;
}

ssize_t
write_blocks(myfs_info *myfs, fs_off_t block_num, const void *block, size_t nblocks)
{
    ssize_t ret;

/* printf("W 0x%x 0x%x\n", block_num, nblocks); */

    if (myfs->flags & FS_READ_ONLY) {
        printf("write_super_block called on a read-only device! (myfs 0x%x)\n",
               myfs);
        return -1;
    }

    if (block_num >= myfs->dsb.num_blocks) {
        printf("Yikes! tried to write block %ld but there are only %ld\n",
               block_num, myfs->dsb.num_blocks);
        return -1;
    }

    if (block_num < myfs->dsb.num_blocks &&
        block_num + nblocks > myfs->dsb.num_blocks) {
        printf("Yikes! tried to write %d blocks starting at %ld but only "
               "have %ld\n", nblocks, block_num, myfs->dsb.num_blocks);
        return -1;
    }

    ret = cached_write(myfs->fd,
                       block_num,
                       block,
                       nblocks,
                       myfs->dsb.block_size);

    if (ret == 0)
        return nblocks;
    else
        return -1;

    return ret;
}


int
read_super_block(myfs_info *myfs)
{
    char *buff;
    int   block_size;
    
    block_size = get_device_block_size(myfs->fd);
    if (block_size < 0)
        return EINVAL;
    myfs->dev_block_size = block_size;

    if (block_size < 1024)  
        block_size = 1024;

    buff = (char *)malloc(block_size);
    if (buff == NULL)
        return ENOMEM;

    if (read_pos(myfs->fd, 0, buff, block_size) != block_size) {
        free(buff);
        return EINVAL;
    }       

    memcpy(&myfs->dsb, buff, sizeof(myfs_super_block));

    free(buff);

    myfs->dev_block_size       = block_size;
    myfs->dev_block_conversion = myfs->dsb.block_size / block_size;

    return 0;
}

int
write_super_block(myfs_info *myfs)
{
    ssize_t  amt;

    myfs->dsb.flags = MYFS_CLEAN;        /* now it's clean! */
    amt = write_pos(myfs->fd, 0, &myfs->dsb, myfs->dsb.block_size);

    if (amt == myfs->dsb.block_size)
        return 0;
    else
        return -1;
}
