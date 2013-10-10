# initramfs -- tools and code to support the "initramfs" scheme,
# whereby files are placed into the "rootfs" aka "tmpfs" aka "ramfs"
# psuedo-filesystem, which actually means that they are held in the
# kernel file buffering system without any real file system.

all : inittrampoline geninitramfs

#############################################################################
# geninitramfs: a tool that makes cpio format archive files.
# This is used in preference to the standard "cpio" command because
# it has input options more suited for initramfs work.
# (cpio archives are used with initramfs because they are simpler
# than tar archives).
geninitramfs: geninitramfs.c
	gcc -Wall -Werror geninitramfs.c -o geninitramfs

#############################################################################
# inittrampoline: a very lightweight and self-contained program that
# can be included in the initramfs archive built into and seen by the kernel 
# and is executed as "/init", the first program spawned from the kernel...
# This program serves as a "trampoline" in that it loads an additional
# initramfs archive from an external source, not part of the kernel,
# then execs the traditional /sbin/init program.
#
# No C library shared object is available at the time /init runs...
# so we must staticly link (and take care in program to keep dependencies
# down).
LDFLAGS = -static
# When building for real target, STRIP should be pre-defined...
# we define it here as something harmless for building for host
# (which is useful only for 
STRIP ?= echo Do not strip
CFLAGS += -Wall -Werror -g
OBJS = inittrampoline.o
inittrampoline: $(OBJS)
	$(CC) $(LDFLAGS) $(CFLAGS) $(CPPFLAGS) -o inittrampoline $(OBJS) $(LIBS)
	$(NM) -n inittrampoline >inittrampoline.nm # for debugging
	$(STRIP) inittrampoline


#############################################################################
clean:
	rm -rf inittrampoline *.nm *.o *.d *.d.* junk* geninitramfs
