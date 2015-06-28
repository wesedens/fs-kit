/*
  This file contains a simple file system "shell" that lets you
  manipulate a file system.  There is a simple command table that
  contains the available functions.  It is very easy to extend.
  
  
  THIS CODE COPYRIGHT DOMINIC GIAMPAOLO.  NO WARRANTY IS EXPRESSED 
  OR IMPLIED.  YOU MAY USE THIS CODE AND FREELY DISTRIBUTE IT FOR
  NON-COMMERCIAL USE AS LONG AS THIS NOTICE REMAINS ATTACHED.

  FOR COMMERCIAL USE, CONTACT DOMINIC GIAMPAOLO (dbg@be.com).

  Dominic Giampaolo
  dbg@be.com
*/
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>

#include "myfs.h"
#include "kprotos.h"
#include "argv.h"

static void do_fsh(void);

int
main(int argc, char **argv)
{
    int        seed;
    char      *disk_name = "big_file";
    myfs_info  *myfs;

    if (argv[1] != NULL && !isdigit(argv[1][0]))
        disk_name = argv[1];
    else if (argv[1] && isdigit(argv[1][0]))
        seed = strtoul(argv[1], NULL, 0);
    else
        seed = getpid() * time(NULL) | 1;
    printf("random seed == 0x%x\n", seed);

    srand(seed);

    myfs = init_fs(disk_name);

    do_fsh();


    if (sys_unmount(1, -1, "/myfs") != 0) {
        printf("could not un-mount /myfs\n");
        return 5;
    }

    shutdown_block_cache();

    return 0;   
}


static void
make_random_name(char *buf, int len)
{
    int i, max = (rand() % (len - 5 - 3)) + 2;

    strcpy(buf, "/myfs/");
    for(i=0; i < max; i++) {
        buf[i+6] = 'a' + (rand() % 26);
    }

    buf[i] = '\0';
}


static void
SubTime(struct timeval *a, struct timeval *b, struct timeval *c)
{
  if ((long)(a->tv_usec - b->tv_usec) < 0)
   {
     a->tv_sec--;
     a->tv_usec += 1000000;
   }

  c->tv_sec  = a->tv_sec  - b->tv_sec;
  c->tv_usec = a->tv_usec - b->tv_usec;
}



int cur_fd = -1;


static void
do_close(int argc, char **argv)
{
    int err;

    err = sys_close(1, cur_fd);
/*  printf("close of fd %d returned: %d\n", cur_fd, err); */
    cur_fd = -1;
}

static void
do_open(int argc, char **argv)
{
    int  err;
    char name[64], buff[64];

    if (cur_fd >= 0)
        do_close(0, NULL);

    if (argc < 2)
        make_random_name(name, sizeof(name));
    else
        sprintf(name, "/myfs/%s", &argv[1][0]);

    cur_fd = sys_open(1, -1, name, O_RDWR, MY_S_IFREG, 0);
    if (cur_fd < 0)
        printf("error opening %s : %s (%d)\n", name, strerror(cur_fd), cur_fd);
    else
        printf("opened: %s\n", name);
}

static void
do_make(int argc, char **argv)
{
    int err;
    char name[64], buff[64];

    if (cur_fd >= 0)
        do_close(0, NULL);

    if (argc < 2) 
        make_random_name(name, sizeof(name));
    else if (argv[1][0] != '/')
        sprintf(name, "/myfs/%s", &argv[1][0]);
    else
        strcpy(name, &argv[1][0]);

    cur_fd = sys_open(1, -1, name, O_RDWR|O_CREAT, 0666, 0);
    if (cur_fd < 0) {
        printf("error creating: %s: %s\n", name, strerror(cur_fd));
        return;
    }

/*  printf("created: %s (fd %d)\n", name, cur_fd); */

}



static void
do_mkdir(int argc, char **argv)
{
    int err;
    char name[64], buff[64];

    if (argc < 2) 
        make_random_name(name, sizeof(name));
    else
        sprintf(name, "/myfs/%s", &argv[1][0]);

    err = sys_mkdir(1, -1, name, MY_S_IRWXU);
    if (err)
        printf("mkdir of %s returned: %s (%d)\n", name, strerror(err), err);
}

