# Change the following to switch configurations:
#  (uncomment exactly one)
#include full.config
include no_enterprise.config
#include small.config
#include tiny.config

include ../common/Makefile.common

ALL=wpa_supplicant 
#  ? wpa_passphrase wpa_cli wcmd

all: $(ALL)
	echo wpa_supplicant recompiled ok

install: all
	cp -a $(ALL) $(INSTALL_ROOT)/sbin/.

# OBJS: object files for wpa_supplicant program
OBJS += config.o
OBJS += topology.o

ifndef CONFIG_BACKEND
CONFIG_BACKEND=file
endif

ifeq ($(CONFIG_BACKEND), file)
OBJS += config_file.o
# OBJS += base64.o
CFLAGS += -DCONFIG_BACKEND_FILE
endif

ifeq ($(CONFIG_BACKEND), winreg)
OBJS += config_winreg.o
endif

ifeq ($(CONFIG_BACKEND), none)
OBJS += config_none.o
endif

ifdef CONFIG_DRIVER_HOSTAP
CFLAGS += -DCONFIG_DRIVER_HOSTAP
OBJS_d += driver_hostap.o
CONFIG_WIRELESS_EXTENSION=y
endif

ifdef CONFIG_DRIVER_WEXT
CFLAGS += -DCONFIG_DRIVER_WEXT
CONFIG_WIRELESS_EXTENSION=y
endif

ifdef CONFIG_DRIVER_PRISM54
CFLAGS += -DCONFIG_DRIVER_PRISM54
OBJS_d += driver_prism54.o
CONFIG_WIRELESS_EXTENSION=y
endif

ifdef CONFIG_DRIVER_HERMES
CFLAGS += -DCONFIG_DRIVER_HERMES
OBJS_d += driver_hermes.o
CONFIG_WIRELESS_EXTENSION=y
endif

ifdef CONFIG_DRIVER_ATHEROS
CFLAGS += -DCONFIG_DRIVER_ATHEROS
OBJS_d += driver_atheros.o
CONFIG_WIRELESS_EXTENSION=y
endif

ifdef CONFIG_DRIVER_MADWIFI
CFLAGS += -DCONFIG_DRIVER_MADWIFI
OBJS_d += driver_madwifi.o
CONFIG_WIRELESS_EXTENSION=y
endif

ifdef CONFIG_DRIVER_ATMEL
CFLAGS += -DCONFIG_DRIVER_ATMEL
OBJS_d += driver_atmel.o
CONFIG_WIRELESS_EXTENSION=y
endif

ifdef CONFIG_DRIVER_NDISWRAPPER
CFLAGS += -DCONFIG_DRIVER_NDISWRAPPER
OBJS_d += driver_ndiswrapper.o
CONFIG_WIRELESS_EXTENSION=y
endif

ifdef CONFIG_DRIVER_BROADCOM
CFLAGS += -DCONFIG_DRIVER_BROADCOM
OBJS_d += driver_broadcom.o
endif

ifdef CONFIG_DRIVER_IPW
CFLAGS += -DCONFIG_DRIVER_IPW
OBJS_d += driver_ipw.o
CONFIG_WIRELESS_EXTENSION=y
endif

ifdef CONFIG_DRIVER_BSD
CFLAGS += -DCONFIG_DRIVER_BSD
OBJS_d += driver_bsd.o
ifndef CONFIG_L2_PACKET
CONFIG_L2_PACKET=freebsd
endif
endif

ifdef CONFIG_DRIVER_NDIS
CFLAGS += -DCONFIG_DRIVER_NDIS
OBJS_d += driver_ndis.o driver_ndis_.o
ifndef CONFIG_L2_PACKET
CONFIG_L2_PACKET=pcap
endif
CONFIG_WINPCAP=y
ifdef CONFIG_USE_NDISUIO
CFLAGS += -DCONFIG_USE_NDISUIO
endif
endif

ifdef CONFIG_DRIVER_WIRED
CFLAGS += -DCONFIG_DRIVER_WIRED
OBJS_d += driver_wired.o
endif

ifdef CONFIG_DRIVER_TEST
CFLAGS += -DCONFIG_DRIVER_TEST
OBJS_d += driver_test.o
endif

ifdef CONFIG_EAP_TLS
OBJS += eap_tls.o
endif

ifdef CONFIG_EAP_PEAP
OBJS += eap_peap.o
CONFIG_EAP_TLV=y
endif

ifdef CONFIG_EAP_TTLS
OBJS += eap_ttls.o
endif

