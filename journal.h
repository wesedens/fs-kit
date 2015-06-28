struct j_entry;

int      myfs_create_journal(myfs_info *myfs);
int      myfs_init_journal(myfs_info *myfs);
int      myfs_shutdown_journal(myfs_info *myfs);
struct j_entry *myfs_create_journal_entry(myfs_info *myfs);
int      myfs_write_journal_entry(myfs_info *myfs, struct j_entry *jent,
                                  fs_off_t block_addr, void *block);
int      myfs_end_journal_entry(myfs_info *myfs, struct j_entry *jent);
void     sync_journal(myfs_info *myfs);
