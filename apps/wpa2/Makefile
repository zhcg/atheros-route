# Makefile for wpa2 software
#
# This Makefile may be run "standalone" to create native software
# for a Linux PC.
# For cross-compiling, some pre-definitions are needed,
# see "buildmips" script as an example.
# (You can still do "make clean" even if cross-compiling).

# Pseudo-targets:
# 	clean -- remove all(?) created files
# 	firsttime -- do clean, then initialize all subdirectories
# 	update (default if .firsttime exists) -- limited rebuild
# 			suitable for typical source file changes
# With no arguments, does update (and firsttime if .firsttime does not exist)
#
# Environmental variables used:
# INSTALL_ROOT=...  where to put output files
#       If missing, uses install subdirectory (useful for development
#       on a linux PC).
# BUILD_WPA2_DEBUG=y to turn off optimizations and staticly linked
# 	in order to ease debugging.
# BUILD_WPA2_STATIC_COMMON_LIBRARY=y to staticly link common library
#       instead of using shared object file.
# BUILD_WPA2_NO_HOSTAPD=y to disable building hostapd.
# BUILD_WPA2_NO_WPA_SUPPLICANT=y to disable building wpa_supplicant.
# BUILD_WPA2_NO_WPATALK=y to disable building wpatalk.
# BUILD_WPA2_NO_APSTART=y to disable building apstart.
# BUILD_WPA2_NO_EXAMPLES=y to disable installing example files to /etc/wpa2.
#
# Output files:
# 	.firsttime -- writtten after "firsttime" make
# 		If missing, "update" will do "firsttime"
# 	$(INSTALL_ROOT) -- important run-time files copied here,
# 		and also some files for cross-module compilation.
# 		(By default this is "install").

ifndef INSTALL_ROOT
INSTALL_ROOT=$(shell pwd)/install
endif

# If building only one of wpa_supplicant and hostapd, then static link
# makes most sense... else shared object file (default) makes most sense.
ifeq ($(BUILD_WPA2_NO_WPA_SUPPLICANT),y)
ifndef BUILD_WPA2_STATIC_COMMON_LIBRARY
export BUILD_WPA2_STATIC_COMMON_LIBRARY=y
endif
endif
ifeq ($(BUILD_WPA2_NO_HOSTAPD),y)
ifndef BUILD_WPA2_STATIC_COMMON_LIBRARY
export BUILD_WPA2_STATIC_COMMON_LIBRARY=y
endif
endif

# Keep "update" first so it is default
update: 
	if [ ! -f .firsttime ] ; then echo MUST DO FIRSTTIME BUILD FIRST; $(MAKE) firsttime; fi
	@echo DOING UPDATE
	@echo UPDATING tags file
	ctags `find . -name '*.h' -o -name '*.c' -o -name '*.cpp'`
	mkdir -p $(INSTALL_ROOT)
	mkdir -p $(INSTALL_ROOT)/sbin
	mkdir -p $(INSTALL_ROOT)/lib
	mkdir -p $(INSTALL_ROOT)/etc/wpa2
	mkdir -p $(INSTALL_ROOT)/include
#was# ifeq ($(CONFIG_TINYUPNP),y)
#was# else
#was# 	@echo UPDATING libupnp
#was# 	cd libupnp && $(MAKE) -f Makefile && $(MAKE) -f Makefile.host install
#was# endif
	#couldbe# @echo UPDATING openssl
	#couldbe# cd openssl && $(MAKE) -f Makefile.host && $(MAKE) -f Makefile.host install
	@echo UPDATING wireless_tools
	cd wireless_tools && $(MAKE) install
	@echo UPDATING common
	cd common && $(MAKE) install
ifeq ($(BUILD_WPA2_NO_WPA_SUPPLICANT),y)
else
	@echo UPDATING wpa_supplicant
	cd wpa_supplicant && $(MAKE) install
endif
ifeq ($(BUILD_WPA2_NO_HOSTAPD),y)
else
	@echo UPDATING hostapd
	cd hostapd && $(MAKE) install
endif
ifeq ($(BUILD_WPA2_NO_WPATALK),y)
else
	@echo UPDATING wpatalk
	cd wpatalk && $(MAKE) install
endif
ifeq ($(BUILD_WPA2_NO_APSTART),y)
else
	@echo UPDATING apstart
	cd apstart && $(MAKE) install
endif
	#couldbe# @echo UPDATING wpa_supplicant/testbed_sta
	#couldbe# cd wpa_supplicant/testbed_sta && $(MAKE) 
	#couldbe# cp -a wpa_supplicant/testbed_sta/testbed_sta install/.
	#couldbe# @echo UPDATING hostapd/testbed_ap
	#couldbe# cd hostapd/testbed_ap && $(MAKE) -f Makefile.host
	#couldbe# cp -a hostapd/testbed_ap/testbed_ap install/.