static void
do_read_test(int argc, char **argv)
{
    int    i, err;
    char   *buff;
    size_t len = 256;
    
    
    if (cur_fd < 0) {
        printf("no file open! (open or create one with open or make)\n");
        return;
    }

    if (argv[1][0] && isdigit(argv[1][0]))
        len = strtoul(&argv[1][0], NULL, 0);

    buff = malloc(len);
    if (buff == NULL) {
        printf("no memory for write buffer of %d bytes\n", len);
        return;
    }

    for(i=0; i < len; i++)
        buff[i] = (char)0xff;

    err = sys_read(1, cur_fd, buff, len);
    
    if (len < 512)
        hexdump(buff, len);
    else
        hexdump(buff, 512);

    free(buff);
    printf("read read %d bytes and returned %d\n", len, err);
}

static void
do_write_test(int argc, char **argv)
{
    int    i, err;
    char   *buff;
    size_t len = 256;
    
    if (cur_fd < 0) {
        printf("no file open! (open or create one with open or make)\n");
        return;
    }

    if (argv[1][0] && isdigit(argv[1][0]))
        len = strtoul(&argv[1][0], NULL, 0);

    buff = malloc(len);
    if (buff == NULL) {
        printf("no memory for write buffer of %d bytes\n", len);
        return;
    }

    for(i=0; i < len; i++)
        buff[i] = i;

    err = sys_write(1, cur_fd, buff, len);
    free(buff);
    
    printf("write wrote %d bytes and returned %d\n", len, err);
}

static void
mode_bits_to_str(int mode, char *str)
{
    int i;
    
    strcpy(str, "----------");

    if (MY_S_ISDIR(mode))
        str[0] = 'd';
    else if (MY_S_ISLNK(mode))
        str[0] = 'l';
    else if (MY_S_ISBLK(mode))
        str[0] = 'b';
    else if (MY_S_ISCHR(mode))
        str[0] = 'c';

    for(i=7; i > 0; i-=3, mode >>= 3) {
        if (mode & MY_S_IROTH)
            str[i]   = 'r';
        if (mode & MY_S_IWOTH)
            str[i+1] = 'w';
        if (mode & MY_S_IXOTH)
            str[i+2] = 'x';
    }
}

static void
do_dir(int argc, char **argv)
{
    int               dirfd, err, fd, max_err = 10;
    char              dirname[128], buff[512], time_buf[64] = { '\0', };
    size_t            len;
    struct my_dirent *dent;
    struct my_stat    st;
    struct tm        *tm;
    char              mode_str[16];
    
    dent = (struct my_dirent *)buff;

    strcpy(dirname, "/myfs/");
    if (argc > 1)
        strcat(dirname, &argv[1][0]);

    if ((dirfd = sys_opendir(1, -1, dirname, 0)) < 0) {
        printf("dir: error opening: %s\n", dirname);
        return;
    }

    printf("Directory listing for: %s\n", dirname);
    printf("      inode#  mode bits     uid    gid        size    "
           "Date      Name\n");
                  
    while(1) {
        len = 1;
        err = sys_readdir(1, dirfd, dent, sizeof(buff), len);
        if (err < 0) {
            printf("readdir failed for: %s\n", dent->d_name);
            if (max_err-- <= 0)
                break;
            
            continue;
        }
        
        if (err == 0)
            break;


        err = sys_rstat(1, dirfd, dent->d_name, &st, 1);
        if (err != 0) {
            printf("stat failed for: %s (%ld)\n", dent->d_name, dent->d_ino);
            if (max_err-- <= 0)
                break;
            
            continue;
        }

        tm = localtime(&st.mtime);
        strftime(time_buf, sizeof(time_buf), "%b %d %I:%M", tm);

        mode_bits_to_str(st.mode, mode_str);

        printf("%12ld %s %6d %6d %12ld %s %s\n", st.ino, mode_str,
               st.uid, st.gid, st.size, time_buf, dent->d_name);
    }

    if (err != 0) {
        printf("readdir failed on: %s\n", dent->d_name);
    }

    sys_closedir(1, dirfd);
}




static void
do_rmall(int argc, char **argv)
{
    int               dirfd, err, fd, max_err = 10;
    char              dirname[128], fname[512], buff[512];
    size_t            len;
    struct my_dirent *dent;
    struct my_stat    st;
    struct tm        *tm;
    
    dent = (struct my_dirent *)buff;

    strcpy(dirname, "/myfs/");
    if (argc > 1)
        strcat(dirname, &argv[1][0]);

    if ((dirfd = sys_opendir(1, -1, dirname, 0)) < 0) {
        printf("dir: error opening: %s\n", dirname);
        return;
    }

    while(1) {
        len = 1;
        err = sys_readdir(1, dirfd, dent, sizeof(buff), len);
        if (err < 0) {
            printf("readdir failed for: %s\n", dent->d_name);
            if (max_err-- <= 0)
                break;
            
            continue;
        }
        
        if (err == 0)
            break;


        if (strcmp(dent->d_name, "..") == 0 || strcmp(dent->d_name, ".") == 0)
            continue;

        sprintf(fname, "%s/%s", dirname, dent->d_name);
        err = sys_unlink(1, -1, fname);
        if (err != 0) {
            printf("unlink failed for: %s (%ld)\n", fname, dent->d_ino);
        }
    }

    if (err != 0) {
        printf("readdir failed on: %s\n", dent->d_name);
    }

    sys_closedir(1, dirfd);
}



