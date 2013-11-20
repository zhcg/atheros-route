ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

GCC = $(CROSS)gcc

SHAREDLIB = so

TERMINAL_DEV_REGISTERBIN = terminal_dev_register
TERMINAL_DEV_REGISTEROBJ = terminal_dev_register.o network_config.o internetwork_communication.o

TERMINAL_INIT_TESTBIN = terminal_init_test
TERMINAL_INIT_TESTOBJ = terminal_init_test.o network_config.o internetwork_communication.o

FINANCIAL_DEALINGBIN = financial_dealing
FINANCIAL_DEALINGOBJ = financial_dealing.o unionpay_message.o communication_stc.o

COMMON_TOOLSBIN = libcommon_tools.so
COMMON_TOOLSOBJ = common_tools.o

DATABASE_MANAGEMENTBIN = libdatabase_management.so
DATABASE_MANAGEMENTOBJ = database_management.o

NVRAMBIN = libnvram_interface.so
NVRAMOBJ = nvram_interface.o

SPI_RTBIN = libspi_rt.so
SPI_RTOBJ = spi_rt_interface.o

SPI_MAINBIN = spi_rt_main
SPI_MAINOBJ = spi_rt_main.o

SWIPE_CARDBIN = swipe_card
SWIPE_CARDOBJ = swipe_card.o

MONITOR_APPBIN = monitor_application
MONITOR_APPOBJ = monitor_application.o

TERMINAL_AUTHENTICATIONBIN = libterminal_authentication.so
TERMINAL_AUTHENTICATIONOBJ = terminal_authentication.o communication_stc.o internetwork_communication.o safe_strategy.o

TERMINAL_REGISTERBIN = libterminal_register.so
TERMINAL_REGISTEROBJ = terminal_register.o

SQLITE_DIR=./sqlite-mips-linux

#编译 动态库
all:$(SHAREDLIB)  app
$(SHAREDLIB):libcommon_tools.so libdatabase_management.so libterminal_authentication.so libterminal_register.so libspi_rt.so
app:$(TERMINAL_DEV_REGISTERBIN) $(TERMINAL_INIT_TESTBIN) $(SPI_MAINBIN) $(SWIPE_CARDBIN) $(MONITOR_APPBIN)

#编译 终端初始化 整体测试执行文件
$(TERMINAL_DEV_REGISTERBIN):$(TERMINAL_DEV_REGISTEROBJ) 
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldatabase_management -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::terminal_dev_register ok:::::::::::::::::"
	-@ echo
	
#编译终端初始化 测试
$(TERMINAL_INIT_TESTBIN):$(TERMINAL_INIT_TESTOBJ) 
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools  -lterminal_authentication -lterminal_register -lspi_rt -ldatabase_management -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::terminal_init_test ok:::::::::::::::::"
	-@ echo
	
$(SPI_MAINBIN):$(SPI_MAINOBJ)
	$(GCC) -o $@ $^ -lpthread -I. -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldatabase_management -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::spi_rt_main ok:::::::::::::::::"
	-@ echo
$(SWIPE_CARDBIN):$(SWIPE_CARDOBJ) 
	$(GCC) -o $@ $^ -lpthread -I. -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldatabase_management -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::swipe_card ok:::::::::::::::::"
	-@ echo
	
$(MONITOR_APPBIN):$(MONITOR_APPOBJ)
	$(GCC) -o $@ $^ -lpthread -I. -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt -ldatabase_management -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::monitor_application ok:::::::::::::::::"
	-@ echo
	
$(COMMON_TOOLSBIN):$(COMMON_TOOLSOBJ)
	$(GCC) -shared -o $@ $^
	-@ echo ":::::::::::::::::libcommon_tools.so ok:::::::::::::::::"
	-@ echo
	
$(DATABASE_MANAGEMENTBIN):$(DATABASE_MANAGEMENTOBJ) -lcommon_tools
	$(GCC) -shared -o $@ $^ -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::libdatabase_management.so ok:::::::::::::::::"
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
	$(GCC) -shared -o $@ $^ -lpthread -L. -ldatabase_management -L$(SQLITE_DIR)/lib -lsqlite3
	-@ echo ":::::::::::::::::libterminal_authentication.so ok:::::::::::::::::"
	-@ echo
	
.c.o:
	$(GCC) -c -fPIC $< -I$(SQLITE_DIR)/include

clean:
	-rm *.o -rf
	-rm *.so -rf
	-rm $(TERMINAL_DEV_REGISTERBIN) $(TERMINAL_INIT_TESTBIN) $(SPI_MAINBIN) -rf
	-rm $(SWIPE_CARDBIN) $(MONITOR_APPBIN) -rf