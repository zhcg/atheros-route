TOPDIR_PHONE = ../

include $(TOPDIR_PHONE)include.mk


CFLAGS += $(DEFS_Debug) -DSPI_FOR_PHONE

CFLAGS += -I$(TOPDIR_PHONE)

%.o: %.c

	$(CC) $(CFLAGS)  -c $< -o $@   

%.o: %.cc

	$(CXX) $(CFLAGS) -c $< -o $@   



SOURCES = $(wildcard *.c *.cc)
OBJS += $(patsubst %.c,%.o,$(patsubst %.cc,%.o,$(SOURCES)))
.PHONY: all clean
all: $(OBJS)
	@echo $(OBJS)

clean:
	rm -rf *.o a.out