static void
do_trunc(int argc, char **argv)
{
    int             err, new_size;
    char            fname[256];
    struct my_stat  st;
    

    strcpy(fname, "/myfs/");
    if (argc < 2) {
        printf("usage: trunc fname newsize\n");
        return;
    }
    strcat(fname, &argv[1][0]);
    new_size = strtoul(&argv[2][0], NULL, 0);

    st.size = new_size;
    err = sys_wstat(1, -1, fname, &st, WSTAT_SIZE, 0);
    if (err != 0) {
        printf("truncate to %d bytes failed for %s\n", new_size, fname);
    }
}




static void
do_seek(int argc, char **argv)
{
    fs_off_t err;
    fs_off_t pos;

    if (cur_fd < 0) {
        printf("no file open! (open or create one with open or make)\n");
        return;
    }

    
    if (argc < 2) {
        printf("usage: seek pos\n");
        return;
    }
    pos = strtoul(&argv[1][0], NULL, 0);

    err = sys_lseek(1, cur_fd, pos, SEEK_SET);
    if (err != pos) {
        printf("seek to %ld failed (%ld)\n", pos, err);
    }
}


static void
do_rm(int argc, char **argv)
{
    int err;
    char name[256], buff[256];

    if (cur_fd >= 0)
        do_close(0, NULL);

    if (argc < 2) {
        printf("rm: need a file name to remove\n");
        return;
    }

    sprintf(name, "/myfs/%s", &argv[1][0]);
    
    err = sys_unlink(1, -1, name);
    if (err != 0) {
        printf("error removing: %s: %s\n", name, strerror(err));
        return;
    }
}


static void
do_rmdir(int argc, char **argv)
{
    int err;
    char name[256], buff[256];

    if (cur_fd >= 0)
        do_close(0, NULL);

    if (argc < 2) {
        printf("rm: need a file name to remove\n");
        return;
    }

    sprintf(name, "/myfs/%s", &argv[1][0]);
    
    err = sys_rmdir(1, -1, name);
    if (err != 0) {
        printf("rmdir: error removing: %s: %s\n", name, strerror(err));
        return;
    }
}


static void
do_copy_to_myfs(char *host_file, char *bfile)
{
    int     bfd, err = 0;
    char    myfs_name[128];
    FILE   *fp;
    size_t  amt;
    static char buff[4 * 1024];

    sprintf(myfs_name, "/myfs/%s", bfile);

    fp = fopen(host_file, "rb");
    if (fp == NULL) {
        printf("can't open host file: %s\n", host_file);
        return;
    }
        
    if ((bfd = sys_open(1, -1, myfs_name, O_RDWR|O_CREAT,
                        MY_S_IFREG|MY_S_IRWXU, 0)) < 0) {
        fclose(fp);
        printf("error opening: %s\n", myfs_name);
        return;
    }
    
    while((amt = fread(buff, 1, sizeof(buff), fp)) == sizeof(buff)) {
        err = sys_write(1, bfd, buff, amt);
        if (err < 0)
            break;
    }

    if (amt && err >= 0) {
        err = sys_write(1, bfd, buff, amt);
    }

    if (err < 0) {
        printf("err == %d, amt == %d\n", err, amt);
        perror("write error");
    }

    sys_close(1, bfd);
    fclose(fp);
}


static void
do_copy_from_myfs(char *bfile, char *host_file)
{
    int     bfd,err = 0;
    char    myfs_name[128];
    FILE   *fp;
    size_t  amt;
    static char buff[4 * 1024];

    sprintf(myfs_name, "/myfs/%s", bfile);

    fp = fopen(host_file, "wb");
    if (fp == NULL) {
        printf("can't open host file: %s\n", host_file);
        return;
    }
        
    if ((bfd = sys_open(1, -1, myfs_name, O_RDONLY, MY_S_IFREG, 0)) < 0) {
        fclose(fp);
        printf("error opening: %s\n", myfs_name);
        return;
    }
    
    while(1) {
        amt = sizeof(buff);
        err = sys_read(1, bfd, buff, amt);
        if (err < 0)
            break;

        if (fwrite(buff, 1, err, fp) != amt)
            break;
    }


    if (err < 0)
        perror("read error");

    sys_close(1, bfd);
    fclose(fp);
}


