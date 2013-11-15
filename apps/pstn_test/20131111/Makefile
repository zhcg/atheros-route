#GCC = arm-linux-gcc
#GCC = mipsel-linux-gcc
GCC = mips-linux-gcc

ifeq ("$(GCC)", "arm-linux-gcc")
LIB_DIR = /home/newdisk_1205/yangjl/sqlite/sqlite-autoconf-3071502/sqlite-arm-linux/
FLAGS = -I$(LIB_DIR)include/ -L$(SQLITE_DIR)lib/ -lsqlite3 -L. -ltools
endif

ifeq ("$(GCC)", "mipsel-linux-gcc")
SOURCE_5350_DIR = /home/newdisk_1205/yangjl/5350/source/romfs/home
LIB_DIR = /home/newdisk_1205/yangjl/5350/source/lib/libnvram/
FLAGS = -L$(LIB_DIR)lib/ -lnvram -L.
endif

ifeq ("$(GCC)", "mips-linux-gcc")

endif

SHAREDLIB = so


TERMINAL_INITBIN = terminal_init
TERMINAL_INITOBJ = terminal_init.o network_config.o internetwork_communication.o

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

SPI_TESTBIN = spi_rt_test
SPI_TESTOBJ = spi_rt_test.o

SPI_RECV_TESTBIN = spi_rt_recv_test
SPI_RECV_TESTOBJ = spi_rt_recv_test.o

SPI_SEND_TESTBIN = spi_rt_send_test
SPI_SEND_TESTOBJ = spi_rt_send_test.o

SPI_RT_PTHREADBIN = spi_rt_pthread
SPI_RT_PTHREADOBJ = spi_rt_pthread.o

SPI_RT_PTHREAD_HOOKBIN = spi_rt_pthread_hook
SPI_RT_PTHREAD_HOOKOBJ = spi_rt_pthread_hook.o

SPI_RECV_SEND_TESTBIN = spi_recv_send_test
SPI_RECV_SEND_TESTOBJ = spi_recv_send_test.o

SPI_RT_SELECTBIN = spi_rt_select
SPI_RT_SELECTOBJ = spi_rt_select.o

SWIPE_CARDBIN = swipe_card
SWIPE_CARDOBJ = swipe_card.o

MONITOR_APPBIN = monitor_application
MONITOR_APPOBJ = monitor_application.o

TERMINAL_AUTHENTICATIONBIN = libterminal_authentication.so
#TERMINAL_AUTHENTICATIONOBJ = terminal_authentication.o unionpay_message.o  communication_stc.o internetwork_communication.o
TERMINAL_AUTHENTICATIONOBJ = terminal_authentication.o communication_stc.o internetwork_communication.o safe_strategy.o
TERMINAL_REGISTERBIN = libterminal_register.so
TERMINAL_REGISTEROBJ = terminal_register.o


CONFIGURE_ROUTEBIN = route
CONFIGURE_ROUTEOBJ = configure_route.c

ifeq ("$(GCC)", "arm-linux-gcc")
#编译 动态库
$(SHAREDLIB):libcommon_tools.so libdatabase_management.so libterminal_authentication.so libterminal_register.so

#编译 终端初始化 整体测试执行文件
$(TERMINAL_INITBIN):$(TERMINAL_INITOBJ) 
	$(GCC) -o $@ $^ -I. -lpthread -L$(LIB_DIR)lib/ -lsqlite3 -L. -lcommon_tools -ldatabase_management -lterminal_authentication -lterminal_register
	-rm *.o -rf
	-@ echo ":::::::::::::::::terminal_init ok:::::::::::::::::"

$(TERMINAL_INIT_TESTBIN):$(TERMINAL_INIT_TESTOBJ) 
	$(GCC) -o $@ $^ -I. -lpthread -L$(LIB_DIR)lib/ -lsqlite3 -L. -lcommon_tools -ldatabase_management -lterminal_authentication -lterminal_register
	-rm *.o -rf
	-@ echo ":::::::::::::::::terminal_init_test ok:::::::::::::::::"

$(COMMON_TOOLSBIN):$(COMMON_TOOLSOBJ)
	$(GCC) -shared -fPIC -o $@ $^
	-@ echo ":::::::::::::::::libcommon_tools.so ok:::::::::::::::::"
	-@ echo
	
$(DATABASE_MANAGEMENTBIN):$(DATABASE_MANAGEMENTOBJ) -lcommon_tools
	$(GCC) -shared -fPIC -o $@ $^ -L$(LIB_DIR)lib -lsqlite3
	-@ echo ":::::::::::::::::libdatabase_management.so ok:::::::::::::::::"
	-@ echo

