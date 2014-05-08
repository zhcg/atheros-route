ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

GCC = $(CROSS)gcc

TERMINAL_DEV_REGISTERBIN = terminal_dev_register
TERMINAL_DEV_REGISTEROBJ = terminal_dev_register.o network_config.o request_cmd.o

TERMINAL_INIT_TESTBIN = terminal_init_test
TERMINAL_INIT_TESTOBJ = terminal_init_test.o network_config.o

INTERFACE_TESTBIN = interface_test
INTERFACE_TESTOBJ = interface_test.o network_config.o

SPI_MAINBIN = spi_rt_main
SPI_MAINOBJ = spi_rt_main.o

SPI_RT_UART1BIN = spi_rt_uart1
SPI_RT_UART1OBJ = spi_rt_uart1.o

SWIPE_CARDBIN = swipe_card
SWIPE_CARDOBJ = swipe_card.o

MONITOR_APPBIN = monitor_application
MONITOR_APPOBJ = monitor_application.o

USB_STUBBIN = usb_stub
USB_STUBOBJ = usb_stub_main.o

###############################################################################
COMMON_TOOLSBIN = libcommon_tools.so
COMMON_TOOLSOBJ = common_tools.o

DATA_STORAGERBIN = libdata_storager.so
DATA_STORAGEROBJ = database_management.o
#DATA_STORAGEROBJ = nvram_interface.o

COMMUNICATIONBIN = libcommunication.so
COMMUNICATIONOBJ = communication_serial.o communication_usb.o communication_network.o

SPI_RTBIN = libspi_rt.so
SPI_RTOBJ = spi_rt_interface.o

TERMINAL_AUTHENTICATIONBIN = libterminal_authentication.so
TERMINAL_AUTHENTICATIONOBJ = terminal_authentication.o safe_strategy.o

TERMINAL_REGISTERBIN = libterminal_register.so
TERMINAL_REGISTEROBJ = terminal_register.o

SQLITE_DIR=./sqlite-mips-linux
###############################################################################

#编译 动态库
all:so app
so:$(COMMON_TOOLSBIN) $(DATA_STORAGERBIN) $(COMMUNICATIONBIN) $(SPI_RTBIN) $(TERMINAL_AUTHENTICATIONBIN) $(TERMINAL_REGISTERBIN)
app:$(TERMINAL_DEV_REGISTERBIN) $(INTERFACE_TESTBIN) $(TERMINAL_INIT_TESTBIN) $(SPI_MAINBIN) $(SPI_RT_UART1BIN) $(SWIPE_CARDBIN) $(MONITOR_APPBIN) $(USB_STUBBIN)

#编译 终端初始化 整体测试执行文件
$(TERMINAL_DEV_REGISTERBIN):$(TERMINAL_DEV_REGISTEROBJ) 
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldata_storager -lcommunication -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::terminal_dev_register ok:::::::::::::::::"
	-@ echo
	
#编译终端初始化 测试
$(INTERFACE_TESTBIN):$(INTERFACE_TESTOBJ) 
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldata_storager -lcommunication -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::interface_test ok:::::::::::::::::"
	-@ echo

#编译终端初始化 测试
$(TERMINAL_INIT_TESTBIN):$(TERMINAL_INIT_TESTOBJ) 
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldata_storager -lcommunication -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::terminal_init_test ok:::::::::::::::::"
	-@ echo
	
$(SPI_MAINBIN):$(SPI_MAINOBJ)
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldata_storager -lcommunication -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::spi_rt_main ok:::::::::::::::::"
	-@ echo
	
$(SPI_RT_UART1BIN):$(SPI_RT_UART1OBJ)
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldata_storager -lcommunication -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::spi_rt_uart1 ok:::::::::::::::::"
	-@ echo

$(SWIPE_CARDBIN):$(SWIPE_CARDOBJ) 
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldata_storager -lcommunication -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::swipe_card ok:::::::::::::::::"
	-@ echo
	
$(MONITOR_APPBIN):$(MONITOR_APPOBJ)
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldata_storager -lcommunication -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::monitor_application ok:::::::::::::::::"
	-@ echo

$(USB_STUBBIN):$(USB_STUBOBJ)
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldata_storager -lcommunication -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::usb_stub ok:::::::::::::::::"
	-@ echo

########################################################################################
$(COMMON_TOOLSBIN):$(COMMON_TOOLSOBJ)
	$(GCC) -shared -o $@ $^
	-@ echo ":::::::::::::::::libcommon_tools.so ok:::::::::::::::::"
	-@ echo
	
$(DATA_STORAGERBIN):$(DATA_STORAGEROBJ) -lcommon_tools
	$(GCC) -shared -o $@ $^ -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::libdata_storager.so ok:::::::::::::::::"
	-@ echo

$(COMMUNICATIONBIN):$(COMMUNICATIONOBJ) -lcommon_tools
	$(GCC) -shared -o $@ $^ -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::libcommunication.so ok:::::::::::::::::"
	-@ echo
	
$(SPI_RTBIN):$(SPI_RTOBJ) -lcommon_tools
	$(GCC) -shared -o $@ $^
	-@ echo ":::::::::::::::::libspi_rt.so ok:::::::::::::::::"
	-@ echo
	
$(TERMINAL_REGISTERBIN):$(TERMINAL_REGISTEROBJ) -lcommon_tools -lterminal_authentication
	$(GCC) -shared -o $@ $^ -lpthread
	-@ echo ":::::::::::::::::libterminal_register.so ok:::::::::::::::::"
	-@ echo
	
$(TERMINAL_AUTHENTICATIONBIN):$(TERMINAL_AUTHENTICATIONOBJ) -lcommon_tools  
	$(GCC) -shared -o $@ $^ -lpthread -L. -ldata_storager -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::libterminal_authentication.so ok:::::::::::::::::"
	-@ echo
	
.c.o:
	$(GCC) -c -fPIC $< -I$(SQLITE_DIR)/include

clean:
	-rm *.o -rf
	-rm *.so -rf
	-rm $(TERMINAL_DEV_REGISTERBIN) $(INTERFACE_TESTBIN) $(TERMINAL_INIT_TESTBIN) $(SPI_MAINBIN) $(SPI_RT_UART1BIN) -rf
	-rm $(SWIPE_CARDBIN) $(MONITOR_APPBIN) $(USB_STUBBIN) -rf
