/*
  This file contains some utility routines that may be useful if you
  need to get temporary blocks of storage in your file system.

  It also contains the myfs_die() routine which is a sort of general
  purpose panic type function that will halt the file system.
  
  THIS CODE COPYRIGHT DOMINIC GIAMPAOLO.  NO WARRANTY IS EXPRESSED 
  OR IMPLIED.  YOU MAY USE THIS CODE AND FREELY DISTRIBUTE IT FOR
  NON-COMMERCIAL USE AS LONG AS THIS NOTICE REMAINS ATTACHED.

  FOR COMMERCIAL USE, CONTACT DOMINIC GIAMPAOLO (dbg@be.com).

  Dominic Giampaolo
  dbg@be.com
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include "myfs.h"


int
init_tmp_blocks(myfs_info *myfs)
{
    int         i;
    tmp_blocks *tmp;
    
    myfs->tmp_blocks_sem = create_sem(1, "tmp_blocks_sem");
    if (myfs->tmp_blocks_sem < 0)
        return ENOMEM;


    myfs->tmp_blocks = (tmp_blocks *)malloc(sizeof(tmp_blocks));
    if (myfs->tmp_blocks == NULL)
        goto err1;

    tmp = myfs->tmp_blocks;
    tmp->next = NULL;
    
    tmp->data = (char *)malloc(NUM_TMP_BLOCKS * myfs->dsb.block_size);
    if (tmp->data == NULL)
        goto err2;

    for(i=0; i < NUM_TMP_BLOCKS; i++)
        tmp->block_ptrs[i] = tmp->data + (i * myfs->dsb.block_size);
        
    return 0;

 err2:
    free(myfs->tmp_blocks);
    myfs->tmp_blocks = NULL;
 err1:
    delete_sem(myfs->tmp_blocks_sem);
    myfs->tmp_blocks_sem = -1;
    
    return ENOMEM;
}


void
shutdown_tmp_blocks(myfs_info *myfs)
{
    int         i;
    tmp_blocks *tmp, *next;
    
    for(tmp=myfs->tmp_blocks; tmp; tmp=next) {
        for(i=0; i < NUM_TMP_BLOCKS; i++)
            if (tmp->block_ptrs[i] == NULL)
                printf("unfree'd tmp block @ 0x%lx index %d\n",
                       (ulong)(tmp->data + i * myfs->dsb.block_size), i);

        free(tmp->data);
        next = tmp->next;
        free(tmp);
    }

    if (myfs->tmp_blocks_sem > 0)
        delete_sem(myfs->tmp_blocks_sem);
    myfs->tmp_blocks_sem = -1;
}


char *
get_tmp_blocks(myfs_info *myfs, int nblocks)
{
    int         i, j, start;
    char       *blocks;
    tmp_blocks *tmp;

    if (nblocks > NUM_TMP_BLOCKS) {
        printf("*** error: requested %d tmp blocks but can only have %d\n",
               nblocks, NUM_TMP_BLOCKS);
        return NULL;
    }

    acquire_sem(myfs->tmp_blocks_sem);

    for(tmp=myfs->tmp_blocks; tmp; tmp=tmp->next) {
        blocks = NULL;
        start  = -999999;

        for(i=0; i < NUM_TMP_BLOCKS; i++) {
            if (blocks == NULL && tmp->block_ptrs[i]) {
                start = i;
                blocks = tmp->block_ptrs[i];
            }

            if (blocks && tmp->block_ptrs[i] != NULL &&
                ((i + 1) - start) == nblocks) {
                for(j=start; j <= i; j++) {
                    if (tmp->block_ptrs[j] == NULL) {
                        myfs_die("you fucking whore of the revelation\n");
                    }
                    tmp->block_ptrs[j] = NULL;
                }

                release_sem(myfs->tmp_blocks_sem);

                return blocks;
            } else if (tmp->block_ptrs[i] == NULL) {
                /* reset our pointers and keep on truckin' */
                blocks = NULL;
                start  = -9999999;
            }
        }
    }

    /*
       if we get here then we couldn't find any free space and we have
       to go allocate some with malloc.
    */   
    tmp = (tmp_blocks *)malloc(sizeof(tmp_blocks));
    if (tmp == NULL)
        return NULL;
    
    tmp->data = (void *)malloc(NUM_TMP_BLOCKS * myfs->dsb.block_size);
    if (tmp->data == NULL) {
        free(tmp);
        return NULL;
    }

    for(i=0; i < nblocks; i++)
        tmp->block_ptrs[i] = NULL;
    
    for(; i < NUM_TMP_BLOCKS; i++)
        tmp->block_ptrs[i] = tmp->data + (i * myfs->dsb.block_size);
        
    tmp->next = myfs->tmp_blocks;
    myfs->tmp_blocks = tmp;
    
    release_sem(myfs->tmp_blocks_sem);

    return tmp->data;
}


void
free_tmp_blocks(myfs_info *myfs, char *blocks, int nblocks)
{
    int         i, j;
    char       *end;
    tmp_blocks *tmp;

    if (blocks == NULL)
        myfs_die("free_tmp_blocks called w/null block ptr (%d)\n", nblocks);

    if (nblocks > NUM_TMP_BLOCKS)
        myfs_die("*** free_tmp_blocks: nblocks == %d but should be < %d\n",
                nblocks, NUM_TMP_BLOCKS);

    acquire_sem(myfs->tmp_blocks_sem);
    
    for(tmp=myfs->tmp_blocks; tmp; tmp=tmp->next) {
        end = tmp->data + NUM_TMP_BLOCKS * myfs->dsb.block_size;
        
        if (blocks >= tmp->data && blocks < end) {  /* we have a match! */
            i = (blocks - tmp->data) / myfs->dsb.block_size;

            for(j=0; j < nblocks; j++, blocks+=myfs->dsb.block_size) {
                if (tmp->block_ptrs[i + j] != NULL) {
                    myfs_die("free_tmp_blocks ptr 0x%x already free?!?\n",
                           tmp->block_ptrs[i + j]);
                    break;
                }
                
                tmp->block_ptrs[i + j] = blocks;
            }

            break;
        }
    }

    if (tmp == NULL)
        myfs_die("free_tmp_blocks: failed to free %d blocks @ 0x%x\n",
                nblocks, blocks);

    release_sem(myfs->tmp_blocks_sem);
}



int
myfs_die(const char *fmt, ...)
{
  va_list     ap;
  static char buff[512];

  printf("\n");
  va_start(ap, fmt);
  vsprintf(buff, fmt, ap);
  va_end(ap);

  printf("%s\n", buff);
  
  printf("spinning forever.\n");
  while(1)
      ;

  return 0;
}

