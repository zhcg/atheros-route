CURRENT_DIR = $(shell pwd)

HOST = mips-linux

BUILD_OSIP2_DIR = $(shell pwd)/build/libosip
BUILD_EXOSIP2_DIR = $(shell pwd)/build/libeXosip

SRC_OSIP2_DIR = $(shell pwd)/src/libosip2-3.3.0
SRC_EXOSIP2_DIR = $(shell pwd)/src/libeXosip2-3.3.0

SRC_CACM_DIR = $(CURRENT_DIR)/src/cacm

TARGET_DIR = $(CURRENT_DIR)/target
LD_LIB_DIR = /home/newdisk_1205/yangjl/fertile_terminal_authentication/20131028

#all: libosip2 libeXosip2 cacm
all: cacm

libosip2:
	cd $(BUILD_OSIP2_DIR);\
	../../src/libosip2-3.3.0/configure --disable-static --enable-shared --host=$(HOST) --prefix=$(TARGET_DIR);\
	make;\
	make install
	@echo ":::::::::::::::::osip2 make ok:::::::::::::::::"

libeXosip2:
	cd $(BUILD_EXOSIP2_DIR);\
	../../src/libeXosip2-3.3.0/configure --host=$(HOST) --with-openssl--disable-shared --enable-static --prefix=$(TARGET_DIR) PKG_CONFIG_PATH=$(TARGET_DIR)/lib/pkgconfig CFLAGS="-L$(TARGET_DIR)/lib/";\
	make;\
	make install
	@echo ":::::::::::::::::exosip2 make ok:::::::::::::::::"
	
cacm:
	cd $(SRC_CACM_DIR);\
	make
	@echo ":::::::::::::::::cacm make ok:::::::::::::::::"
	
clean:
#	$(MAKE) -C $(BUILD_OSIP2_DIR) clean 
#	$(MAKE) -C $(BUILD_EXOSIP2_DIR) clean 
	$(MAKE) -C $(SRC_CACM_DIR) clean 
	
#	$(RM) $(TARGET_DIR)/* -rf

