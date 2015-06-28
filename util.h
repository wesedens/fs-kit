
int      init_tmp_blocks(myfs_info *myfs);
void     shutdown_tmp_blocks(myfs_info *myfs);

char    *get_tmp_blocks(myfs_info *myfs, int nblocks);
void     free_tmp_blocks(myfs_info *myfs, char *blocks, int nblocks);

int      get_device_block_size(int fd);
fs_off_t get_num_device_blocks(int fd);
int      device_is_read_only(const char *device);

int      myfs_die(const char *str, ...);
