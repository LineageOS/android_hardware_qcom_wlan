# TODO:  Fix this properly when b/37901207 is fixed
#ifneq ($(BOARD_IS_AUTOMOTIVE),true)
WLAN_PATH := $(call my-dir)

ifeq ($(WLAN_PATH),$(call project-path-for,wlan))
include $(call all-subdir-makefiles)
endif
