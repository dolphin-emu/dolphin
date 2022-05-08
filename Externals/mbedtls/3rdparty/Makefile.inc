THIRDPARTY_DIR = $(dir $(lastword $(MAKEFILE_LIST)))
include $(THIRDPARTY_DIR)/everest/Makefile.inc
