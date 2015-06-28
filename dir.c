/*
  This file contains the directory manipulation routines.
  
  THIS CODE COPYRIGHT DOMINIC GIAMPAOLO.  NO WARRANTY IS EXPRESSED 
  OR IMPLIED.  YOU MAY USE THIS CODE AND FREELY DISTRIBUTE IT FOR
  NON-COMMERCIAL USE AS LONG AS THIS NOTICE REMAINS ATTACHED.

  FOR COMMERCIAL USE, CONTACT DOMINIC GIAMPAOLO (dbg@be.com).

  Dominic Giampaolo
  dbg@be.com
*/
#include "myfs.h"


/*
  this ugly macro advances a myfs_dirent pointer up to the next
  structure.  it is complicated by the fact that we round up to
  an address that is a multiple of 8 (to avoid alignment woes).
*/
#define NEXT_MDE(mde) (void *)((((ulong)&mde->name[mde->name_len+1]) + 7) & ~7)

#define MDE_SIZE(mde) (ulong)(((char *)NEXT_MDE(mde)) - (char *)mde)

/*
  this is the number of bytes needed to hold two dirent structs for
  "." and ".." (which every directory has to have).  the +1 is for
  the name "." and the +2 is for the name "..". we go for 16 byte
  alignment on this guy because there are two dirents in it.
*/  
#define MKDIR_BUFSIZE  (((2*sizeof(myfs_dirent) + 1 + 2) + 15) & ~15)


static int
make_dir(myfs_info *myfs, myfs_inode *parent, int mode, myfs_inode **newdir)
{
    int          ret;
    size_t       len;
    myfs_inode  *mi;
    char         buff[MKDIR_BUFSIZE + 8];
    myfs_dirent *mde;
    myfs_dirent *start;

    /* force mde to start on an 8-byte boundary */
    start = mde = (myfs_dirent *)(((ulong)&buff[0] + 7) & ~7);

    mi = myfs_allocate_inode(myfs, parent, mode);
    if (mi == NULL)
        return ENOSPC;

    mi->mode |= MY_S_IFDIR;            /* mark it as a directory */

    mde->inum     = mi->inode_num;
    mde->name_len = 1;
    strcpy(&mde->name[0], ".");

    mde = NEXT_MDE(mde);
    if (parent)
        mde->inum = parent->inode_num;
    else
        mde->inum = mi->inode_num;
    mde->name_len = 2;
    strcpy(&mde->name[0], "..");

    mde = NEXT_MDE(mde);   /* so the size calculation works out right */
    
    len = (size_t)((ulong)mde - (ulong)start);
    ret = myfs_write_data_stream(myfs, mi, 0, (const char *)start, &len);
    if (ret != 0 || len != ((ulong)mde - (ulong)start)) {
        myfs_free_inode(myfs, mi->inode_num);
        if (ret == 0)
            ret = ENOSPC;
        return ret;
    }

    update_inode(myfs, mi);

    *newdir = mi;

    return 0;
}

int
myfs_create_root_dir(myfs_info *myfs)
{
    int         ret;
    myfs_inode *mi;

    ret = make_dir(myfs, NULL, 0777, &mi);
    if (ret < 0)
        return ret;

    myfs->dsb.root_inum = mi->inode_num;
    myfs->root_dir      = mi;

    return 0;
}


static int
get_dir_contents(myfs_info *myfs, myfs_inode *mi)
{
    int    ret;
    size_t sz;
    
    mi->etc->contents = (char *)malloc(mi->data.size);
    if (mi->etc->contents == NULL) {
        return ENOMEM;
    }

if ((ulong)mi->etc->contents & 7)
    printf("malloc returns 4-byte aligned data? (0x%x)\n", mi->etc->contents);

    sz = (size_t)mi->data.size;
    ret = myfs_read_data_stream(myfs, mi, 0, mi->etc->contents, &sz);
    if (sz != mi->data.size) {
        free(mi->etc->contents);
        mi->etc->contents = NULL;
        return ret;
    }

    return 0;
}

