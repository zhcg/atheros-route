ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

GCC = $(CROSS)gcc

CACMBIN = cacm
CACMOBJ = cacm.o cacm_tools.o exevents.o

ROOT_DIR = ../..
LD_LIB_DIR = ../../../terminal_dev_register

$(CACMBIN):$(CACMOBJ)
	$(GCC) -o $@ $^ -I. -I$(LD_LIB_DIR)/sqlite-mips-linux/include -I$(ROOT_DIR)/target/include -lpthread -L$(ROOT_DIR)/target/lib -losipparser2 -losip2 -leXosip2  -L$(LD_LIB_DIR)/sqlite-mips-linux/lib -lsqlite3 -L$(LD_LIB_DIR) -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldata_storager -lcommunication
	cp $(CACMBIN) $(ROOT_DIR)/target/bin
.c.o:
	$(GCC) -c $< -I. -I$(ROOT_DIR)/target/include -I$(LD_LIB_DIR) -I$(LD_LIB_DIR)/sqlite-mips-linux/include

clean:
	rm *.o -rf
	rm $(CACMBIN) -rf
