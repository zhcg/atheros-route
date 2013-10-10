#
# Makefile for general filesystem caching code
#

fscache-y := \
	cache.o \
	cookie.o \
	fsdef.o \
	main.o \
	netfs.o \
	object.o \
	operation.o \
	page.o

ifneq ($(CONFIG_ATH_2X8),y)
fscache-$(CONFIG_PROC_FS) += proc.o
endif
fscache-$(CONFIG_FSCACHE_STATS) += stats.o
fscache-$(CONFIG_FSCACHE_HISTOGRAM) += histogram.o

obj-$(CONFIG_FSCACHE) := fscache.o
