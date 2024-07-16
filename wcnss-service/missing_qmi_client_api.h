/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef WCNSS_QMI_OSS
/* From TARGET_WCNSS_QMI_INCLUDE_DIR */
#include <missing_constants.h>

/* Decide whether to use the DMS shim */
#if !defined (DMS_V01_IDL_MAJOR_VERS) && !defined(DMS_V01_IDL_MINOR_VERS) && !defined(DMS_V01_IDL_TOOL_VERS)
#define ENABLE_DMS_SHIM
#endif

/* Missing magic */
#define QMI_NO_ERR 0
#define QMI_CLIENT_INSTANCE_ANY 0xffff
#define DMS_DEVICE_MAC_WLAN_V01 0
#define QMI_DMS_GET_MAC_ADDRESS_REQ_V01 0x5Cull

/* Local functions */
int missing_client_api_init();
void *dms_get_service_object_shimshim_v01();

/* Library handles */
void *libqmiservices_handle;
void *libqmi_cci_handle;

/* Missing QMI/QCCI functions */
void *(*dms_get_service_object_internal_v01)(int32_t, int32_t, int32_t);
#ifdef ENABLE_DMS_SHIM
#define dms_get_service_object_v01() dms_get_service_object_shimshim_v01()
#else
#define dms_get_service_object_v01() dms_get_service_object_internal_v01( \
			DMS_V01_IDL_MAJOR_VERS, DMS_V01_IDL_MINOR_VERS, \
			DMS_V01_IDL_TOOL_VERS)
#endif

int (*qmi_client_init_instance)(void*, unsigned int, void*, void**, void**, uint32_t, void**);
int (*qmi_client_send_msg_sync)(void*, unsigned int, void*, unsigned int,
								void*, unsigned int, unsigned int);
int (*qmi_client_release)(void*);

/* Missing types */
typedef int qmi_client_error_type;
typedef void *qmi_client_type;
typedef void *qmi_client_os_params;

typedef struct {
	int device;
} dms_get_mac_address_req_msg_v01;

typedef struct {
	uint8_t  pad1[8];
	uint8_t  mac_address_valid;
	uint32_t mac_address_len;
	uint8_t  mac_address[6];
	uint8_t  pad2[2];
} dms_get_mac_address_resp_msg_v01;
#endif
