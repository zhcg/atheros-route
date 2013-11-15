#/bin/sh
export PATH:=$(PATH):$(PWD)/../../build/gcc-3.4.4-2.16.1/build_mips/bin
export CC=mips-linux-uclibc-gcc
CROSS=$(subst -gcc,,$(CC)) 
all: ppp rp-pppoe 

ppp: 
	cd ppp-2.4.5 ;make  all 

rp-pppoe: 
	cd rp-pppoe-3.8/src/ ;make 

configure: ppp_conf rp-pppoe_conf 

ppp_conf: 
	cd ppp-2.4.5 ;./configure  --prefix=$(prefix) CC=$(CC) 

rp-pppoe_conf: 
	cd rp-pppoe-3.8/src/ ;./configure --enable-plugin=$(PWD)/ppp-2.4.5 --prefix=$(prefix)/usr --host=$(CROSS) CC=$(CC)  


install: ppp_ins rp-pppoe_ins 

ppp_ins: 
	cd ppp-2.4.5 ;make INSTROOT=$(INSTROOT) install 

rp-pppoe_ins: 
	cd rp-pppoe-3.8/src/ ;make RPM_INSTALL_ROOT=$(INSTROOT) install 

clean: ppp_clean rp-pppoe_clean 

ppp_clean: 
	cd ppp-2.4.5 ;make clean 

rp-pppoe_clean: 
	cd rp-pppoe-3.8/src ; make clean
