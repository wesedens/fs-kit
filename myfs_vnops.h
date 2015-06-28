#include "myfs.h"

vnode_ops myfs_ops =  {
      &myfs_read_vnode,
      &myfs_release_vnode,
      &myfs_remove_vnode,
      &myfs_walk,
      &myfs_access,
      &myfs_create,
      &myfs_mkdir,
      &myfs_symlink,
      NULL,                   /* link */
      &myfs_rename,
      &myfs_unlink,
      &myfs_rmdir,
      &myfs_readlink,

      &myfs_opendir,
      &myfs_closedir,
      &myfs_free_dircookie,
      &myfs_rewinddir,
      &myfs_readdir,

      &myfs_open,
      &myfs_close,
      &myfs_free_cookie,
      &myfs_read,
      &myfs_write,
      &myfs_ioctl,
      &myfs_rstat,
      &myfs_wstat,
      &myfs_fsync,
      &myfs_mount,
      &myfs_unmount,
      NULL                    /* sync */
};