$(TERMINAL_REGISTERBIN):$(TERMINAL_REGISTEROBJ) -lcommon_tools -ldatabase_management -lterminal_authentication
	$(GCC) -shared -fPIC -o $@ $^ -lpthread
	-@ echo ":::::::::::::::::libterminal_register.so ok:::::::::::::::::"
	-@ echo
	
$(TERMINAL_AUTHENTICATIONBIN):$(TERMINAL_AUTHENTICATIONOBJ) -lcommon_tools -ldatabase_management
	$(GCC) -shared -fPIC -o $@ $^ -lpthread
	-@ echo ":::::::::::::::::libterminal_authentication.so ok:::::::::::::::::"
	-@ echo
endif

ifeq ("$(GCC)", "mipsel-linux-gcc")

#编译 动态库
$(SHAREDLIB):libcommon_tools.so libnvram_interface.so libterminal_authentication.so libterminal_register.so libspi_rt.so

#编译 终端初始化 整体测试执行文件
$(TERMINAL_INITBIN):$(TERMINAL_INITOBJ) 
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools -lnvram_interface -lterminal_authentication -lterminal_register -lspi_rt
#	-rm *.o -rf
	-@ echo ":::::::::::::::::terminal_init ok:::::::::::::::::"

#编译终端初始化 测试
$(TERMINAL_INIT_TESTBIN):$(TERMINAL_INIT_TESTOBJ) 
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools -lnvram_interface -lterminal_authentication -lterminal_register -lspi_rt
#	-rm *.o -rf
	-@ echo ":::::::::::::::::terminal_init_test ok:::::::::::::::::"

$(SPI_MAINBIN):$(SPI_MAINOBJ) 
	$(GCC) -o $@ $^ -I. -L. -lcommon_tools -lnvram_interface -lterminal_authentication -lterminal_register -lspi_rt
#	-rm *.o -rf
	-@ echo ":::::::::::::::::spi_rt_main ok:::::::::::::::::"

$(SPI_TESTBIN):$(SPI_TESTOBJ) 
	$(GCC) -o $@ $^ -I. -L. -lcommon_tools -lnvram_interface -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::spi_rt_test ok:::::::::::::::::"
	
$(SPI_RECV_TESTBIN):$(SPI_RECV_TESTOBJ) 
	$(GCC) -o $@ $^ -I. -L. -lcommon_tools -lnvram_interface -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::spi_rt_recv_test ok:::::::::::::::::"

$(SPI_SEND_TESTBIN):$(SPI_SEND_TESTOBJ) 
	$(GCC) -o $@ $^ -I. -L. -lcommon_tools -lnvram_interface -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::spi_rt_send_test ok:::::::::::::::::"

$(SPI_RT_PTHREADBIN):$(SPI_RT_PTHREADOBJ) 
	$(GCC) -o $@ $^ -I. -L. -lcommon_tools -lnvram_interface -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::spi_rt_pthread ok:::::::::::::::::"

$(SPI_RT_PTHREAD_HOOKBIN):$(SPI_RT_PTHREAD_HOOKOBJ) 
	$(GCC) -o $@ $^ -I. -L. -lcommon_tools -lnvram_interface -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::spi_rt_pthread_hook ok:::::::::::::::::"
	
$(SPI_RECV_SEND_TESTBIN):$(SPI_RECV_SEND_TESTOBJ) 
	$(GCC) -o $@ $^ -I. -L. -lcommon_tools -lnvram_interface -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::spi_recv_send_test ok:::::::::::::::::"
	
$(SPI_RT_SELECTBIN):$(SPI_RT_SELECTOBJ) 
	$(GCC) -o $@ $^ -I. -L. -lcommon_tools -lnvram_interface -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::spi_rt_select ok:::::::::::::::::"

$(SWIPE_CARDBIN):$(SWIPE_CARDOBJ) 
	$(GCC) -o $@ $^ -I. -L. -lcommon_tools -lnvram_interface -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::swipe_card ok:::::::::::::::::"
	
$(MONITOR_APPBIN):$(MONITOR_APPOBJ)
	$(GCC) -o $@ $^ -I. -L. -lcommon_tools -lnvram_interface -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::monitor_application ok:::::::::::::::::"
	
$(COMMON_TOOLSBIN):$(COMMON_TOOLSOBJ)
	$(GCC) -shared -fPIC -o $@ $^
	-@ echo ":::::::::::::::::libcommon_tools.so ok:::::::::::::::::"
	-@ echo

$(NVRAMBIN):$(NVRAMOBJ) -lcommon_tools
	$(GCC) -shared -fPIC -o $@ $^ -L$(LIB_DIR) -lnvram
	-@ echo ":::::::::::::::::libnvram_interface.so ok:::::::::::::::::"
	-@ echo

