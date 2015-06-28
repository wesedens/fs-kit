/*
  This file contains the bulk of the file system interface routines
  and all the routines that operate on files directly.
  
  
  THIS CODE COPYRIGHT DOMINIC GIAMPAOLO.  NO WARRANTY IS EXPRESSED 
  OR IMPLIED.  YOU MAY USE THIS CODE AND FREELY DISTRIBUTE IT FOR
  NON-COMMERCIAL USE AS LONG AS THIS NOTICE REMAINS ATTACHED.

  FOR COMMERCIAL USE, CONTACT DOMINIC GIAMPAOLO (dbg@be.com).

  Dominic Giampaolo
  dbg@be.com
*/
#include "myfs.h"


int
myfs_read_vnode(void *ns, vnode_id vnid, char r, void **node)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi;
    int         bsize = myfs->dsb.block_size, offset;
    char       *block;
    char        tmp[IDENT_NAME_LENGTH];
    fs_off_t    addr; 
    my_ino_t    ia = (my_ino_t)vnid;
    myfs_inode_etc *metc;
    
    mi = (myfs_inode *)malloc(sizeof(myfs_inode));
    if (mi == NULL)
        return ENOMEM;

    metc = (myfs_inode_etc *)calloc(1, sizeof(myfs_inode_etc));
    if (metc == NULL) {
        free(mi);
        return ENOMEM;
    }   

    addr = myfs->dsb.inodes_start + ((ia * sizeof(myfs_inode)) / bsize);
    offset = (ia % (bsize / sizeof(myfs_inode))) * sizeof(myfs_inode);

    block = get_block(myfs->fd, addr, bsize);
    if (block == NULL) {
        printf("couldn't read inode block at block #%ld", addr);
        return EINVAL;
    }
    
    memcpy(mi, &block[offset], sizeof(myfs_inode));

    release_block(myfs->fd, addr);

    mi->etc = metc;

    CHECK_INODE(mi);    /* make sure it's not corrupt */

    sprintf(tmp, "ino#%ld", ia);
    new_lock(&mi->etc->lock, tmp);

    /* only directories use this. it is read on demmand so clear it for now */
    mi->etc->contents = NULL;
    
    *node = mi;
    return 0;
}

int
myfs_release_vnode(void *ns, void *node, char r)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)node;

    CHECK_INODE(mi);

    free_lock(&mi->etc->lock);
    
    if (mi->etc->contents) {
        free(mi->etc->contents);
        mi->etc->contents = NULL;
    }

    free(mi->etc);
    mi->etc = NULL;
    
    free(mi);

    return 0;
}

int
myfs_remove_vnode(void *ns, void *node, char r)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)node;

    CHECK_INODE(mi);

    myfs_free_data_stream(myfs, mi);
    
    myfs_free_inode(myfs, mi->inode_num);

    if (mi->etc->contents)
        free(mi->etc->contents);
    mi->etc->contents = NULL;      /* paranoia, it'll destroy ya */
    
    free(mi->etc);
    mi->etc = NULL;
    
    free(mi);

    return 0;
}


int
myfs_access(void *ns, void *node, int mode)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)node;

    CHECK_INODE(mi);

    /* no permission checking yet... */

    return 0;
}


static int
make_file(myfs_info *myfs, myfs_inode *parent, int mode, myfs_inode **newfile)
{
    int         ret;
    size_t      len;
    myfs_inode *mi;

    mi = myfs_allocate_inode(myfs, parent, mode);
    if (mi == NULL)
        return ENOSPC;

    mi->mode |= MY_S_IFREG;            /* mark it as a regular file */

    update_inode(myfs, mi);

    *newfile = mi;

    return 0;
}


int
myfs_create(void *ns, void *dir, const char *name,
            int omode, int mode, vnode_id *vnid, void **cookie)
{
    int         ret, err;
    myfs_info  *myfs   = (myfs_info *)ns;
    myfs_inode *parent = (myfs_inode *)dir;
    myfs_inode *mi;

    CHECK_INODE(parent);

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return EINVAL;

    if (strlen(name) >= FILE_NAME_LENGTH-1)
        return ENAMETOOLONG;

    if (dir_lookup(myfs, dir, name, vnid) == 0)
        return EEXIST;

    ret = make_file(myfs, parent, mode, &mi);
    if (ret < 0)
        return ret;
    
    ret = dir_insert(myfs, parent, name, mi->inode_num);
    if (ret != 0) {
        printf("create: failed to insert the new guy %s\n", name);

        myfs_free_inode(myfs, mi->inode_num);
        free(mi);
        
        return ret;
    }

    *vnid   = (vnode_id)mi->inode_num;
    *cookie = NULL;

    if ((err = new_vnode(myfs->nsid, *vnid, mi)) != 0)
        myfs_die("new_vnode failed for vnid %ld: %s\n", *vnid, strerror(err));

    return 0;
    
}

