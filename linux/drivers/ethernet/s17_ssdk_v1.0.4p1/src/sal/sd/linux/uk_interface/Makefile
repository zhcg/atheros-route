LOC_DIR=src/sal/sd/linux/uk_interface
LIB=SAL

include $(PRJ_PATH)/make/config.mk

SRC_LIST=

ifeq (TRUE, $(UK_IF))
ifeq (KSLIB, $(MODULE_TYPE))
  SRC_LIST=sw_api_ks.c
endif

ifeq (USLIB, $(MODULE_TYPE))
  SRC_LIST=sw_api_us.c
endif
endif

include $(PRJ_PATH)/make/components.mk
include $(PRJ_PATH)/make/defs.mk
include $(PRJ_PATH)/make/target.mk

all: dep obj