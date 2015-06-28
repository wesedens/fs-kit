/*
  This file contains the routines that manipulate the map of used and
  free blocks on disk.
  
  
  THIS CODE COPYRIGHT DOMINIC GIAMPAOLO.  NO WARRANTY IS EXPRESSED 
  OR IMPLIED.  YOU MAY USE THIS CODE AND FREELY DISTRIBUTE IT FOR
  NON-COMMERCIAL USE AS LONG AS THIS NOTICE REMAINS ATTACHED.

  FOR COMMERCIAL USE, CONTACT DOMINIC GIAMPAOLO (dbg@be.com).

  Dominic Giampaolo
  dbg@be.com
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "myfs.h"

static void sanity_check_bitmap(myfs_info *myfs);

int
myfs_create_storage_map(myfs_info *myfs)
{
    char      *buff = NULL;
    int        ret, i, n, bsize;
    size_t     num_bytes, num_blocks;
    ssize_t    amt;
    BitVector *bv;

    myfs->bbm_sem = create_sem(1, "bbm");
    if (myfs->bbm_sem < 0)
        return ENOMEM;

    bsize = myfs->dsb.block_size;

    /*
       calculate how many bitmap blocks we'll need by rounding up the
       number of blocks on the disk to the nearest multiple of 8 times
       the block size (8 == number of bits in a byte).
    */   
    n  = myfs->dsb.num_blocks / 8;
    n  = ((n + bsize - 1) & ~(bsize - 1));
    n /= bsize;

    myfs->bbm.num_bitmap_blocks = n;

    
    myfs->bbm.bv = (BitVector *)calloc(1, sizeof(BitVector));
    if (myfs->bbm.bv == NULL) {
        ret = ENOMEM;
        goto err;
    }

    num_bytes = myfs->bbm.num_bitmap_blocks * myfs->dsb.block_size;
    buff = (char *)calloc(1, num_bytes);
    if (buff == NULL) {
        ret = ENOMEM;
        goto err;
    }
    
    myfs->bbm.bv->bits    = (chunk *)buff;
    myfs->bbm.bv->numbits = myfs->dsb.num_blocks; 


    /* fill in used blocks, including the superblock */
    num_blocks = myfs->bbm.num_bitmap_blocks + 1;
    if (GetFreeRangeOfBits(myfs->bbm.bv, num_blocks, NULL) != 0) {
        printf("*** failed to allocate %ld bits in create_bitmap\n",
               myfs->bbm.num_bitmap_blocks + 1);

        ret = ENOSPC;
        goto err;
    }

    /* write out the bitmap blocks, starting at block # 1 */
    amt = write_blocks(myfs, 1, buff, n);
    if (amt != n)  {
        ret = EINVAL;       
        goto err;
    }
        
    myfs->dsb.used_blocks += num_blocks;

    return 0;

 err:
    if (buff)
        free(buff);
    myfs->bbm.bv = NULL;
    
    if (myfs->bbm_sem > 0)
        delete_sem(myfs->bbm_sem);
    myfs->bbm_sem = -1;
    return ret;
}


int
myfs_init_storage_map(myfs_info *myfs)
{
    char      *buff = NULL;
    int        i, n, ret, bsize;
    size_t     num_bytes;
    ssize_t    amt;
    BitVector *bv;

    myfs->bbm_sem = create_sem(1, "bbm");
    if (myfs->bbm_sem < 0)
        return ENOMEM;

    bsize = myfs->dsb.block_size;

    n  = myfs->dsb.num_blocks / 8;
    n  = ((n + bsize - 1) & ~(bsize - 1));
    n /= bsize;

    myfs->bbm.num_bitmap_blocks = n;
    

    myfs->bbm.bv = (BitVector *)calloc(1, sizeof(BitVector));
    if (myfs->bbm.bv == NULL) {
        ret = ENOMEM;
        goto err;
    }


    num_bytes = myfs->bbm.num_bitmap_blocks * myfs->dsb.block_size;
    buff = (char *)malloc(num_bytes);
    if (buff == NULL) {
        ret = ENOMEM;
        goto err;
    }
    

    amt = read_blocks(myfs, 1, buff, n);
    if (amt != n) {
        printf("loading bitmap: failed to read %d bitmap blocks\n", n);
        ret = EINVAL;
        goto err;
    }

    myfs->bbm.bv->bits    = (chunk *)buff;
    myfs->bbm.bv->numbits = myfs->dsb.num_blocks;
    


    if (IsSetBV(myfs->bbm.bv, 0) == 0) {
        printf("myfs: danger! super block is not allocated! patching up...\n");
        SetBV(myfs->bbm.bv, 0);
        write_blocks(myfs, 1, myfs->bbm.bv[0].bits, 1);
    }

    printf("Checking block bitmap...\n");
    sanity_check_bitmap(myfs);
    printf("Done checking bitmap.\n");


    return 0;

 err:
    if (buff)
        free(buff);
    myfs->bbm.bv = NULL;
    if (myfs->bbm_sem > 0)
        delete_sem(myfs->bbm_sem);
    myfs->bbm_sem = -1;
    return ret;
}


