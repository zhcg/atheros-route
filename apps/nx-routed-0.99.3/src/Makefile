# $Id: //depot/sw/releases/Aquila_9.2.0_U11/apps/nx-routed-0.99.3/src/Makefile#1 $
CC=$(TOOLPREFIX)gcc
LD=$(TOOLPREFIX)ld
AS=$(TOOLPREFIX)as
AR=$(TOOLPREFIX)ar
STRIP=$(TOOLPREFIX)strip
CFLAGS=-DNORMALUNIX -DLINUX -Os -O3 -Wall -s 
LIBS = -lnsl 
INCLUDES=-I../include
OUTDIR=../bin
OBJS= main.o ctlfile.o rip2.o router.o socket.o timers.o sys_linux.o link.o util.o conf.o acl.o netlink.o
PROG=nx-routed

all: $(PROG)

$(PROG): $(OBJS)
	@echo "Linking $@"
	@$(CC) $(OBJS) -o $@ $(CFLAGS) $(LIBS)
#	@mv $(PROG) $(OUTDIR)

clean:
	rm -rf $(PROG) $(OBJS)

%.o: %.cpp
	@echo "Compiling  $<"
	@$(CPP) $(CFLAGS) $(INCLUDES) -c -o $(basename $<).o $<

%.o: %.c
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c -o $(basename $<).o $<
