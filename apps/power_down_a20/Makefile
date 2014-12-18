BIN=power_down_a20
OBJ=power_down_a20.o

ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP=$(CROSS)strip

all:$(BIN)

$(BIN):$(OBJ)
		$(CC) -o $(BIN) $^

.c.o:
		$(CC) -c $^ -D$(BUILD_VERSION)

clean:
		rm -f $(BIN) $(OBJ)