ifdef CONFIG_EAP_MD5
OBJS += eap_md5.o
endif

ifdef CONFIG_EAP_MSCHAPV2
OBJS += eap_mschapv2.o
endif

ifdef CONFIG_EAP_GTC
OBJS += eap_gtc.o
endif

ifdef CONFIG_EAP_OTP
OBJS += eap_otp.o
endif

ifdef CONFIG_EAP_SIM
OBJS += eap_sim.o
endif

ifdef CONFIG_EAP_LEAP
OBJS += eap_leap.o
endif

ifdef CONFIG_EAP_PSK
OBJS += eap_psk.o
# OBJS += eap_psk_common.o
endif

ifdef CONFIG_EAP_AKA
OBJS += eap_aka.o
endif

ifdef CONFIG_EAP_SIM_COMMON
# OBJS += eap_sim_common.o
endif

ifdef CONFIG_EAP_TLV
OBJS += eap_tlv.o
endif

ifdef CONFIG_EAP_FAST
OBJS += eap_fast.o
endif

ifdef CONFIG_EAP_PAX
OBJS += eap_pax.o
endif

ifdef CONFIG_EAP_SAKE
OBJS += eap_sake.o
endif

ifdef CONFIG_EAP_GPSK
OBJS += eap_gpsk.o
endif

ifdef CONFIG_EAP_WPS
ifdef CONFIG_DRIVER_ATHEROS
CFLAGS += -DATH_WPS_IE
endif #CONFIG_DRIVER_ATHEROS
ifdef CONFIG_DRIVER_MADWIFI
CFLAGS += -DATH_WPS_IE
endif #CONFIG_DRIVER_MADWIFI
OBJS += eap_wps.o 
OBJS += wps_config.o
ifeq ($(CONFIG_WPS_OPT_UPNP),y)
OBJS += upnp_wps_ctrlpt.o 
OBJS += wps_opt_upnp.o
endif #CONFIG_WPS_OPT_UPNP
ifdef CONFIG_WPS_OPT_NFC
OBJS += wps_opt_nfc.o
endif #CONFIG_WPS_OPT_NFC
endif #CONFIG_EAP_WPS

ifdef CONFIG_IEEE8021X_EAPOL
# IEEE 802.1X/EAPOL state machines (e.g., for RADIUS authentication)
OBJS += eapol_sm.o eap.o eap_methods.o
endif

ifdef CONFIG_PCSC
# PC/SC interface for smartcards (USIM, GSM SIM)
OBJS += pcsc_funcs.o
endif

ifdef TLS_FUNCS
# Shared TLS functions (needed for EAP_TLS, EAP_PEAP, EAP_TTLS, and EAP_FAST)
OBJS += eap_tls_common.o
else  # TLS_FUNCS
endif # TLS_FUNCS

ifdef CONFIG_PKCS12
endif

ifdef CONFIG_SMARTCARD
endif

ifdef MS_FUNCS
endif

ifdef NEED_CRYPTO
ifndef TLS_FUNCS
ifeq ($(CONFIG_TLS), openssl)
endif
ifeq ($(CONFIG_TLS), gnutls)
endif
ifeq ($(CONFIG_TLS), schannel)
endif
ifeq ($(CONFIG_TLS), internal)
ifeq ($(CONFIG_CRYPTO), libtomcrypt)
endif
endif
endif
ifeq ($(CONFIG_TLS), openssl)
endif
ifeq ($(CONFIG_TLS), gnutls)
endif
ifeq ($(CONFIG_TLS), schannel)
endif
ifeq ($(CONFIG_TLS), internal)
ifeq ($(CONFIG_CRYPTO), libtomcrypt)
endif
ifeq ($(CONFIG_CRYPTO), internal)
endif
ifeq ($(CONFIG_CRYPTO), cryptoapi)
endif
endif
ifeq ($(CONFIG_TLS), none)
endif
else
endif


ifdef NEED_SHA256
endif

ifdef CONFIG_WIRELESS_EXTENSION
CFLAGS += -DCONFIG_WIRELESS_EXTENSION
OBJS_d += driver_wext.o
endif

ifdef CONFIG_CTRL_IFACE
ifeq ($(CONFIG_CTRL_IFACE), y)
endif
ifeq ($(CONFIG_CTRL_IFACE), unix)
endif
ifeq ($(CONFIG_CTRL_IFACE), udp)
endif
ifeq ($(CONFIG_CTRL_IFACE), named_pipe)
endif
OBJS += ctrl_iface.o ctrl_iface_$(CONFIG_CTRL_IFACE).o
endif