void
myfs_shutdown_storage_map(myfs_info *myfs)
{
    if (myfs->bbm.bv) {
        if (myfs->bbm.bv->bits) {
            free(myfs->bbm.bv->bits);
            myfs->bbm.bv->bits = NULL;
        }
        free(myfs->bbm.bv);
        myfs->bbm.bv = NULL;
    }

    if (myfs->bbm_sem > 0)
        delete_sem(myfs->bbm_sem);
    myfs->bbm_sem = -1;
}


static int
real_allocate_blocks(myfs_info *myfs, fs_off_t *num_blocks,
                     fs_off_t *start_addr, int do_log_write, int exact)
{
    int        i, n, len, bsize = myfs->dsb.block_size, nblocks;
    int        biggest_free_chunk = 0, max_free_chunk = 1;
    fs_off_t   start = -1;
    char      *ptr;
    BitVector *bv;

    if (*num_blocks <= 0)
        return EINVAL;

    /* XXXdbg -- when journaling is implemented, fix this */
    do_log_write = 0;

    acquire_sem(myfs->bbm_sem);

    if (*num_blocks > (myfs->dsb.num_blocks - myfs->dsb.used_blocks)) {
        release_sem(myfs->bbm_sem);
        return ENOSPC;
    }

    for(nblocks=*num_blocks; nblocks >= 1; nblocks /= 2) {
        bv = myfs->bbm.bv;
        start = GetFreeRangeOfBits(bv, nblocks, &biggest_free_chunk);
        if (start != -1)
            break;
 
        if (biggest_free_chunk > max_free_chunk) {
            max_free_chunk  = biggest_free_chunk;
        }

        if (exact == LOOSE_ALLOCATION && biggest_free_chunk > 0 &&
            biggest_free_chunk >= (nblocks>>4)) {
            nblocks = biggest_free_chunk;
            start = GetFreeRangeOfBits(bv, nblocks, NULL);
            if (start != -1)
                break;
        }

        if (start != -1 || exact == EXACT_ALLOCATION)
            break;

        if (start == -1 && exact == TRY_HARD_ALLOCATION && max_free_chunk > 1) {
            if (max_free_chunk < nblocks)
                nblocks = max_free_chunk;

            start = GetFreeRangeOfBits(bv, nblocks, &biggest_free_chunk);
            if (start != -1) {
                i  = 0;
                break;
            }
        }

        if (exact == LOOSE_ALLOCATION && nblocks > max_free_chunk*2) {
            nblocks = max_free_chunk * 2;  /* times 2 because of the loop continuation */
        }

        max_free_chunk = 1;
    }

    if (start == -1) {
        release_sem(myfs->bbm_sem);
        return ENOSPC;
    }
    

    *start_addr = (fs_off_t)start;
    *num_blocks = nblocks;

    /*
       calculate the block number of the bitmap block we just modified.
       the +1 accounts for the super block.
    */
    n   = (start / 8 / bsize) + 1;
    len = (nblocks / 8 / bsize) + 1;
    
    ptr = (char *)bv->bits + (((start / 8) / bsize) * bsize);

    if (do_log_write)  {
        for(i=0; i < len; i++, ptr += bsize) {
            if (myfs_write_journal_entry(myfs, myfs->cur_je, n+i, ptr) != 1) {
                printf("error:1 failed to write bitmap block run %d:1!\n",n+i);
                release_sem(myfs->bbm_sem);
                return EINVAL;
            }
        }
    } else if (write_blocks(myfs, n, ptr, len) != len) {
        printf("error: 2 failed to write back bitmap block @ block %d!\n", n);
        release_sem(myfs->bbm_sem);
        return EINVAL;
    }

    myfs->dsb.used_blocks += *num_blocks;
    release_sem(myfs->bbm_sem);

    myfs->dsb.flags = MYFS_DIRTY;

    return 0;
}

