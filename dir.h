int myfs_create_root_dir(myfs_info *myfs);
int myfs_mkdir(void *ns, void *dir, const char *name, int perms);
int myfs_rmdir(void *ns, void *dir, const char *name);
int myfs_opendir(void *ns, void *node, void **cookie);
int myfs_closedir(void *ns, void *node, void *cookie);
int myfs_free_dircookie(void *ns, void *node, void *cookie);
int myfs_rewinddir(void *ns, void *node, void *cookie);
int myfs_readdir(void *ns, void *node, void *cookie, long *num,
                 struct my_dirent *buf, size_t bufsize);
int myfs_walk(void *ns, void *base, const char *file, char **newpath,
              vnode_id *vnid);

int dir_lookup(myfs_info *myfs, myfs_inode *dir, const char *name, vnode_id *vnid);
int dir_delete(myfs_info *myfs, myfs_inode *dir, const char *name);
int dir_insert(myfs_info *myfs, myfs_inode *dir, const char *name,
               inode_addr inum);

