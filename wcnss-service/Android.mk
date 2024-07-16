ifneq (,$(filter arm aarch64 arm64, $(TARGET_ARCH)))
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
ifeq ($(PRODUCT_VENDOR_MOVE_ENABLED),true)
LOCAL_VENDOR_MODULE := true
endif
LOCAL_MODULE := wcnss_service
LOCAL_HEADER_LIBRARIES += vendor_common_inc
LOCAL_SRC_FILES := wcnss_service.c
LOCAL_SHARED_LIBRARIES := libc libcutils libutils liblog
ifeq ($(strip $(TARGET_USES_QCOM_WCNSS_QMI)),true)
ifeq ($(TARGET_USES_WCNSS_MAC_ADDR_REV),true)
LOCAL_CFLAGS += -DWCNSS_QMI_MAC_ADDR_REV
endif
ifneq ($(QCPATH),)
LOCAL_CFLAGS += -DWCNSS_QMI -DMDM_DETECT
LOCAL_HEADER_LIBRARIES += libqmi_common_headers
LOCAL_SRC_FILES += wcnss_qmi_client.c device_management_service_v01.c
LOCAL_SHARED_LIBRARIES += libqmi_cci libqmi_common_so
LOCAL_HEADER_LIBRARIES += libmdmdetect_headers
LOCAL_SHARED_LIBRARIES += libmdmdetect
LOCAL_HEADER_LIBRARIES += libril-qc-qmi-services-headers
else
LOCAL_CFLAGS += -DWCNSS_QMI
LOCAL_SHARED_LIBRARIES += libdl
ifeq ($(TARGET_PROVIDES_WCNSS_QMI),true)
LOCAL_CFLAGS += -DWCNSS_QMI_OSS
else
LOCAL_SRC_FILES += wcnss_qmi_client.c missing_qmi_client_api.c
LOCAL_CFLAGS += -DWCNSS_QMI_PRIVATE
endif #TARGET_PROVIDES_WCNSS_QMI
ifneq ($(TARGET_WCNSS_QMI_INCLUDE_DIR),)
LOCAL_C_INCLUDES += $(TARGET_WCNSS_QMI_INCLUDE_DIR)
else
LOCAL_CFLAGS += -DENABLE_DMS_SHIM
endif #TARGET_WCNSS_QMI_INCLUDE_DIR
endif #QCPATH
endif #TARGET_USES_QCOM_WCNSS_QMI
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -Wall -Werror
include $(BUILD_EXECUTABLE)
endif