int
myfs_allocate_blocks(myfs_info *myfs, fs_off_t start_hint,
                     fs_off_t *num_blocks, fs_off_t *start_addr, int exact)
{
    return real_allocate_blocks(myfs, num_blocks, start_addr, 1, exact);
}


/*
  this is for internal use only -- it's needed when created a file system
  and we can't start a journal transaction
*/  
int
pre_allocate_blocks(myfs_info *myfs, fs_off_t *num_blocks,fs_off_t *start_addr)
{
    return real_allocate_blocks(myfs, num_blocks, start_addr, 0,
                                LOOSE_ALLOCATION);
}


int
myfs_free_blocks(myfs_info *myfs, fs_off_t start, fs_off_t num_blocks)
{
    int        i, n, len, bsize = myfs->dsb.block_size;
    char      *ptr;
    BitVector *bv;
    int        do_log_write = 0;   /* XXXdbg - revisit when journaling works */

    
    acquire_sem(myfs->bbm_sem);

    bv = myfs->bbm.bv;
    UnSetRangeBV(bv, start, num_blocks);

    bv->is_full = 0;
    
    /*
       calculate the block number of the bitmap block we just modified.
       the +1 accounts for the super block.
    */
    n   = (start / 8 / bsize) + 1;

    /*
      the number of blocks modified is the number of bits modified divided
      by the number of bits in a disk block.
    */
    len = (num_blocks / 8 / bsize) + 1;

    ptr = (char *)bv->bits + (((start / 8) / bsize) * bsize);

    for(i=0; i < len; i++, ptr += bsize) {
        if (do_log_write) {
            if (myfs_write_journal_entry(myfs, myfs->cur_je, n+i, ptr) != 1) {
                printf("error: bitmap free: failed to write back bitmap "
                       "block run %d:1!\n", n+i);
                release_sem(myfs->bbm_sem);
                return EINVAL;
            }
        } else if (write_blocks(myfs, n+i, ptr, 1) != 1) {
            printf("error: bitmap free:2: failed to write back bitmap "
                   "block @ block %d!\n", n);
            release_sem(myfs->bbm_sem);
            return EINVAL;
        }
    }

    myfs->dsb.used_blocks -= len;

    release_sem(myfs->bbm_sem);

    myfs->dsb.flags = MYFS_DIRTY;

    return 0;
}



int
myfs_check_blocks(myfs_info *myfs, fs_off_t start, fs_off_t len, int state)
{
    int        i;
    BitVector *bv;

    acquire_sem(myfs->bbm_sem);

    bv = myfs->bbm.bv;
    for(i=0; i < len; i++) {
        if ((state == 1 && !IsSetBV(bv, start + i)) ||
            (state == 0 && IsSetBV(bv, start + i))) {
            break;
        }
    }

    release_sem(myfs->bbm_sem);
 
    if (i != len) {
        return 0;
    }

    return 1;
}


static void
sanity_check_bitmap(myfs_info *myfs)
{
    int        i, j, k, *ptr;
    fs_off_t   used_blocks = 0;
    BitVector *bv;

    bv = myfs->bbm.bv;
    ptr = bv->bits;

    for(j=0; j < bv->numbits/(sizeof(int)*8); j++) {
        for(k=0; k < sizeof(chunk)*CHAR_BIT; k++)
            if (ptr[j] & (1 << k))
                used_blocks++;
    }

    if (myfs->dsb.used_blocks != used_blocks) {
        printf("*** super block sez %ld used blocks but it's really %ld\n",
               myfs->dsb.used_blocks, used_blocks);
        myfs->dsb.used_blocks = used_blocks;
    }
}