ifdef CONFIG_CTRL_IFACE_DBUS
CFLAGS += -DCONFIG_CTRL_IFACE_DBUS -DDBUS_API_SUBJECT_TO_CHANGE
OBJS += ctrl_iface_dbus.o ctrl_iface_dbus_handlers.o dbus_dict_helpers.o
LIBS += `pkg-config --libs dbus-1`
CFLAGS += `pkg-config --cflags dbus-1`
endif

ifdef CONFIG_NATIVE_WINDOWS
CFLAGS += -DCONFIG_NATIVE_WINDOWS
LIBS += -lws2_32 -lgdi32 -lcrypt32
LIBS_c += -lws2_32
LIBS_p += -lws2_32
ifeq ($(CONFIG_CRYPTO), cryptoapi)
LIBS_p += -lcrypt32
endif
endif

ifdef CONFIG_NO_STDOUT_DEBUG
ifndef CONFIG_CTRL_IFACE
endif
endif

ifdef CONFIG_PEERKEY
endif

ifdef CONFIG_TDLS
CFLAGS += -DCONFIG_TDLS
CFALGS += -DUMAC_SUPPORT_TDLS
CFLAGS += -I../../wpa/wsc/lib/openssl-0.9.8a/include/
#LIBS += -lcrypto
endif

ifdef CONFIG_IEEE80211W
endif

ifndef CONFIG_NO_WPA
OBJS += wpa.o preauth.o pmksa_cache.o
else
endif

ifdef CONFIG_NO_WPA2
endif

ifdef CONFIG_NO_AES_EXTRAS
endif

ifdef CONFIG_CLIENT_MLME
OBJS += mlme.o
endif

ifndef CONFIG_MAIN
endif

OBJS += wpa_supplicant.o events.o
OBJS += $(CONFIG_MAIN).o drivers.o $(OBJS_d)

ifndef LDO
LDO=$(CC)
LDFLAGS=$(CFLAGS)
endif

dynamic_eap_methods: $(EAPDYN)

wpa_supplicant: $(CONFIG_FILE) $(OBJS) $(COMMON_LIBS)
	$(LDO) $(LDFLAGS) -o wpa_supplicant $(OBJS) $(LIBS)

#eapol_test: $(CONFIG_FILE) $(OBJS_t)
#	$(LDO) $(LDFLAGS) -o eapol_test $(OBJS_t) $(LIBS)

preauth_test: $(CONFIG_FILE) $(OBJS_t2) 
	$(LDO) $(LDFLAGS) -o preauth_test $(OBJS_t2) $(LIBS)

wpa_passphrase: $(OBJS_p)
	$(LDO) $(LDFLAGS) -o wpa_passphrase $(OBJS_p) $(LIBS_p)

wpa_cli: $(OBJS_c)
	$(LDO) $(LDFLAGS) -o wpa_cli $(OBJS_c) $(LIBS_c)

wcmd: $(OBJS_wcmd)
	$(LDO) $(LDFLAGS) -o wcmd $(OBJS_wcmd) $(LIBS_c)

OBJSa=asn1_test.o asn1.o x509v3.o common.o os_unix.o \
	crypto_$(CONFIG_CRYPTO).o md5.o sha1.o \
	rc4.o des.o aes_wrap.o \
	bignum.o rsa.o
asn1_test: $(OBJSa)
	$(LDO) $(LDFLAGS) -o asn1_test $(OBJSa)

OBJSx=tests/test_x509v3.o asn1.o x509v3.o \
	common.o os_unix.o \
	crypto_$(CONFIG_CRYPTO).o \
	md5.o sha1.o \
	rc4.o des.o aes_wrap.o \
	bignum.o rsa.o
test_x509v3: $(OBJSx)
	$(LDO) $(LDFLAGS) -o test_x509v3 $(OBJSx)

win_if_list: win_if_list.c
	$(LDO) $(LDFLAGS) -o $@ win_if_list.c $(CFLAGS) $(LIBS_w)

eap_psk.so: eap_psk.c eap_psk_common.c
	$(CC) -o $@ $(CFLAGS) -shared -rdynamic -fPIC $^ \
		-Deap_peer_psk_register=eap_peer_method_dynamic_init

eap_pax.so: eap_pax.c eap_pax_common.c
	$(CC) -o $@ $(CFLAGS) -shared -rdynamic -fPIC $^ \
		-Deap_peer_pax_register=eap_peer_method_dynamic_init

