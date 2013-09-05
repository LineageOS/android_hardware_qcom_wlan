ifeq ($(TARGET_ARCH),arm)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := wcnss_service
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/common/inc/
LOCAL_SRC_FILES := wcnss_service.c
LOCAL_SHARED_LIBRARIES := libc libcutils libutils
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -Wall
include $(BUILD_EXECUTABLE)
endif
