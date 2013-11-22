TOPDIR= ./

ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC = $(CROSS)gcc

CXX = $(CROSS)g++


DIR_LIBMEDIASTREAMER := mediastreamer-2.8.2
DIR_LIBORTP := ortp-0.20.0
DIR_LIBSPEEX := speex-1.2rc1
DIR_LIBOGG := libogg-1.3.0

DIR_LIBSPIRT := 20131111
DIR_PSTN_APP := pstn_serv_handaer

DIR_LIBPHONE = $(TOPDIR)phone/lib

LIBMEDIASTREAMER_LA = libmediastreamer.la
LIBMEDIASTREAMER_SO = libmediastreamer.so
LIBMEDIASTREAMER_SO_1 = libmediastreamer.so.1
LIBMEDIASTREAMER_SO_1_0_0 = libmediastreamer.so.1.0.0

LIBORTP_LA = libortp.la
LIBORTP_SO = libortp.so
LIBORTP_SO_8 = libortp.so.8
LIBORTP_SO_8_0_0 = libortp.so.8.0.0

LIBSPEEX_LA = libspeex.la
LIBSPEEX_SO = libspeex.so
LIBSPEEX_SO_1 = libspeex.so.1
LIBSPEEX_SO_1_5_0 = libspeex.so.1.5.0

LIBSPEEXDSP_LA = libspeexdsp.la
LIBSPEEXDSP_SO = libspeexdsp.so
LIBSPEEXDSP_SO_1 = libspeexdsp.so.1
LIBSPEEXDSP_SO_1_5_0 = libspeexdsp.so.1.5.0

LIBOGG_LA = libogg.la
LIBOGG_SO = libogg.so
LIBOGG_SO_0 = libogg.so.0
LIBOGG_SO_0_8_0 = libogg.so.0.8.0


RESET_SRC = reset.cpp
RESET_EXE = reset_mips


all: libmediastreamer  libspirt socket_dep  pstn_app reset_mips


reset_mips:
	$(CXX) -o $(RESET_EXE) $(RESET_SRC)


pstn_app: libmediastreamer  libspirt  socket_dep
	make -C $(DIR_PSTN_APP)

socket_dep:
	@echo $(TOOLPATH)	
	cp -af ./socket.h $(TOOLPATH)include/bits/

libspirt:
	make -C $(DIR_LIBSPIRT) clean
	make -C $(DIR_LIBSPIRT)
	make -C $(DIR_LIBSPIRT) spi_rt_main

libmediastreamer: libspeex libortp
	@echo  make $(DIR_LIBMEDIASTREAMER) lib
	@echo $(shell pwd)/$(DIR_LIBMEDIASTREAMER)
	@echo $(shell pwd)
	echo "                                         "
	echo "                                         "
	echo "                                         "
	cd $(DIR_LIBMEDIASTREAMER); \
	./autogen.sh; \
	./configure --prefix=$(shell pwd)/$(DIR_LIBMEDIASTREAMER)/../phone --host=mips-linux --with-gnu-ld --disable-static  --enable-macsnd=no --enable-video=no --enable-tests --disable-gsm PKG_CONFIG_PATH=$(shell pwd)/$(DIR_LIBMEDIASTREAMER)/../phone/lib/pkgconfig ;\
	make ; make install	
	

	



libortp:
	cd $(DIR_LIBORTP); \
	./autogen.sh; \
	./configure --prefix=$(shell pwd)/$(DIR_LIBORTP)/../phone --host=mips-linux --with-gnu-ld --disable-static ;\
	make ;make install
	




libspeex: libogg
	cd $(DIR_LIBSPEEX); \
	./configure --prefix=$(shell pwd)/$(DIR_LIBSPEEX)/../phone --host=mips-linux --with-gnu-ld --disable-static --enable-fixed-point --disable-float-api  --with-ogg=$(shell pwd)/$(DIR_LIBSPEEX)/../phone;\
	make ;make install




libogg:
	cd $(DIR_LIBOGG); \
	./configure --prefix=$(shell pwd)/$(DIR_LIBOGG)/../phone --host=mips-linux --with-gnu-ld --disable-static;\
	make ;make install


clean:
	$(RM) *.la *.so *.so.1 *.so.1.0.0 *.so.*  *.o  $(RESET_EXE)

	$(MAKE) -C $(DIR_LIBOGG) uninstall 
	$(MAKE) -C $(DIR_LIBOGG) clean 
	$(MAKE) -C $(DIR_LIBORTP) uninstall 
	$(MAKE) -C $(DIR_LIBORTP) clean 
	$(MAKE) -C $(DIR_LIBSPEEX) uninstall 
	$(MAKE) -C $(DIR_LIBSPEEX) clean 
	if test -f  mediastreamer-2.8.2/Makefile ; then echo mediastreamer-2.8.2/Makefile exist;\
		$(MAKE) -C $(DIR_LIBMEDIASTREAMER) uninstall ;\
		$(MAKE) -C $(DIR_LIBMEDIASTREAMER) clean ; \
	else echo mediastreamer-2.8.2/Makefile not exist; \
	fi

	$(MAKE) -C $(DIR_LIBSPIRT) clean

	$(MAKE) -C $(DIR_PSTN_APP) clean
	

