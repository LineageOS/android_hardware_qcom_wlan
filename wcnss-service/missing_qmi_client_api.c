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
#include <log/log.h>
#include <dlfcn.h>
#include "missing_qmi_client_api.h"

#define SUCCESS 0
#define FAILED -1

#define QMI_CCI_LIB "libqmi_cci.so"
#define QMISERVICES_LIB "libqmiservices.so"

int missing_client_api_init()
{
    /* Get a handle of libqmi_cci.so */
    libqmi_cci_handle = dlopen(QMI_CCI_LIB, RTLD_NOW);
    if (!libqmi_cci_handle) {
        ALOGE("%s: Failed to open %s: %s", __func__, QMI_CCI_LIB, dlerror());
        goto exit;
    }

    qmi_client_init_instance = dlsym(libqmi_cci_handle, "qmi_client_init_instance");
    if (!qmi_client_init_instance) {
        ALOGE("%s: Failed to resolve function: %s: %s", __func__, "qmi_client_init_instance", dlerror());
        goto exit;
    }

    qmi_client_send_msg_sync = dlsym(libqmi_cci_handle, "qmi_client_send_msg_sync");
    if (!qmi_client_send_msg_sync) {
        ALOGE("%s: Failed to resolve function: %s: %s", __func__, "qmi_client_send_msg_sync", dlerror());
        goto exit;
    }

    qmi_client_release = dlsym(libqmi_cci_handle, "qmi_client_release");
    if (!qmi_client_release) {
        ALOGE("%s: Failed to resolve function: %s: %s", __func__, "qmi_client_release", dlerror());
        goto exit;
    }

    /* Get a handle of libqmiservices.so */
    libqmiservices_handle = dlopen(QMISERVICES_LIB, RTLD_NOW);
    if (!libqmiservices_handle) {
        ALOGE("%s: Failed to open %s: %s", __func__, QMISERVICES_LIB, dlerror());
        goto exit;
    }

    dms_get_service_object_internal_v01 = dlsym(libqmiservices_handle, "dms_get_service_object_internal_v01");
    if (!dms_get_service_object_internal_v01) {
        ALOGE("%s: Failed to resolve function: %s: %s", __func__, "dms_get_service_object_internal_v01", dlerror());
        goto exit;
    }
    return SUCCESS;

exit:
    return FAILED;
}

#ifdef ENABLE_DMS_SHIM
void *dms_get_service_object_shimshim_v01()
{
    for (int i = 0; i < 255; ++i)
    {
        /* Only DMS_V01_IDL_MINOR_VERS is observed to vary between different blobs */
        void* result = dms_get_service_object_internal_v01(1, i, 6);
        if (result)
        {
            ALOGI("%s: Found magic value: %d", __func__, i);
            return result;
        }
    }
    // Nothing found
    return 0;
}
#endif
#endif
