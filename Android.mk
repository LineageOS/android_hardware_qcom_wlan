WLAN_PATH := $(call my-dir)

ifeq ($(WLAN_PATH),$(call project-path-for,wlan))
include $(call first-makefiles-under,$(WLAN_PATH))
endif