static void
do_copy(int argc, char **argv)
{
    char name1[128], name2[128];

    if (cur_fd >= 0)
        do_close(0, NULL);

    if (argc < 3) {
        printf("copy needs two arguments!\n");
        return;
    }
    
    if (argv[1][0] == ':' && argv[2][0] != ':') {
        do_copy_to_myfs(&argv[1][1], &argv[2][0]);
        return;
    }

    if (argv[2][0] == ':' && argv[1][0] != ':') {
        do_copy_from_myfs(&argv[1][0], &argv[2][1]);
        return;
    }

    printf("can't copy around inside of the file system (only in and out)\n");
}


static void
do_rename(int argc, char **argv)
{
    int  err;
    char oldname[128], newname[128];

    if (cur_fd >= 0)
        do_close(0, NULL);

    if (argc < 3) {
        printf("rename needs two arguments!\n");
        return;
    }
    
    strcpy(oldname, "/myfs/");
    strcpy(newname, "/myfs/");

    strcat(oldname, &argv[1][0]);
    strcat(newname, &argv[2][0]);

    err = sys_rename(1, -1, oldname, -1, newname);
    if (err)
        printf("rename failed with err: %s\n", strerror(err));
}


static void
do_sync(int argc, char **argv)
{
    int err;
    
    err = sys_sync();
    
    if (err)
        printf("sync failed with err %s\n", strerror(err));
}

#define MAX_ITER  512
#define NUM_READS 16
#define READ_SIZE 4096

static void
do_cio(int argc, char **argv)
{
    int            i, j, fd, err;
    char           fname[64];
    fs_off_t       pos;
    size_t         len;
    static char    buff[READ_SIZE];
    struct timeval start, end, result;
    
    strcpy(fname, "/myfs/");
    if (argc == 1)
        strcpy(fname, "/myfs/fsh");
    else
        strcat(fname, &argv[1][0]);
    
    fd = sys_open(1, -1, fname, O_RDONLY, MY_S_IFREG, 0);
    if (fd < 0) {
        printf("can't open %s\n", fname);
        return;
    }

    gettimeofday(&start, NULL);

    for(i=0; i < MAX_ITER; i++) {
        for(j=0; j < NUM_READS; j++) {
            len = sizeof(buff);
            if (sys_read(1, fd, buff, len) != len) {
                perror("cio read");
                break;
            }
        }

        pos = 0;
        if (sys_lseek(1, fd, pos, SEEK_SET) != pos) {
            perror("cio lseek");
            break;
        }
    }

    gettimeofday(&end, NULL);
    SubTime(&end, &start, &result);
        
    printf("read %d bytes in %2ld.%.6ld seconds\n",
           (MAX_ITER * NUM_READS * sizeof(buff)) / 1024,
           result.tv_sec, result.tv_usec);


    sys_close(1, fd);
}



static void
mkfile(char *s, int sz)
{
    int  fd, len;
    char buf[16*1024];

    if ((fd = sys_open(1, -1, s, O_RDWR|O_CREAT,
                       MY_S_IFREG|MY_S_IRWXU, 0)) < 0) {
        printf("error creating: %s\n", s);
        return;
    }


    len = sz;
    if (sz) {
        if (sys_write(1, fd, buf, len) != len)
            printf("error writing %d bytes to %s\n", sz, s);
    }
    if (sys_close(1, fd) != 0)
        printf("close failed?\n");
}

#define LAT_FS_ITER   1000


static void
do_lat_fs(int argc, char **argv)
{
    int  i, j, iter;
/*  int  sizes[] = { 0, 1024, 4096, 10*1024 }; */
    int  sizes[] = { 0, 1024 };
    char name[64];

    iter = LAT_FS_ITER;

    if (argc > 1) 
        iter = strtoul(&argv[1][0], NULL, 0);

    for (i = 0; i < sizeof(sizes)/sizeof(int); ++i) {
        printf("CREATING: %d files of %5d bytes each\n", iter, sizes[i]);
        for (j = 0; j < iter; ++j) {
            sprintf(name, "/myfs/%.5d", j);
            mkfile(name, sizes[i]);
        }

        printf("DELETING: %d files of %5d bytes each\n", iter, sizes[i]);
        for (j = 0; j < iter; ++j) {
            sprintf(name, "/myfs/%.5d", j);
            if (sys_unlink(1, -1, name) != 0)
                printf("lat_fs: failed to remove: %s\n", name);
        }
    }
}