ifeq ($(BUILD_WPA2_NO_EXAMPLES),y)
	@echo Copying eap_user files only
	chmod -R +w $(INSTALL_ROOT)/etc/wpa2
	cp -af examples/*.eap_user $(INSTALL_ROOT)/etc/wpa2/.
else
	@echo NEW runtime conf files
	chmod -R +w $(INSTALL_ROOT)/etc/wpa2
	cp -af examples/*.conf examples/*.eap_user examples/*.ap_radio examples/*.ap_bss examples/*.sta $(INSTALL_ROOT)/etc/wpa2/.
	cp -f examples/repeater_pass_configuration $(INSTALL_ROOT)/sbin/.
	#was# cp -a examples/web $(INSTALL_ROOT)/etc/wpa2/.
	chmod +w $(INSTALL_ROOT)/etc/wpa2/*
	@echo DO IF NECESSARY: update runtime conf files
endif
	@echo DONE with update


#was# LIBUPNP_CONFIGURE_OPTIONS :=
#was# ifeq ($(BUILD_WPA2_DEBUG),y)
#was# LIBUPNP_CONFIGURE_OPTIONS += --enable-debug
#was# endif
#was# ifdef CC
#was# ifeq ($(CC),gcc)
#was# # Assume not cross compiler
#was# else
#was# # The following assumes that a cross compiler $CC ends in -gcc ;
#was# # libupnp configuration wants the prefix part (not including hyphen!)
#was# # libupnp refers to the target as the "host"... sigh.
#was# LIBUPNP_COMPILER_PREFIX := $(subst -gcc,,$(CC))
#was# #### UGLY HACK, ASSUME IS MIPS IN THIS CASE
#was# LIBUPNP_CONFIGURE_OPTIONS += --host=mips-linux-uclibc
#was# ###doesn't work### LIBUPNP_CONFIGURE_OPTIONS += --host=$(LIBUPNP_COMPILER_PREFIX)
#was# endif
#was# endif
#was# 
#was# libupnp_firsttime:
#was# 	@echo DOING FIRST TIME BUILD OF libupnp
#was# 	echo CC is $(CC)
#was# 	cd libupnp && CFLAGS="$(COPTS) -g" ./configure 			\
#was# 		--prefix $(INSTALL_ROOT)		\
#was# 		--disable-largefile			\
#was# 		$(LIBUPNP_CONFIGURE_OPTIONS)

#was# ifeq ($(CONFIG_TINYUPNP),y)
firsttime:  clean 
#was# else
#was# firsttime: clean libupnp_firsttime
#was# endif
	@echo BUILD_WPA2_DEBUG is $(BUILD_WPA2_DEBUG)
	#couldbe# @echo DOING FIRST TIME BUILD OF openssl
	#couldbe# cd openssl && ./config --prefix=`cd .. && pwd`/install no-threads no-zlib no-shared
	#couldbe# @echo DOING FIRST TIME BUILD OF testbed_sta...
	#couldbe# cd wpa_supplicant/testbed_sta && ($(MAKE) -f Makefile.host distclean || true) && qmake-qt4 && chmod +x sta_start sta_end
	#couldbe# @echo DOING FIRST TIME BUILD OF testbed_ap...
	#couldbe# cd hostapd/testbed_ap && ($(MAKE) -f Makefile.host distclean || true) && qmake-qt4 && chmod +x ap_start ap_end
	echo Did first time inits >.firsttime
	@echo DONE with firsttime
	$(MAKE) update

clean:
	@echo DOING CLEAN
	-rm -rf install tags .firsttime
	-find . -name 'junk*' -exec rm -rf '{}' ';'
	-find . -name '.deps' -exec rm -rf '{}' ';'
	-find . -name '.dirstamp' -exec rm -rf '{}' ';'
	-find . -name '.obj' -exec rm -rf '{}' ';'
	-find . -name '.moc' -exec rm -rf '{}' ';'
	-find . -name '.ui' -exec rm -rf '{}' ';'
	-find . -name '.rnd' -exec rm -rf '{}' ';'
	-find . -name '.*.swp' -exec rm -rf '{}' ';'
	-find . -name '*.log' -exec rm -rf '{}' ';'
	#was# -(cd libupnp && $(MAKE) -f Makefile -k clean)
	#couldbe# -(cd openssl && $(MAKE) -f Makefile.host -k clean)
	#couldbe# -(cd madwifi.host && $(MAKE) -f Makefile.host -k clean)
	-(cd wireless_tools && $(MAKE) -k clean)
	-(cd common && $(MAKE) -k clean)
	-(cd hostapd && $(MAKE) -k clean)
	-(cd wpa_supplicant && $(MAKE) -k clean)
	-(cd wpatalk && $(MAKE) -k clean)
	-(cd apstart && $(MAKE) -k clean)
	#couldbe# -(cd hostapd/testbed_ap && $(MAKE) -f Makefile.host -k distclean)
	#couldbe# -(cd wpa_supplicant/testbed_sta && $(MAKE) -f Makefile.host -k distclean)
	## (cd WpsNfcLibrary && $(MAKE) -f Makefile.host -k clean)
	@echo All cleaned