eap_sake.so: eap_sake.c eap_sake_common.c
	$(CC) -o $@ $(CFLAGS) -shared -rdynamic -fPIC $^ \
		-Deap_peer_sake_register=eap_peer_method_dynamic_init

%.so: %.c
	$(CC) -o $@ $(CFLAGS) -shared -rdynamic -fPIC $< \
		-D$(*:eap_%=eap_peer_%)_register=eap_peer_method_dynamic_init


wpa_supplicant.exe: wpa_supplicant
	mv -f $< $@
wpa_cli.exe: wpa_cli
	mv -f $< $@
wpa_passphrase.exe: wpa_passphrase
	mv -f $< $@
win_if_list.exe: win_if_list
	mv -f $< $@
eapol_test.exe: eapol_test
	mv -f $< $@

WINALL=wpa_supplicant.exe wpa_cli.exe wpa_passphrase.exe win_if_list.exe

windows-bin: $(WINALL)
	$(STRIP) $(WINALL)

wpa_gui/Makefile:
	qmake -o wpa_gui/Makefile wpa_gui/wpa_gui.pro 

wpa_gui: wpa_gui/Makefile
	$(MAKE) -C wpa_gui

TEST_MS_FUNCS_OBJS = crypto.o sha1.o md5.o \
	os_unix.o rc4.o tests/test_ms_funcs.o
test-ms_funcs: $(TEST_MS_FUNCS_OBJS)
	$(LDO) $(LDFLAGS) -o $@ $(TEST_MS_FUNCS_OBJS) $(LIBS) -lcrypto
	./test-ms_funcs
	rm test-ms_funcs

TEST_SHA1_OBJS = sha1.o md5.o tests/test_sha1.o #crypto.o
test-sha1: $(TEST_SHA1_OBJS)
	$(LDO) $(LDFLAGS) -o $@ $(TEST_SHA1_OBJS) $(LIBS)
	./test-sha1
	rm test-sha1

TEST_SHA256_OBJS = sha256.o md5.o tests/test_sha256.o crypto.o
test-sha256: $(TEST_SHA256_OBJS)
	$(LDO) $(LDFLAGS) -o $@ $(TEST_SHA256_OBJS) $(LIBS)
	./test-sha256
	rm test-sha256

TEST_AES_OBJS = aes_wrap.o tests/test_aes.o # crypto.o
test-aes: $(TEST_AES_OBJS)
	$(LDO) $(LDFLAGS) -o $@ $(TEST_AES_OBJS) $(LIBS)
	./test-aes
	rm test-aes

TEST_EAP_SIM_COMMON_OBJS = sha1.o md5.o \
	aes_wrap.o common.o os_unix.o \
	tests/test_eap_sim_common.o
test-eap_sim_common: $(TEST_EAP_SIM_COMMON_OBJS)
	$(LDO) $(LDFLAGS) -o $@ $(TEST_AES_OBJS) $(LIBS)
	./test-eap_sim_common
	rm test-eap_sim_common

TEST_MD4_OBJS = md4.o tests/test_md4.o #crypto.o
test-md4: $(TEST_MD4_OBJS)
	$(LDO) $(LDFLAGS) -o $@ $(TEST_MD4_OBJS) $(LIBS)
	./test-md4
	rm test-md4

TEST_MD5_OBJS = md5.o tests/test_md5.o #crypto.o
test-md5: $(TEST_MD5_OBJS)
	$(LDO) $(LDFLAGS) -o $@ $(TEST_MD5_OBJS) $(LIBS)
	./test-md5
	rm test-md5

tests: test-ms_funcs test-sha1 test-aes test-eap_sim_common test-md4 test-md5

clean:
	rm -f core junk* *~ *.o *.d eap_*.so $(ALL) $(WINALL)

%.eps: %.fig
	fig2dev -L eps $*.fig $*.eps

%.png: %.fig
	fig2dev -L png -m 3 $*.fig | pngtopnm | pnmscale 0.4 | pnmtopng \
		> $*.png

docs-pics: doc/wpa_supplicant.png doc/wpa_supplicant.eps

docs: docs-pics
	doxygen doc/doxygen.full
	$(MAKE) -C doc/latex
	cp doc/latex/refman.pdf wpa_supplicant-devel.pdf

docs-fast: docs-pics
	doxygen doc/doxygen.fast

clean-docs:
	rm -rf doc/latex doc/html
	rm -f doc/wpa_supplicant.{eps,png} wpa_supplicant-devel.pdf

-include $(OBJS:%.o=%.d)
