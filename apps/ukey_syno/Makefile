SRCS=$(wildcard *.c)
OBJS=$(patsubst %.c,%.o,$(SRCS))
CFLAGS = -O2 -g -Wall -fmessage-length=0
LDFLAGS = -static  
LIBS += -lcrypt
#CC = /home/xuyanxue/xuyx_work/a10/lichee/buildroot/output/external-toolchain/bin/arm-none-linux-gnueabi-gcc
CC = ../../build/gcc-4.3.3/build_mips/staging_dir/usr/bin/mips-linux-uclibc-gcc
TARGET =	ukey_test

$(TARGET):	$(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS) $(LDFLAGS)
	
all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
