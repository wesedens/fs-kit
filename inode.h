int         myfs_create_inodes(myfs_info *myfs);
int         myfs_init_inodes(myfs_info *myfs);
void        myfs_shutdown_inodes(myfs_info *myfs);
myfs_inode *myfs_allocate_inode(myfs_info *myfs, myfs_inode *parent, int mode);
int         myfs_free_inode(myfs_info *myfs, inode_addr ia);
int         update_inode(myfs_info *myfs, myfs_inode *mi);

