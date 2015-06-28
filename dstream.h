
int myfs_init_data_stream(myfs_info *myfs, myfs_inode *mi);
int myfs_read_data_stream(myfs_info *myfs, myfs_inode *mi,
                           fs_off_t pos, char *buf, size_t *len);
int myfs_write_data_stream(myfs_info *myfs, myfs_inode *mi,
                           fs_off_t pos, const char *buf, size_t *len);
int myfs_set_file_size(myfs_info *myfs, myfs_inode *mi, fs_off_t new_size);
int myfs_free_data_stream(myfs_info *myfs, myfs_inode *mi);
