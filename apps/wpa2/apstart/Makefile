# apstart -- set up network interface per topology file.
#

include ../common/full.config
include ../common/Makefile.common

ALL = apstart

all: $(ALL)
	echo All done for apstart

install: all
	cp -a $(ALL) $(INSTALL_ROOT)/sbin/.

OBJS = apstart.o

apstart: $(OBJS) $(COMMON_LIBS)
	$(CC) $(CFLAGS) -o apstart $(OBJS) $(LIBS)

clean:
	rm -f core junk* *~ *.o *.d *.a *.so $(ALL) $(WINALL)

-include $(OBJS:%.o=%.d)

