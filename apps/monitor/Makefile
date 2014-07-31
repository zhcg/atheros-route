BASE_TYPE = 
BIN = monitor

ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC=$(CROSS)gcc
STRIP=$(CROSS)strip

CC_9344 = mips-linux-uclibc-gcc
FLAGS_9344 = -L../cacm/target/lib -I../cacm/target/include -I../terminal_dev_register/sqlite-mips-linux/include -L../terminal_dev_register/sqlite-mips-linux/lib -lsqlite3
CC_A20 = arm-linux-gnueabi-gcc
FLAGS_A20 = -L./lib -L.. -I./include -lupdate -lmjson

all:
	$(error 'make 9344_build/a20_build BASE_TYPE=BASE_9344/BASE_A20') 
9344_build:
	$(CC_9344) monitor.c -D$(BASE_TYPE) $(FLAGS_9344)  -losipparser2 -losip2 -leXosip2 -lpthread -o $(BIN)
a20_build:
	$(CC_A20) monitor.c -D$(BASE_TYPE) $(FLAGS_A20)  -losipparser2 -losip2 -leXosip2 -lpthread -o $(BIN)
clean:
	rm -rf $(BIN)

