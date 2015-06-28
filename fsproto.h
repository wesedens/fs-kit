#ifndef _FS_PROTO_H
#define _FS_PROTO_H

#include <sys/param.h>
#include "compat.h"

typedef unsigned long       nspace_id;
typedef struct nspace_info  nspace_info;
typedef long long           vnode_id;

struct nspace_info {
    vnode_id        root;
};

/*
 * PUBLIC PART OF THE FILE SYSTEM PROTOCOL
 */

struct fsinfo;


#define     WSTAT_MODE      0x0001
#define     WSTAT_UID       0x0002
#define     WSTAT_GID       0x0004
#define     WSTAT_SIZE      0x0008
#define     WSTAT_ATIME     0x0010
#define     WSTAT_MTIME     0x0020
#define     WSTAT_CRTIME    0x0040

#define     WFSSTAT_NAME    0x0001

typedef int op_read_vnode(void *ns, vnode_id vnid, char r, void **node);
typedef int op_release_vnode(void *ns, void *node, char r);
typedef int op_remove_vnode(void *ns, void *node, char r);

typedef int op_walk(void *ns, void *base, const char *file, char **newpath,
                    vnode_id *vnid);

typedef int op_access(void *ns, void *node, int mode);

typedef int op_create(void *ns, void *dir, const char *name,
                    int perms, int omode, vnode_id *vnid, void **cookie);
typedef int op_mkdir(void *ns, void *dir, const char *name, int perms);
typedef int op_symlink(void *ns, void *dir, const char *name,
                    const char *path);
typedef int op_link(void *ns, void *dir, const char *name, void *node);

typedef int op_rename(void *ns, void *olddir, const char *oldname,
                    void *newdir, const char *newname);
typedef int op_unlink(void *ns, void *dir, const char *name);
typedef int op_rmdir(void *ns, void *dir, const char *name);

typedef int op_readlink(void *ns, void *node, char *buf, size_t *bufsize);

typedef int op_opendir(void *ns, void *node, void **cookie);
typedef int op_closedir(void *ns, void *node, void *cookie);
typedef int op_rewinddir(void *ns, void *node, void *cookie);
typedef int op_readdir(void *ns, void *node, void *cookie, long *num,
                    struct my_dirent *buf, size_t bufsize);

typedef int op_open(void *ns, void *node, int omode, void **cookie);
typedef int op_close(void *ns, void *node, void *cookie);
typedef int op_free_cookie(void *ns, void *node, void *cookie);
typedef int op_read(void *ns, void *node, void *cookie, fs_off_t pos, void *buf,
                    size_t *len);
typedef int op_write(void *ns, void *node, void *cookie, fs_off_t pos,
                    const void *buf, size_t *len);
typedef int op_ioctl(void *ns, void *node, void *cookie, int cmd, void *buf,
                    size_t len);

typedef int op_rstat(void *ns, void *node, struct my_stat *);
typedef int op_wstat(void *ns, void *node, struct my_stat *, long mask);
typedef int op_fsync(void *ns, void *node);

typedef int op_mount(nspace_id nsid, const char *devname, unsigned long flags,
                    void *parms, size_t len, void **data, vnode_id *vnid);
typedef int op_unmount(void *ns);
typedef int op_sync(void *ns);

typedef struct vnode_ops {
    op_read_vnode           (*read_vnode);
    op_release_vnode        (*release_vnode);
    op_remove_vnode         (*remove_vnode);
    op_walk                 (*walk);
    op_access               (*access);
    op_create               (*create);
    op_mkdir                (*mkdir);
    op_symlink              (*symlink);
    op_link                 (*link);
    op_rename               (*rename);
    op_unlink               (*unlink);
    op_rmdir                (*rmdir);
    op_readlink             (*readlink);
    op_opendir              (*opendir);
    op_closedir             (*closedir);
    op_free_cookie          (*free_dircookie);
    op_rewinddir            (*rewinddir);
    op_readdir              (*readdir);
    op_open                 (*open);
    op_close                (*close);
    op_free_cookie          (*free_cookie);
    op_read                 (*read);
    op_write                (*write);
    op_ioctl                (*ioctl);
    op_rstat                (*rstat);
    op_wstat                (*wstat);
    op_fsync                (*fsync);
    op_mount                (*mount);
    op_unmount              (*unmount);
    op_sync                 (*sync);
} vnode_ops;

extern int      new_path(const char *path, char **copy);
extern void     free_path(char *p);

extern int      notify_listener(int op, nspace_id nsid, vnode_id vnida,
                                vnode_id vnidb, vnode_id vnidc, const char *name);
extern int      get_vnode(nspace_id nsid, vnode_id vnid, void **data);
extern int      put_vnode(nspace_id nsid, vnode_id vnid);
extern int      new_vnode(nspace_id nsid, vnode_id vnid, void *data);
extern int      remove_vnode(nspace_id nsid, vnode_id vnid);
extern int      unremove_vnode(nspace_id nsid, vnode_id vnid);
extern int      is_vnode_removed(nspace_id nsid, vnode_id vnid);


#endif /* _FS_PROTO_H */
