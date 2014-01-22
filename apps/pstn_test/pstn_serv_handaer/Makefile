include include.mk

TARGET = phone_serv_19200

TOPDIR_PHONE = ..


#SPI_INCLUDE_DIR = $(TOPDIR_PHONE)/lib_of_terminal_init
SPI_INCLUDE_DIR = $(TOPDIR_PHONE)/../terminal_dev_register
#SPI_LIB_DIR = $(TOPDIR_PHONE)/lib/lib
SQLITE_DIR = $(SPI_INCLUDE_DIR)/sqlite-mips-linux

NVRAM_INCLUDE_DIR = $(TOPDIR_PHONE)/lib/libnvram
LIB_LIB_DIR = $(TOPDIR_PHONE)/lib/lib


CFLAGS += -I$(SPI_INCLUDE_DIR) -DSPI_FOR_PHONE -I$(NVRAM_INCLUDE_DIR) -I$(SQLITE_DIR)/include
#LDFLAGS += -L$(SPI_INCLUDE_DIR)  -lcommon_tools -lnvram_interface -lterminal_authentication -lterminal_register -lspi_rt  -L$(NVRAM_INCLUDE_DIR) -lnvram -lpthread
#LDFLAGS += -L$(LIB_LIB_DIR)  -L$(NVRAM_INCLUDE_DIR)   -L$(SPI_INCLUDE_DIR)  -lcommon_tools  -lspi_rt   -pthread
LDFLAGS += -L$(LIB_LIB_DIR)  -L$(NVRAM_INCLUDE_DIR)   -L$(SPI_INCLUDE_DIR)  -lpthread -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldata_storager -lcommunication -L$(SQLITE_DIR)/lib -lsqlite3 


%.o: %.c

	$(CC) $(CFLAGS) -c $< -o $@   

%.o: %.cc

	$(CXX) $(CFLAGS) -c $< -o $@   
 

SOURCES := $(wildcard *.c *.cc)
OBJS := $(patsubst %.c,%.o,$(patsubst %.cc,%.o,$(SOURCES)))

SUBDIR = system_wrapper
SUBSOURCES := $(wildcard $(SUBDIR)/*.c $(SUBDIR)/*.cc)
SUBOBJ := $(patsubst %.c,%.o,$(patsubst %.cc,%.o,$(SUBSOURCES)))

.PHONY: all clean
all: subsystem  $(TARGET)

subsystem:
	$(MAKE) -C $(SUBDIR)
	@echo $(SUBSOURCES)
	@echo $(SUBOBJ)

$(TARGET) : subsystem $(SUBOBJ) $(OBJS)  
	@echo "src&objs"
	@echo $(SOURCES)
	@echo $(OBJS)
	@echo $(SUBSOURCES)
	@echo $(SUBOBJ)

	@echo $(TOPDIR_PHONE)
	
#	$(CXX) $(LDFLAGS)  -o $(TARGET)  $(OBJS) $(SUBOBJ)  
#	$(CXX) -Wl,-rpath,/home/chenchunsheng/build_mips/staging_dir/lib  -o $(TARGET)  $(OBJS) $(SUBOBJ)  $(LDFLAGS)
	$(CXX) -Wl,-rpath,$(TOOLPATH)../lib  -o $(TARGET)  $(OBJS) $(SUBOBJ)  $(LDFLAGS)
	chmod a+x $(TARGET)
	@echo $(TOOLPATH)
	@echo $(TOOLPREFIX)



clean:

	rm -rf *.o $(TARGET)
	$(MAKE) -C $(SUBDIR) clean