$(SPI_RTBIN):$(SPI_RTOBJ) -lcommon_tools
	$(GCC) -shared -fPIC -o $@ $^
	-@ echo ":::::::::::::::::libspi_rt.so ok:::::::::::::::::"
	-@ echo
	
$(TERMINAL_REGISTERBIN):$(TERMINAL_REGISTEROBJ) -lcommon_tools -lnvram_interface -lterminal_authentication
	$(GCC) -shared -fPIC -o $@ $^ -lpthread
	-@ echo ":::::::::::::::::libterminal_register.so ok:::::::::::::::::"
	-@ echo
	
$(TERMINAL_AUTHENTICATIONBIN):$(TERMINAL_AUTHENTICATIONOBJ) -lcommon_tools -lnvram_interface
	$(GCC) -shared -fPIC -o $@ $^ -lpthread
	-@ echo ":::::::::::::::::libterminal_authentication.so ok:::::::::::::::::"
	-@ echo
endif

ifeq ("$(GCC)", "mips-linux-gcc")
#编译 动态库
$(SHAREDLIB):libcommon_tools.so libterminal_authentication.so libterminal_register.so libspi_rt.so

#编译 终端初始化 整体测试执行文件
$(TERMINAL_INITBIN):$(TERMINAL_INITOBJ) 
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::terminal_init ok:::::::::::::::::"

#编译终端初始化 测试
$(TERMINAL_INIT_TESTBIN):$(TERMINAL_INIT_TESTOBJ) 
	$(GCC) -o $@ $^ -I. -lpthread -L. -lcommon_tools  -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::terminal_init_test ok:::::::::::::::::"

$(SPI_MAINBIN):$(SPI_MAINOBJ)
	$(GCC) -o $@ $^ -lpthread -I. -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::spi_rt_main ok:::::::::::::::::"

$(SWIPE_CARDBIN):$(SWIPE_CARDOBJ) 
	$(GCC) -o $@ $^ -lpthread -I. -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::swipe_card ok:::::::::::::::::"

$(SPI_RT_PTHREADBIN):$(SPI_RT_PTHREADOBJ) 
	$(GCC) -o $@ $^ -lpthread -I. -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::spi_rt_pthread ok:::::::::::::::::"
	
$(MONITOR_APPBIN):$(MONITOR_APPOBJ)
	$(GCC) -o $@ $^ -lpthread -I. -L. -lcommon_tools -lterminal_authentication -lterminal_register -lspi_rt
	-@ echo ":::::::::::::::::monitor_application ok:::::::::::::::::"
	
$(COMMON_TOOLSBIN):$(COMMON_TOOLSOBJ)
	$(GCC) -shared -o $@ $^
	-@ echo ":::::::::::::::::libcommon_tools.so ok:::::::::::::::::"
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
	$(GCC) -shared -o $@ $^ -lpthread
	-@ echo ":::::::::::::::::libterminal_authentication.so ok:::::::::::::::::"
	-@ echo
endif

#$(CONFIGURE_ROUTEBIN):$(CONFIGURE_ROUTEOBJ)
#	mipsel-linux-gcc -o configure_route $^
#	-cp configure_route /mnt/mount/
#	-cp configure_route /home/newdisk_1205/yangjl/5350/RT288x_SDK/source/romfs/bin/ -rf
#	-@ echo ":::::::::::::::::configure_route ok:::::::::::::::::"

.c.o:
	$(GCC) -c -fPIC $<
	

	
ifeq ("$(GCC)", "arm-linux-gcc")

install:
	-mkdir -p install/bin
	-mkdir -p install/lib
	-mkdir -p install/include
	-mv $(TERMINAL_INITBIN) install/bin
	-mv *.so install/lib
	-cp *.h install/include

clean:
	rm install -rf
	rm configure_route -rf

endif

