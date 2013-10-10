# wpatalk -- talk to running wpa_supplicant or hostapd program
#
#

include ../common/full.config
include ../common/Makefile.common

LIBS += -lwpa_ctrl

ALL = wpatalk

all: $(ALL)
	echo All done for wpatalk

install: all
	cp -a $(ALL) $(INSTALL_ROOT)/sbin/.

# Uncomment to use readline (but makes program GPL only)
#CFLAGS += -DCONFIG_READLINE

OBJS = wpatalk.o

wpatalk: $(OBJS) $(COMMON_LIBS)
	$(CC) $(CFLAGS) -o wpatalk $(OBJS) $(LIBS)

clean:
	rm -f core junk* *~ *.o *.d *.a *.so $(ALL) $(WINALL)

-include $(OBJS:%.o=%.d)

