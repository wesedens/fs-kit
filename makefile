TARGETS = makefs fsh tstfs

all : $(TARGETS)

#
# change the -O7 to -O3 if your compiler doesn't grok -O7
#
CFLAGS = -DUSER=1 -O7

SUPPORT_OBJS = rootfs.o initfs.o kernel.o cache.o sl.o stub.o
MISC_OBJS    = sysdep.o util.o hexdump.o argv.o

FS_OBJS = mount.o bitmap.o journal.o inode.o dstream.o dir.o \
          file.o io.o bitvector.o


fsh : fsh.o $(FS_OBJS) $(SUPPORT_OBJS) $(MISC_OBJS)
	cc -o $@ fsh.o $(FS_OBJS) $(SUPPORT_OBJS) $(MISC_OBJS)

tstfs : tstfs.o $(FS_OBJS) $(SUPPORT_OBJS) $(MISC_OBJS)
	cc -o $@ tstfs.o $(FS_OBJS) $(SUPPORT_OBJS) $(MISC_OBJS)

makefs : makefs.o $(FS_OBJS) $(SUPPORT_OBJS) $(MISC_OBJS)
	cc -o $@ makefs.o $(FS_OBJS) $(SUPPORT_OBJS) $(MISC_OBJS)


.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<


makefs.o : makefs.c myfs.h
fsh.o    : fsh.c myfs.h
tstfs.o  : tstfs.c myfs.h


mount.o     : mount.c myfs.h
journal.o   : journal.c myfs.h
bitmap.o    : bitmap.c myfs.h
inode.o     : inode.c myfs.h
dstream.o   : dstream.c myfs.h
dir.o       : dir.c myfs.h
file.o      : file.c myfs.h 
bitvector.o : bitvector.c bitvector.h 
util.o      : util.c myfs.h

myfs.h : compat.h cache.h lock.h mount.h bitmap.h journal.h inode.h file.h \
         dir.h dstream.h io.h util.h fsproto.h bitvector.h

sysdep.o : sysdep.c compat.h 
kernel.o : kernel.c compat.h fsproto.h kprotos.h
rootfs.o : compat.h fsproto.h
initfs.o : initfs.c compat.h fsproto.h myfs_vnops.h
sl.o     : sl.c skiplist.h
cache.o  : cache.c cache.h compat.h
stub.o   : stub.c compat.h

clean:
	rm -f *.o $(TARGETS)