static void
do_create(int argc, char **argv)
{
    int  i, j, iter = 100, err;
    int  size = 0;
    char name[64];

    sprintf(name, "/myfs/test");
    err = sys_mkdir(1, -1, name, MY_S_IRWXU);
    if (err && err != EEXIST)
        printf("mkdir of %s returned: %s (%d)\n", name, strerror(err), err);

    if (argc > 1)
        iter = strtoul(&argv[1][0], NULL, 0);
    if (argc > 2)
        size = strtoul(&argv[2][0], NULL, 0);

    printf("creating %d files (each %d bytes long)...\n", iter, size);

    for (j = 0; j < iter; ++j) {
        sprintf(name, "/myfs/test/%.5d", j);
        /* printf("CREATING: %s (%5d)\n", name, size); */
        mkfile(name, size);
    }
}

static void
do_delete(int argc, char **argv)
{
    int  i, j, iter = 100;
    char name[64];

    if (argc > 1)
        iter =strtoul(&argv[1][0], NULL, 0);

    for (j = 0; j < iter; ++j) {
        sprintf(name, "/myfs/test/%.5d", j);
        printf("DELETING: %s\n", name);
        if (sys_unlink(1, -1, name) != 0)
            printf("lat_fs: failed to remove: %s\n", name);
    }
}



static void do_help(int argc, char **argv);


typedef struct cmd_entry {
    char *name;
    void  (*func)(int argc, char **argv);
    char *help;
} cmd_entry;

cmd_entry fsh_cmds[] =
{
    { "ls",      do_dir, "print a directory listing" },
    { "dir",     do_dir, "print a directory listing (same as ls)" },
    { "open",    do_open, "open an existing file for read/write access" },
    { "make",    do_make, "create a file (optionally specifying a name)" },
    { "close",   do_close, "close the currently open file" },
    { "mkdir",   do_mkdir, "create a directory" },
    { "rdtest",  do_read_test, "read N bytes from the current file. default is 256" },
    { "wrtest",  do_write_test, "write N bytes to the current file. default is 256"  },
    { "rm",      do_rm, "remove the named file" },
    { "rmall",   do_rmall, "remove all the files. if no dirname, use '.'" },
    { "rmdir",   do_rmdir, "remove the named directory" },
    { "cp",      do_copy, "copy a file to/from myfs. prefix a ':' for host filenames" },
    { "copy",    do_copy, "same as cp" },
    { "trunc",   do_trunc, "truncate a file to the size specified" },
    { "seek",    do_seek, "seek to the position specified" },
    { "mv",      do_rename, "rename a file or directory" },
    { "sync",    do_sync, "call sync" },
    { "lat_fs",  do_lat_fs, "simulate what the lmbench test lat_fs does" },
    { "create",  do_create, "create N files. default is 100" },
    { "delete",  do_delete, "delete N files. default is 100" },
    { "help",    do_help, "print this help message" },
    { "?",       do_help, "print this help message" },
    { NULL, NULL }
};


static void
do_help(int argc, char **argv)
{
    cmd_entry *cmd;

    printf("commands fsh understands:\n");
    for(cmd=fsh_cmds; cmd->name != NULL; cmd++) {
        printf("%8s - %s\n", cmd->name, cmd->help);
    }
}

static char *
getline(char *prompt, char *input, int len)
{
    printf("%s", prompt); fflush(stdout);

    return fgets(input, len, stdin);
}


static void
do_fsh(void)
{
    int   argc, len;
    char *prompt = "fsh>> ";
    char  input[512], **argv;
    cmd_entry *cmd;

    while(getline(prompt, input, sizeof(input)) != NULL) {
        argc = 0;
        argv = build_argv(input, &argc);
        if (argv == NULL || argc == 0) {
            continue;
        }

        len = strlen(&argv[0][0]);
        
        for(cmd=fsh_cmds; cmd->name != NULL; cmd++) {
            if (strncmp(cmd->name, &argv[0][0], len) == 0) {
                cmd->func(argc, argv);
                break;
            }
        }
        
        if (strncmp(&argv[0][0], "quit", 4) == 0)
            break;

        if (cmd->name == NULL && argv[0][0] != '\0')
            printf("command `%s' not understood\n", &argv[0][0]);
        
        free(argv);
    }

    if (feof(stdin))
        printf("\n");

    if (cur_fd != -1)
        do_close(0, NULL);
}