int
dir_lookup(myfs_info *myfs, myfs_inode *dir, const char *name, vnode_id *vnid)
{
    int ret;
    myfs_dirent *mde, *end;
        
    if (dir->etc->contents == NULL) {
        ret = get_dir_contents(myfs, dir);
        if (ret != 0)
            return ret;
    }

    mde = (myfs_dirent *)dir->etc->contents;
    end = (myfs_dirent *)(dir->etc->contents + dir->data.size);

    for(; mde < end; mde=NEXT_MDE(mde)) {
        if (strcmp(mde->name, name) == 0) {
            *vnid = (vnode_id)mde->inum;
            return 0;
        }
    }

    return ENOENT;
}


int
dir_insert(myfs_info *myfs, myfs_inode *dir, const char *name, inode_addr inum)
{
    int          ret;
    size_t       sz, osize;
    char         buff[FILE_NAME_LENGTH + sizeof(myfs_dirent) + 8], *ptr;
    myfs_dirent *mde;

    mde = (myfs_dirent *)(((ulong)&buff[0] + 7) & ~7);
    
    if (dir->etc->contents == NULL) {
        ret = get_dir_contents(myfs, dir);
        if (ret != 0)
            return ret;
    }

    mde->inum     = inum;
    mde->name_len = (uchar)strlen(name);
    strcpy(&mde->name[0], name);

    sz = MDE_SIZE(mde);
    
    ptr = realloc(dir->etc->contents, dir->data.size + sz);
    if (ptr == NULL)
        return ENOMEM;

    dir->etc->contents = ptr;
    dir->etc->counter++;

    memcpy(ptr+dir->data.size, mde, sz);
    
    osize = sz = dir->data.size + sz;
    ret = myfs_write_data_stream(myfs, dir, 0, ptr, &sz);
    if (ret != 0 || sz != osize)       /* damn. no space for the new guy */
        return ret;

    return 0;
}


int
dir_delete(myfs_info *myfs, myfs_inode *dir, const char *name)
{
    int          ret;
    size_t       sz, nsize;
    char         buff[FILE_NAME_LENGTH + sizeof(myfs_dirent) + 8], *ptr;
    myfs_dirent *mde;
    myfs_dirent *next;
    myfs_dirent *end;
    
    mde = (myfs_dirent *)(((ulong)&buff[0] + 7) & ~7);

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return EINVAL;

    if (dir->etc->contents == NULL) {
        ret = get_dir_contents(myfs, dir);
        if (ret != 0)
            return ret;
    }

    mde = (myfs_dirent *)dir->etc->contents;
    end = (myfs_dirent *)(dir->etc->contents + dir->data.size);

    for(; mde < end; mde=NEXT_MDE(mde)) {
        if (strcmp(mde->name, name) == 0) {
            break;
        }
    }
    
    if (mde >= end)
        return ENOENT;

    sz   = MDE_SIZE(mde);
    next = NEXT_MDE(mde);
    if (next < end)
        memcpy(mde, next, ((ulong)end - (ulong)next));

    ptr = realloc(dir->etc->contents, dir->data.size - sz);
    if (ptr == NULL)
        return ENOMEM;

    dir->etc->contents = ptr;
    dir->etc->counter++;

    nsize = dir->data.size - sz;

    ret = myfs_write_data_stream(myfs, dir, 0, ptr, &nsize);
    if (ret != 0 || nsize != (dir->data.size - sz))
        return ret;

    myfs_set_file_size(myfs, dir, nsize);

    return 0;
}


int
myfs_walk(void *ns, void *base, const char *file, char **newpath,
          vnode_id *vnid)
{
    int         ret;
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *dir  = (myfs_inode *)base;
    myfs_inode *mi;
    
    ret = dir_lookup(myfs, dir, file, vnid);
    if (ret != 0) {
printf("did not find name %s\n", file);
        return ret;
    }

    mi = NULL;
    if (get_vnode(myfs->nsid, *vnid, (void *)&mi) != 0)
        return ENOENT;

    return 0;
}


