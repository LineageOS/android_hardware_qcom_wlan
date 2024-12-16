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
    mRequestId = 0;
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

void TwtCommand::setReqId(wifi_request_id id)
{
    mRequestId = id;
}

void TwtCommand::setTwtFlowId(int flowId)
{
    mTwtFlowId = flowId;
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
        case QCA_WLAN_TWT_GET_STATS:
            struct nlattr *tb1[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_MAX + 1];
            struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAX + 1];
            struct nlattr *attr;
            wifi_twt_session_stats twtSessionStats;
            int flow_id, rem;

            if (nla_parse(tb1, QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_MAX,
                          (struct nlattr *) mVendorData, mDataLen, NULL)) {
                    ALOGE("Parse TWT get stats failed");
                    return NL_SKIP;
            }

            if (!tb1[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS]) {
                ALOGE("TWT get statistics nested attributes is null");
                return NL_SKIP;
            }

            nla_for_each_nested(attr, tb1[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS], rem) {
                if (nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_TWT_STATS_MAX,
                              (struct nlattr *)nla_data(attr), nla_len(attr), NULL)) {
                    ALOGE("TWT parse get stats failed");
                    return NL_SKIP;
                }

                flow_id = -1;
                if (tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_FLOW_ID])
                    flow_id = get_u8(tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_FLOW_ID]);
                else
                    ALOGE("TWT flow id attribute is not present");

                if (flow_id != mTwtFlowId) {
                    ALOGE("TWT flow id received:%d is invalid expected flow id:%d",
                          flow_id, mTwtFlowId);
                    return NL_SKIP;
                }

                memset(&twtSessionStats, 0, sizeof(twtSessionStats));
                if (tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_TX_MPDU])
                    twtSessionStats.avg_pkt_num_tx =
                        get_u32(tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_TX_MPDU]);
                else
                    ALOGE("Average TX MPDU attribute is not present");

                if (tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_RX_MPDU])
                    twtSessionStats.avg_pkt_num_rx =
                        get_u32(tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_RX_MPDU]);
                else
                    ALOGE("Average TX MPDU attribute is not present");

                if (tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_TX_PACKET_SIZE])
                    twtSessionStats.avg_tx_pkt_size =
                        get_u32(tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_TX_PACKET_SIZE]);
                else
                    ALOGE("Average TWT Stats average TX packets size attributes is not present");

                if (tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_RX_PACKET_SIZE])
                    twtSessionStats.avg_rx_pkt_size =
                        get_u32(tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVERAGE_RX_PACKET_SIZE]);
                else
                    ALOGE("Average RX packets size attributes is not present");

                if (tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVG_EOSP_DUR_US])
                    twtSessionStats.avg_eosp_dur_us =
                        get_u32(tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_AVG_EOSP_DUR_US]);
                else
                    ALOGE("Average eosp duration us attribute is not present");

                if (tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_EOSP_COUNT])
                    twtSessionStats.eosp_count =
                        get_u32(tb2[QCA_WLAN_VENDOR_ATTR_TWT_STATS_EOSP_COUNT]);
                else
                    ALOGE("eosp count attribute is not present");

                ALOGV("TWT Session stats: avg_pkt_num_tx:%d avg_pkt_num_rx:%d avg_tx_pkt_size:%d avg_rx_pkt_size:%d avg_eosp_dur_us:%d eosp_count:%d",
                      twtSessionStats.avg_pkt_num_tx,
                      twtSessionStats.avg_pkt_num_rx,
                      twtSessionStats.avg_tx_pkt_size,
                      twtSessionStats.avg_rx_pkt_size,
                      twtSessionStats.avg_eosp_dur_us,
                      twtSessionStats.eosp_count);

                if (mHandler.on_twt_session_stats)
                    (*mHandler.on_twt_session_stats)(mRequestId, flow_id,
                                                     twtSessionStats);
                else
                    ALOGE("TWT: session stats Callback is not registered:");
                /*
                 * Reset the flow id once response is received to avoid
                 * duplicate event processing
                 */
                mTwtFlowId = -1;
            }
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

wifi_error wifi_twt_session_get_stats(wifi_request_id id,
                                      wifi_interface_handle iface,
                                      int session_id)
{
    wifi_error ret;
    TwtCommand *ptwtCommand;
    struct nlattr *nlData, *nlTwtParams;
    interface_info *iinfo;
    wifi_handle handle;

    if(!iface){
        ALOGE("%s: iface is NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    iinfo = getIfaceInfo(iface);
    if (!iinfo) {
        ALOGE("%s: iinfo is NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    ALOGV("%s: Enter id:%d session_id:%d", __FUNCTION__, id, session_id);
    handle = getWifiHandle(iface);

    ptwtCommand = TwtCommand::instance(handle);
    if (ptwtCommand == NULL) {
        ALOGE("%s: Error TwtCommand is NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }
    ptwtCommand->setSubCmd(QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
    ptwtCommand->setTWTRequestType(QCA_WLAN_TWT_GET_STATS);
    ptwtCommand->setReqId(id);
    ptwtCommand->setTwtFlowId(session_id);

    /* Create the NL message. */
    ret = ptwtCommand->create();
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /* Set the interface Id of the message. */
    ret = ptwtCommand->set_iface_id(iinfo->name);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /* Add the vendor specific attributes for the NL command. */
    nlData = ptwtCommand->attr_start(NL80211_ATTR_VENDOR_DATA);
    if (!nlData){
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }

    ret = ptwtCommand->put_u8(QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION,
                              QCA_WLAN_TWT_GET_STATS);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    nlTwtParams =
        ptwtCommand->attr_start(QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS);

    ret = ptwtCommand->put_u8(QCA_WLAN_VENDOR_ATTR_TWT_STATS_FLOW_ID,
                              session_id);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ptwtCommand->attr_end(nlTwtParams);
    ptwtCommand->attr_end(nlData);

    ret = ptwtCommand->requestResponse();
    if (ret != WIFI_SUCCESS){
        ALOGE("%s: requestResponse Error:%d", __FUNCTION__, ret);
        goto cleanup;
    }

cleanup:
    return ret;
}