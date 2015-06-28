
int myfs_read_vnode(void *ns, vnode_id vnid, char r, void **node);
int myfs_release_vnode(void *ns, void *node, char renter);
int myfs_remove_vnode(void *ns, void *node, char renter);
int myfs_walk(void *ns, void *base, const char *file,
              char **newpath, vnode_id *vnid);
int myfs_access(void *ns, void *node, int mode);
int myfs_create(void *ns, void *dir, const char *name,
                int perms, int omode, vnode_id *vnid, void **cookie);
int myfs_symlink(void *ns, void *dir, const char *name, const char *path);

int myfs_readlink(void *ns, void *node, char *buf, size_t *bufsize);
int myfs_link(void *ns, void *dir, const char *name, void *node);
int myfs_rename(void *ns, void *olddir, const char *oldname,
                void *newdir, const char *newname);
int myfs_unlink(void *ns, void *dir, const char *name);
int myfs_open(void *ns, void *node, int omode, void **cookie);
int myfs_close(void *ns, void *node, void *cookie);
int myfs_free_cookie(void *ns, void *node, void *cookie);
int myfs_read(void *ns, void *node, void *cookie, fs_off_t pos, void *buf,
              size_t *len);
int myfs_write(void *ns, void *node, void *cookie, fs_off_t pos,
               const void *buf, size_t *len);
int myfs_ioctl(void *ns, void *node, void *cookie, int cmd,
               void *buf, size_t len);
int myfs_rstat(void *ns, void *node, struct my_stat *st);
int myfs_wstat(void *ns, void *node, struct my_stat *st, long mask);
int myfs_fsync(void *ns, void *node);