int
myfs_mkdir(void *ns, void *parent_dir, const char *name, int mode)
{
    int         ret;
    vnode_id    vnid;
    myfs_info  *myfs   = (myfs_info *)ns;
    myfs_inode *parent = (myfs_inode *)parent_dir;
    myfs_inode *mi;

    CHECK_INODE(parent);

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return EINVAL;

    if (strlen(name) >= FILE_NAME_LENGTH-1)
        return ENAMETOOLONG;

    if (dir_lookup(myfs, parent, name, &vnid) == 0)
        return EEXIST;

    ret = make_dir(myfs, parent, mode, &mi);
    if (ret < 0)
        return ret;
    
    ret = dir_insert(myfs, parent, name, mi->inode_num);
    if (ret != 0) {
        printf("mkdir: failed to insert the new guy %s\n", name);

        myfs_free_inode(myfs, mi->inode_num);
    }

    /* free this stuff because we didn't call new_vnode() on it */
    free_lock(&mi->etc->lock);
    free(mi->etc);
    mi->etc = NULL;
    
    free(mi);
        
    return ret;
}


int
myfs_rmdir(void *ns, void *_dir, const char *name)
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
    
    if (MY_S_ISDIR(mi->mode) == 0) {
        put_vnode(myfs->nsid, vnid);
        return ENOTDIR;
    }

    if (mi->data.size > MKDIR_BUFSIZE) {  /* then it still has stuff in it */
        put_vnode(myfs->nsid, vnid);
        return ENOTEMPTY;
    }

    ret = dir_delete(myfs, dir, name);
    if (ret != 0)
        return ret;

    remove_vnode(myfs->nsid, vnid);
    put_vnode(myfs->nsid, vnid);

    return 0;
}


int
myfs_opendir(void *ns, void *dir, void **cookie)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)dir;
    dir_cookie *dc;
    size_t      ret, sz;

    CHECK_INODE(mi);

    dc = (dir_cookie *)malloc(sizeof(dir_cookie));
    if (dc == NULL)
        return ENOMEM;

    if (mi->etc->contents == NULL) {
        ret = get_dir_contents(myfs, mi);
        if (ret != 0) {
            free(dc);
            return ret;
        }
    }
    
    dc->curptr  = mi->etc->contents;
    dc->counter = mi->etc->counter;
    dc->index   = 0;

    *cookie = dc;
    return 0;
}

int
myfs_closedir(void *ns, void *dir, void *cookie)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)dir;

    CHECK_INODE(mi);

    return 0;
}

int
myfs_free_dircookie(void *ns, void *dir, void *cookie)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)dir;
    dir_cookie *dc   = (dir_cookie *)cookie;

    CHECK_INODE(mi);

    free(dc);

    return 0;
}

int
myfs_rewinddir(void *ns, void *dir, void *cookie)
{
    myfs_info  *myfs = (myfs_info *)ns;
    myfs_inode *mi   = (myfs_inode *)dir;
    dir_cookie *dc   = (dir_cookie *)cookie;

    CHECK_INODE(mi);

    dc->curptr  = mi->etc->contents;
    dc->counter = mi->etc->counter;
    dc->index   = 0;

    return 0;
}

int
myfs_readdir(void *ns, void *dir, void *cookie, long *num,
             struct my_dirent *buf, size_t bufsize)
{
    int          i;
    myfs_info   *myfs = (myfs_info *)ns;
    myfs_inode  *mi   = (myfs_inode *)dir;
    dir_cookie  *dc   = (dir_cookie *)cookie;
    myfs_dirent *mde, *end;

    CHECK_INODE(mi);

    if (mi->etc->contents == NULL) {     /* nothing in the directory */
        *num = 0;
        return 0;
    }

    if (dc->counter != mi->etc->counter) {      /* then we have to re-sync */
        dc->curptr = mi->etc->contents;
        mde = (myfs_dirent *)mi->etc->contents;
        end = (myfs_dirent *)((char *)mi->etc->contents + mi->data.size);

        for(i=0; i < dc->index-1 && mde < end; i++) {
            mde = NEXT_MDE(mde);
        }

        dc->curptr  = (char *)mde;
        dc->counter = mi->etc->counter;
        dc->index--;
    }

    /* check if we're at the end of the directory */
    if (dc->curptr >= (mi->etc->contents + mi->data.size)) { 
        *num = 0;
        return 0;
    }
    
    mde = (myfs_dirent *)dc->curptr;
    buf->d_dev    = myfs->fd;
    buf->d_ino    = mde->inum;
    buf->d_reclen = mde->name_len;
    memcpy(&buf->d_name[0], mde->name, mde->name_len+1);

    *num = 1;

    mde = NEXT_MDE(mde);

    dc->curptr = (char *)mde;
    dc->index++;
    
    return 0;
}
