/*
  This file doesn't contain anything currently.  You could fill it in
  with a journaling implementation if you wanted (I may do that at some
  point).  Other parts of the file system will have to be modified to
  support journaling when that is done.
  
  THIS CODE COPYRIGHT DOMINIC GIAMPAOLO.  NO WARRANTY IS EXPRESSED 
  OR IMPLIED.  YOU MAY USE THIS CODE AND FREELY DISTRIBUTE IT FOR
  NON-COMMERCIAL USE AS LONG AS THIS NOTICE REMAINS ATTACHED.

  FOR COMMERCIAL USE, CONTACT DOMINIC GIAMPAOLO (dbg@be.com).

  Dominic Giampaolo
  dbg@be.com
*/
#include "myfs.h"

typedef struct j_entry
{
    int junk;
} j_entry;


int
myfs_create_journal(myfs_info *myfs)
{
    return 0;
}


int
myfs_init_journal(myfs_info *myfs)
{
    return 0;
}


int
myfs_shutdown_journal(myfs_info *myfs)
{
    return 0;
}


j_entry *
myfs_create_journal_entry(myfs_info *myfs)
{
    return 0;
}


int
myfs_write_journal_entry(myfs_info *myfs, j_entry *jent,
                         fs_off_t block_addr, void *block)
{
    return 0;
}

int
myfs_end_journal_entry(myfs_info *myfs, j_entry *jent)
{
    return 0;
}


void
sync_journal(myfs_info *myfs)
{
}
