/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "twtCommand.h"

TwtCommand::TwtCommand(wifi_handle handle, int id, u32 vendor_id, u32 subcmd)
        : WifiVendorCommand(handle, id, vendor_id, subcmd)
{
    memset(&mHandler, 0, sizeof(mHandler));
}

TwtCommand::~TwtCommand()
{
    unregisterVendorHandler(mVendor_id, QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
}

TwtCommand* TwtCommand::instance(wifi_handle handle)
{
    if (handle == NULL) {
        ALOGE("Interface Handle is invalid");
        return NULL;
    }
    hal_info* info = getHalInfo(handle);
    if (!info || !info->twt_cmd_handler) {
        ALOGE("twt_cmd_handler is invalid");
        return NULL;
    }

    TwtCommand* pTwtCommand = info->twt_cmd_handler->pTwtCommand;

    if (pTwtCommand == NULL) {
        pTwtCommand = new TwtCommand(handle, 0,
                OUI_QCA,
                QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
        ALOGV("TwtCommand instance is created");
        info->twt_cmd_handler->pTwtCommand = pTwtCommand;
    } else if (handle != getWifiHandle(pTwtCommand->mInfo)) {
        /*
         * upper layer must have cleaned up the handle and reinitialized,
         * so we need to update the same.
         */
        ALOGV("Handle is different, update the handle");
        pTwtCommand->mInfo = (hal_info *)handle;
    }

    ALOGV("TwtCommand instance has already been created");

    return pTwtCommand;
}

wifi_error TwtCommand::setCallbackHandler(wifi_twt_events handler)
{
    wifi_error res;
    mHandler = handler;

    res = registerVendorHandler(mVendor_id,
                                QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
    if (res != WIFI_SUCCESS) {
        ALOGE("%s: Unable to register Vendor Handler Vendor Id=0x%x"
              "subcmd=QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT", __FUNCTION__, mVendor_id);
        return res;
    }

    return res;
}

wifi_error wifi_twt_register_events(wifi_interface_handle iface,
                                    wifi_twt_events events)
{
    TwtCommand *pTwtCommand = NULL;
    wifi_handle wifiHandle;
    hal_info *info;

    ALOGV("%s:Enter", __FUNCTION__);

    if (!iface) {
        ALOGE("%s: iface is NULL");
        return WIFI_ERROR_UNKNOWN;
    }

    wifiHandle = getWifiHandle(iface);
    info = getHalInfo(wifiHandle);
    if (!info) {
        ALOGE("%s: Hal Info is NULL");
        return WIFI_ERROR_UNKNOWN;
    }

    if (!info->twt_cmd_handler) {
        info->twt_cmd_handler = (twt_cmd_handler *)malloc(sizeof(twt_cmd_handler));
        if (info->twt_cmd_handler == NULL) {
            ALOGE("%s: Allocation of twt handler failed",__FUNCTION__);
            return WIFI_ERROR_OUT_OF_MEMORY;
        }
        info->twt_cmd_handler->pTwtCommand = NULL;
    }

    pTwtCommand = TwtCommand::instance(wifiHandle);
    if (!pTwtCommand) {
        ALOGE("%s: Error while fetching twtCommand instance", __FUNCTION__);
        free(info->twt_cmd_handler);
        info->twt_cmd_handler = NULL;
        return WIFI_ERROR_UNKNOWN;
    }

    return pTwtCommand->setCallbackHandler(events);
}

void cleanupTwtCommand(hal_info *info)
{
    TwtCommand *pTwtCommand;

    ALOGV("%s:Enter", __FUNCTION__);
    if (info == NULL || info->twt_cmd_handler == NULL)
        return;

    pTwtCommand = info->twt_cmd_handler->pTwtCommand;
    if (pTwtCommand)
        delete pTwtCommand;

    free(info->twt_cmd_handler);
    info->twt_cmd_handler = NULL;

    return;
}