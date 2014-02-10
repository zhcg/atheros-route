BIN=check_phone_status
OBJ=check_phone_status.o

ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP=$(CROSS)strip

all:$(BIN)

$(BIN):$(OBJ)
		$(CC) -o $(BIN) $^ -lpthread

.c.o:
		$(CC) -c $^

clean:
		rm -f $(BIN) $(OBJ)
