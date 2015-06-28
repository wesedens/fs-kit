
ssize_t read_blocks(myfs_info *myfs, fs_off_t block_num,
                    void *block, size_t nblocks);
ssize_t write_blocks(myfs_info *myfs, fs_off_t block_num, const
                     void *block, size_t nblocks);
int     read_super_block(myfs_info *myfs);
int     write_super_block(myfs_info *myfs);

