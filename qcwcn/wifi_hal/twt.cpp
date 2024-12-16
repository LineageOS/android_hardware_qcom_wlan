/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "twtCommand.h"

TwtCommand::TwtCommand(wifi_handle handle, int id, u32 vendor_id, u32 subcmd)
        : WifiVendorCommand(handle, id, vendor_id, subcmd)
{
    memset(&mHandler, 0, sizeof(mHandler));
    mTWTCapabilities = NULL;
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

wifi_error TwtCommand::requestResponse()
{
    return WifiCommand::requestResponse(mMsg);
}

void TwtCommand::setSubCmd(u32 subcmd)
{
    mSubcmd = subcmd;
}

void TwtCommand::setTWTRequestType(enum qca_wlan_twt_operation event)
{
    mTWTRequestType = event;
}

void TwtCommand::setTwtCapabilities(wifi_twt_capabilities* capabilities)
{
    mTWTCapabilities = capabilities;
}

int TwtCommand::handleResponse(WifiEvent &reply)
{
    WifiVendorCommand::handleResponse(reply);
    u16 self_capabilities = 0;

    if (mSubcmd != QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT)
        return NL_SKIP;

    switch (mTWTRequestType) {
        case QCA_WLAN_TWT_GET_CAPABILITIES:
            struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MAX + 1];

            nla_parse_nested(tb_vendor, QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MAX,
                            (struct nlattr *)mVendorData, NULL);
            if (!tb_vendor) {
                ALOGE("TWT capabilities nested attributes is null");
                return NL_SKIP;
            }

            ALOGV("QCA_WLAN_TWT_GET_CAPABILITIES response Received");

            if (tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_SELF])
                self_capabilities =
                    get_u16(tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_SELF]);
            else {
                ALOGE("Get capabilities self attribute is not present");
                return NL_SKIP;
            }

            mTWTCapabilities->is_twt_requester_supported =
                (self_capabilities & QCA_WLAN_TWT_CAPA_REQUESTOR) ? 1 : 0;
            mTWTCapabilities->is_twt_responder_supported =
                (self_capabilities & QCA_WLAN_TWT_CAPA_RESPONDER) ? 1 : 0;
            mTWTCapabilities->is_broadcast_twt_supported =
                (self_capabilities & QCA_WLAN_TWT_CAPA_BROADCAST) ? 1 : 0;
            mTWTCapabilities->is_flexible_twt_supported =
                (self_capabilities & QCA_WLAN_TWT_CAPA_FLEXIBLE) ? 1 : 0;

            if (tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MIN_WAKE_DURATION])
                mTWTCapabilities->min_wake_duration_micros =
                    get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MIN_WAKE_DURATION]);
            else
                ALOGE("min wake duration attribute is not present");

            if (tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MAX_WAKE_DURATION])
                mTWTCapabilities->max_wake_duration_micros =
                    get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MAX_WAKE_DURATION]);
            else
                ALOGE("max wake duration attribute is not present");

            if (tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MIN_WAKE_INTVL])
                mTWTCapabilities->min_wake_interval_micros =
                    get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MIN_WAKE_INTVL]);
            else
                ALOGE("min wake interval attribute is not present here");

            if (tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MAX_WAKE_INTVL])
                mTWTCapabilities->max_wake_interval_micros =
                    get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MAX_WAKE_INTVL]);
            else
                ALOGE("max wake interval attribute is not present");

            ALOGV("TWT caps: %s%s%s%s SP:[min:%d max:%d] SI:[min:%d max:%d]",
                  mTWTCapabilities->is_twt_requester_supported ? "[Requestor]" : "",
                  mTWTCapabilities->is_twt_responder_supported ? "[Responder]" : "",
                  mTWTCapabilities->is_broadcast_twt_supported ? "[Broadcast]" : "",
                  mTWTCapabilities->is_flexible_twt_supported ? "[Flexible]" : "",
                  mTWTCapabilities->min_wake_duration_micros,
                  mTWTCapabilities->max_wake_duration_micros,
                  mTWTCapabilities->min_wake_interval_micros,
                  mTWTCapabilities->max_wake_interval_micros);
            break;
        default:
            break;
    }

    return NL_SKIP;
}

wifi_error wifi_twt_get_capabilities(wifi_interface_handle iface,
                                     wifi_twt_capabilities* capabilities)
{
    wifi_error ret;
    TwtCommand *ptwtCommand;
    struct nlattr *nlData;
    interface_info *info;
    wifi_handle handle;

    if(!iface) {
        ALOGE("%s: iface is NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    info = getIfaceInfo(iface);
    if (!info) {
        ALOGE("%s: info is NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    handle = getWifiHandle(iface);
    ptwtCommand = TwtCommand::instance(handle);
    if (ptwtCommand == NULL) {
        ALOGE("%s: Error TwtCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    if (capabilities == NULL) {
        ALOGE("%s: capabilities is NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    memset(capabilities, 0, sizeof(wifi_twt_capabilities));

    ptwtCommand->setSubCmd(QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
    ptwtCommand->setTWTRequestType(QCA_WLAN_TWT_GET_CAPABILITIES);

    /* Create the NL message. */
    ret = ptwtCommand->create();
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /* Set the interface Id of the message. */
    ret = ptwtCommand->set_iface_id(info->name);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /* Add the vendor specific attributes for the NL command. */
    nlData = ptwtCommand->attr_start(NL80211_ATTR_VENDOR_DATA);
    if (!nlData){
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }

    ret = ptwtCommand->put_u8(QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION,
                              QCA_WLAN_TWT_GET_CAPABILITIES);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ptwtCommand->attr_end(nlData);

    ptwtCommand->setTwtCapabilities(capabilities);

    ret = ptwtCommand->requestResponse();
    if (ret != WIFI_SUCCESS)
        goto cleanup;

cleanup:
    if (ret != WIFI_SUCCESS)
        ALOGE("%s: Error:%d", __FUNCTION__, ret);

    return ret;
}