ifeq ("$(GCC)", "mipsel-linux-gcc")
install:
	-rm *.o -rf
	-mkdir -p /mnt/mount/terminal_init_f1b/bin
	-mkdir -p /mnt/mount/terminal_init_f1b/lib
	-mkdir -p /mnt/mount/terminal_init_f1b/include
	-cp $(TERMINAL_INITBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(TERMINAL_INIT_TESTBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(SPI_MAINBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(SPI_TESTBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(SPI_RECV_TESTBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(SPI_SEND_TESTBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(SPI_RT_PTHREADBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(SPI_RT_PTHREAD_HOOKBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(SPI_RECV_SEND_TESTBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(SPI_RT_SELECTBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(SWIPE_CARDBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(MONITOR_APPBIN) /mnt/mount/terminal_init_f1b/bin
	-cp *.so /mnt/mount/terminal_init_f1b/lib
	-cp *.h /mnt/mount/terminal_init_f1b/include
	-cp spi_test.sh /mnt/mount/terminal_init_f1b/bin
	
#	-mv $(TERMINAL_INITBIN) $(SOURCE_5350_DIR)/terminal_init_f1b/bin/
#	-mv $(TERMINAL_INIT_TESTBIN) $(SOURCE_5350_DIR)/terminal_init_f1b/bin/
#	-mv $(SPI_MAINBIN) $(SOURCE_5350_DIR)/terminal_init_f1b/bin/
#	-mv $(SPI_TESTBIN) $(SOURCE_5350_DIR)/terminal_init_f1b/bin/
#	-mv $(SPI_RECV_TESTBIN) $(SOURCE_5350_DIR)/terminal_init_f1b/bin/
#	-mv $(SPI_SEND_TESTBIN) $(SOURCE_5350_DIR)/terminal_init_f1b/bin/
#	-mv $(SPI_RT_PTHREADBIN) $(SOURCE_5350_DIR)/terminal_init_f1b/bin/
#	-mv $(SPI_RT_PTHREAD_HOOKBIN) $(SOURCE_5350_DIR)/terminal_init_f1b/bin/
#	-mv $(SPI_RECV_SEND_TESTBIN) $(SOURCE_5350_DIR)/terminal_init_f1b/bin/
#	-mv $(SPI_RT_SELECTBIN) $(SOURCE_5350_DIR)/terminal_init_f1b/bin/
#	-mv $(SWIPE_CARDBIN) $(SOURCE_5350_DIR)/terminal_init_f1b/bin/
#	-mv $(MONITOR_APPBIN) $(SOURCE_5350_DIR)/terminal_init_f1b/bin/
#	-mv *.so $(SOURCE_5350_DIR)/terminal_init_f1b/lib/
#	-cp *.h $(SOURCE_5350_DIR)/terminal_init_f1b/include/
#	-cp spi_test.sh $(SOURCE_5350_DIR)/terminal_init_f1b/bin/

clean:
	rm /mnt/mount/terminal_init_f1b/ -rf
	rm configure_route -rf
	-rm *.o -rf
	-rm *.so -rf
	-rm $(TERMINAL_INITBIN) $(TERMINAL_INIT_TESTBIN) $(SPI_MAINBIN) $(SPI_RT_SELECTBIN) -rf
	-rm $(SPI_TESTBIN) $(SPI_RECV_TESTBIN) $(SPI_SEND_TESTBIN) $(SPI_RT_PTHREADBIN) -rf
	-rm $(SPI_RT_PTHREAD_HOOKBIN) $(SPI_RECV_SEND_TESTBIN) $(SWIPE_CARDBIN) $(MONITOR_APPBIN) -rf
	
	-rm $(SOURCE_5350_DIR)/terminal_init_f1b/bin/* -rf
	-rm $(SOURCE_5350_DIR)/terminal_init_f1b/include/* -rf
	-rm $(SOURCE_5350_DIR)/terminal_init_f1b/lib/* -rf
	
endif

ifeq ("$(GCC)", "mips-linux-gcc")
install:
	-rm *.o -rf
	-mkdir -p /mnt/mount/terminal_init_f1b/bin
	-mkdir -p /mnt/mount/terminal_init_f1b/lib
	-mkdir -p /mnt/mount/terminal_init_f1b/include
	-cp $(TERMINAL_INITBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(TERMINAL_INIT_TESTBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(SPI_MAINBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(SWIPE_CARDBIN) /mnt/mount/terminal_init_f1b/bin
	-cp $(MONITOR_APPBIN) /mnt/mount/terminal_init_f1b/bin
	-cp *.so /mnt/mount/terminal_init_f1b/lib
	-cp *.h /mnt/mount/terminal_init_f1b/include

clean:
	rm /mnt/mount/terminal_init_f1b/ -rf
	rm configure_route -rf
	-rm *.o -rf
	-rm *.so -rf
	-rm $(TERMINAL_INITBIN) $(TERMINAL_INIT_TESTBIN) $(SPI_MAINBIN) -rf
	-rm $(SWIPE_CARDBIN) $(MONITOR_APPBIN) -rf
	
	-rm $(SOURCE_5350_DIR)/terminal_init_f1b/bin/* -rf
	-rm $(SOURCE_5350_DIR)/terminal_init_f1b/include/* -rf
	-rm $(SOURCE_5350_DIR)/terminal_init_f1b/lib/* -rf
endif