int
myfs_symlink(void *ns, void *dir, const char *name, const char *path)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi;

    return EINVAL;
}

int
myfs_readlink(void *ns, void *node, char *buf, size_t *bufsize)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)node;

    CHECK_INODE(mi);

    return EINVAL;
}

int
myfs_link(void *ns, void *dir, const char *name, void *node)
{
    return EINVAL;
}


int
myfs_rename(void *ns, void *olddir, const char *oldname,
            void *newdir, const char *newname)
{
    return EINVAL;
}

int
myfs_unlink(void *ns, void *_dir, const char *name)
{
    int         ret;
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *dir  = (myfs_inode *)_dir;
    myfs_inode *mi;
    vnode_id    vnid;
    
    CHECK_INODE(dir);

    ret = dir_lookup(myfs, dir, name, &vnid);
    if (ret != 0)
        return ret;

    ret = get_vnode(myfs->nsid, vnid, (void *)&mi);
    if (ret != 0)
        return ret;
    
    if (MY_S_ISDIR(mi->mode)) {
        put_vnode(myfs->nsid, vnid);
        return EISDIR;
    }

    ret = dir_delete(myfs, dir, name);
    if (ret != 0)
        return ret;

    remove_vnode(myfs->nsid, vnid);
    put_vnode(myfs->nsid, vnid);

    return 0;
}


int
myfs_open(void *ns, void *node, int omode, void **cookie)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)node;

    CHECK_INODE(mi);

    return 0;
}

int
myfs_close(void *ns, void *node, void *cookie)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)node;

    CHECK_INODE(mi);

    return 0;
}

int
myfs_free_cookie(void *ns, void *node, void *cookie)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)node;

    CHECK_INODE(mi);

    return 0;
}

int
myfs_read(void *ns, void *node, void *cookie, fs_off_t pos, void *buf,
          size_t *len)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)node;

    CHECK_INODE(mi);

    return myfs_read_data_stream(myfs, mi, pos, buf, len);
}

int
myfs_write(void *ns, void *node, void *cookie, fs_off_t pos,
           const void *buf, size_t *len)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)node;

    CHECK_INODE(mi);

    return myfs_write_data_stream(myfs, mi, pos, buf, len);
}

int
myfs_ioctl(void *ns, void *node, void *cookie, int cmd, void *buf, size_t len)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)node;

    CHECK_INODE(mi);

    return EINVAL;
}


int
myfs_rstat(void *ns, void *node, struct my_stat *st)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)node;

    CHECK_INODE(mi);

    st->dev      = myfs->fd;
    st->ino      = mi->inode_num;
    st->mode     = mi->mode;
    st->nlink    = 1;
    st->uid      = mi->uid;
    st->gid      = mi->gid;
    st->size     = mi->data.size;
    st->blksize  = 1024;
    st->atime    = time(NULL);
    st->mtime    = mi->last_modified_time;
    st->ctime    = mi->last_modified_time;
    st->crtime   = mi->create_time;

    return 0;
}

int
myfs_wstat(void *ns, void *node, struct my_stat *st, long mask)
{
    int         err  = 0;
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)node;

    CHECK_INODE(mi);

    if (mask == 0)                /* check this first */
        return 0;
    
    if (mask & WSTAT_MODE) {
        mi->mode = (mi->mode & MY_S_IFMT) | (st->mode & MY_S_IUMSK);
    }

    if (mask & WSTAT_UID) {
        mi->uid = st->uid;
    }

    if (mask & WSTAT_GID) {
        mi->gid = st->gid;
    }

    if (mask & WSTAT_SIZE) {
        if (MY_S_ISDIR(mi->mode)) {
            err = EISDIR;
        } else {
            err = myfs_set_file_size(myfs, mi, st->size);
            mi->last_modified_time = time(NULL);
        }

        if (err)
            return err;
    }

    if (mask & WSTAT_CRTIME) {
        mi->create_time = st->crtime;
    }


    if (mask & WSTAT_MTIME) {
        mi->last_modified_time = st->mtime;
    }


    if (mask & WSTAT_ATIME) {
        /* hah! nothing to do because we're wanky and don't maintain atime */
    }

    update_inode(myfs, mi);
    return 0;
}

int
myfs_fsync(void *ns, void *node)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)node;

    CHECK_INODE(mi);

    return 0;
}

