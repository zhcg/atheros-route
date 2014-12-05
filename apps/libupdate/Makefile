BIN = libupdate.a
OBJ = update.o our_md5.o

ifneq ($(strip $(TOOLPREFIX)),)
	CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
AR=$(CROSS)ar

CFLAGS = -fPIC

all:$(OBJ)
	$(AR) -cr $(BIN) $^ libmjson.a 

.c.o:
	$(CC) -c $^ $(CFLAGS)

clean:
	rm -rf $(BIN) $(OBJ) 

