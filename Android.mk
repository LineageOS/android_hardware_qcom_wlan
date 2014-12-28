WLAN_PATH := $(call my-dir)

ifeq ($(WLAN_PATH),$(call project-path-for,wlan))
include $(call all-subdir-makefiles)
endif
