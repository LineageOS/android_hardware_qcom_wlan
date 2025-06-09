/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "twtCommand.h"
#include <math.h>
#include "errno.h"
#include <inttypes.h>

TwtCommand::TwtCommand(wifi_handle handle, int id, u32 vendor_id, u32 subcmd)
        : WifiVendorCommand(handle, id, vendor_id, subcmd)
{
    memset(&mHandler, 0, sizeof(mHandler));
    mTWTCapabilities = NULL;
    mRequestId = 0;
    mWakeTwtCapabilities = false;
    mTwtIsSessionUpdateCmd = false;
}

TwtCommand::~TwtCommand()
{
}

TwtCommand* TwtCommand::handlerInstance(wifi_handle handle)
{
    if (handle == NULL) {
        ALOGE("Interface Handle is invalid");
        return NULL;
    }

    hal_info* info = getHalInfo(handle);
    if (!info->twt_cmd_handler) {
        info->twt_cmd_handler = (twt_cmd_handler *)malloc(sizeof(twt_cmd_handler));
        if (info->twt_cmd_handler == NULL) {
            ALOGE("%s: Allocation of twt handler failed",__FUNCTION__);
            return NULL;
        }
        info->twt_cmd_handler->pTwtCommand = NULL;
    }

    TwtCommand* pTwtCommand = info->twt_cmd_handler->pTwtCommand;

    if (pTwtCommand == NULL) {
        pTwtCommand = new TwtCommand(handle, 0,
                OUI_QCA,
                QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
        ALOGV("TwtCommand handler instance is created");
        info->twt_cmd_handler->pTwtCommand = pTwtCommand;

        return pTwtCommand;
    } else if (handle != getWifiHandle(pTwtCommand->mInfo)) {
        /*
         * upper layer must have cleaned up the handle and reinitialized,
         * so we need to update the same.
         */
        ALOGV("Handle is different, update the handle");
        pTwtCommand->mInfo = (hal_info *)handle;
    }

    ALOGV("TwtCommand handler instance has already been created");

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

    pTwtCommand = TwtCommand::handlerInstance(wifiHandle);
    if (!pTwtCommand) {
        ALOGE("%s: Error while fetching twtCommand instance", __FUNCTION__);
        free(info->twt_cmd_handler);
        info->twt_cmd_handler = NULL;
        return WIFI_ERROR_UNKNOWN;
    }

    return pTwtCommand->setCallbackHandler(events);
}

void TwtCommand::removeCallbackHandler()
{
    unregisterVendorHandler(mVendor_id, QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
}

void cleanupTwtCommand(hal_info *info)
{
    TwtCommand *pTwtCommand;

    ALOGV("%s:Enter", __FUNCTION__);
    if (info == NULL || info->twt_cmd_handler == NULL)
        return;

    pTwtCommand = info->twt_cmd_handler->pTwtCommand;
    if (pTwtCommand) {
        pTwtCommand->removeCallbackHandler();
        delete pTwtCommand;
    }
    free(info->twt_cmd_handler);
    info->twt_cmd_handler = NULL;

    return;
}

wifi_error TwtCommand::requestResponse()
{
    return WifiCommand::requestResponse(mMsg);
}

int TwtCommand::requestResponseWithKernelStatus()
{
    return WifiCommand::requestResponseWithKernelStatus(mMsg);
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

bool TwtCommand::getWakeTwtCapabilities()
{
    return mWakeTwtCapabilities;
}

void TwtCommand::setWakeTwtCapabilities(bool WakeTwtCapabilities)
{
    mWakeTwtCapabilities = WakeTwtCapabilities;
}

void TwtCommand::setTwtIsSessionUpdateCmd(bool TwtIsSessionUpdateCmd)
{
    mTwtIsSessionUpdateCmd = TwtIsSessionUpdateCmd;
}

int TwtCommand::handleResponse(WifiEvent &reply)
{
    WifiVendorCommand::handleResponse(reply);
    u16 self_capabilities = 0;

    if (mSubcmd != QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT) {
        ALOGE("Invalid Subcmd:%d received", mSubcmd);
        return NL_SKIP;
    }

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

            if (tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_SELF]) {
                self_capabilities =
                    get_u16(tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_SELF]);
            } else {
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

            if (tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MIN_WAKE_DURATION]) {
                mTWTCapabilities->min_wake_duration_micros =
                    get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MIN_WAKE_DURATION]);
                mWakeTwtCapabilities = true;
            } else {
                ALOGE("min wake duration attribute is not present");
            }

            if (tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MAX_WAKE_DURATION]) {
                mTWTCapabilities->max_wake_duration_micros =
                    get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MAX_WAKE_DURATION]);
                mWakeTwtCapabilities = true;
            } else {
                ALOGE("max wake duration attribute is not present");
            }

            if (tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MIN_WAKE_INTVL]) {
                mTWTCapabilities->min_wake_interval_micros =
                    get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MIN_WAKE_INTVL]);
                mWakeTwtCapabilities = true;
            } else {
                ALOGE("min wake interval attribute is not present here");
            }

            if (tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MAX_WAKE_INTVL]) {
                mTWTCapabilities->max_wake_interval_micros =
                    get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_TWT_CAPABILITIES_MAX_WAKE_INTVL]);
                mWakeTwtCapabilities = true;
            } else {
                ALOGE("max wake interval attribute is not present");
            }

            ALOGV("TWT caps: %s%s%s%s SP:[min:%d max:%d] SI:[min:%" PRIu64 " max:%" PRIu64 "]",
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

    if (capabilities == NULL) {
        ALOGE("%s: capabilities is NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    memset(capabilities, 0, sizeof(wifi_twt_capabilities));

    ptwtCommand = new TwtCommand(handle, 0, OUI_QCA,
                                 QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
    if (ptwtCommand == NULL) {
        ALOGE("%s: Error TwtCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

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

    ptwtCommand->setWakeTwtCapabilities(false);
    ret = ptwtCommand->requestResponse();
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    if (!ptwtCommand->getWakeTwtCapabilities()) {
        ALOGE("%s: driver doesn't support framework TWT APIs if wake duration and interval capabilities are not advertised.", __FUNCTION__);
        ret = WIFI_ERROR_NOT_SUPPORTED;
        goto cleanup;
    }

cleanup:
    if (ret != WIFI_SUCCESS)
        ALOGE("%s: Error:%d", __FUNCTION__, ret);

    delete ptwtCommand;
    return ret;
}

wifi_error wifi_twt_session_get_stats(wifi_request_id id,
                                      wifi_interface_handle iface,
                                      int session_id)
{
    wifi_error ret;
    int kernelError;
    TwtCommand *ptwtCommand;
    TwtCommand *twtCommandHandler;
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

    ptwtCommand = new TwtCommand(handle, 0, OUI_QCA,
                                 QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
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

    if(!nlTwtParams) {
        ALOGE("%s: nlTwtParams is NULL",  __FUNCTION__);
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }

    ret = ptwtCommand->put_u8(QCA_WLAN_VENDOR_ATTR_TWT_STATS_FLOW_ID,
                              session_id);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ptwtCommand->attr_end(nlTwtParams);
    ptwtCommand->attr_end(nlData);

    twtCommandHandler = TwtCommand::handlerInstance(handle);
    if (twtCommandHandler)
        ptwtCommand->mHandler = twtCommandHandler->mHandler;

    kernelError = ptwtCommand->requestResponseWithKernelStatus();
    ret = mapKernelErrortoWifiHalError(kernelError);
    if (ret != WIFI_SUCCESS) {
        ptwtCommand->sendTwtFailure(id, kernelError);
        ALOGE("%s: requestResponse Error:%d", __FUNCTION__, ret);
    }

cleanup:
    delete ptwtCommand;
    return WIFI_SUCCESS;
}

void TwtCommand::sendTwtFailure(wifi_request_id id, int ret)
{
    wifi_twt_error_code twtErrorCode;

    switch(ret) {
        case -EOPNOTSUPP:
            twtErrorCode = WIFI_TWT_ERROR_CODE_NOT_SUPPORTED;
            break;
        case -EPROTONOSUPPORT:
            twtErrorCode =  WIFI_TWT_ERROR_CODE_PEER_NOT_SUPPORTED;
            break;
        case -EINVAL:
            twtErrorCode = WIFI_TWT_ERROR_CODE_INVALID_PARAMS;
            break;
        default:
            twtErrorCode = WIFI_TWT_ERROR_CODE_FAILURE_UNKNOWN;
            break;
    }

    if (mHandler.on_twt_failure)
        mHandler.on_twt_failure(id, twtErrorCode);
}

static
wifi_error wifi_twt_send_set_twt(wifi_request_id id,
                                 wifi_interface_handle iface,
                                 int session_id, wifi_twt_request request,
                                 bool is_session_update)
{

    wifi_error ret;
    int kernelError;
    TwtCommand *ptwtCommand;
    TwtCommand *twtCommandHandler;
    struct nlattr *nlData, *nlTwtParams;
    interface_info *iinfo;
    wifi_handle handle;
    u32 wake_duration;
    u32 wake_interval, wake_interval_mantissa;
    u8 wake_interval_exp, exponent;

    if(!iface) {
        ALOGE("%s: iface is NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    iinfo = getIfaceInfo(iface);
    if (!iinfo) {
        ALOGE("%s: iinfo is NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    ALOGV("%s: Enter id:%d", __FUNCTION__, id);
    handle = getWifiHandle(iface);

    ptwtCommand = new TwtCommand(handle, 0, OUI_QCA,
                                 QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
    if (ptwtCommand == NULL) {
        ALOGE("%s: Error TwtCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    twtCommandHandler = TwtCommand::handlerInstance(handle);
    if (twtCommandHandler) {
        ptwtCommand->mHandler = twtCommandHandler->mHandler;
        twtCommandHandler->setSubCmd(QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
        twtCommandHandler->setReqId(id);
        if (is_session_update) {
            twtCommandHandler->setTwtFlowId(session_id);
            twtCommandHandler->setTwtIsSessionUpdateCmd(true);
        } else {
            twtCommandHandler->setTwtIsSessionUpdateCmd(false);
        }
    }

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
    if (!nlData) {
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }

    ret = ptwtCommand->put_u8(QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION,
                              QCA_WLAN_TWT_SET);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    nlTwtParams = ptwtCommand->attr_start(
                                QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS);
    if(!nlTwtParams) {
        ALOGE("%s: nlTwtParams is NULL",  __FUNCTION__);
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }

    if (is_session_update) {
        ret = ptwtCommand->put_u8(QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID,
                                  session_id);
        if (ret != WIFI_SUCCESS)
            goto cleanup;
    }

    ret = ptwtCommand->put_u32(QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MIN_WAKE_INTVL,
                               request.min_wake_interval_micros);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = ptwtCommand->put_u32(QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX_WAKE_INTVL,
                               request.max_wake_interval_micros);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /*
     * Android Framework configuration doesn't provide the fixed value for wake
     * interval (SI).
     * However, vendor command requires this mandatory parameter for TWT session
     * update/create.
     * Hence, calculate the average based on min/max wake interval provided during
     * TWT session update/create.
     */
    wake_interval = (u32) ((request.min_wake_interval_micros +
                            request.max_wake_interval_micros) / 2);

    /*
     * Fill QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_EXP = 0,
     * and QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_MANTISSA = 0,
     * so that driver will read the below attribute:
     * QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL2_MANTISSA
     */
    if(wake_interval <= TWT_SETUP_WAKE_INTVL_MANTISSA_MAX) {
        wake_interval_mantissa = wake_interval;
        wake_interval_exp = 0;
    } else {
        exponent = log2(wake_interval * 1.0 /
                        TWT_SETUP_WAKE_INTVL_MANTISSA_MAX);
        wake_interval_exp = ceil(exponent);
        wake_interval_mantissa = wake_interval / pow(2, wake_interval_exp);
    }

    ret = ptwtCommand->put_u32(QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_MANTISSA, 0);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = ptwtCommand->put_u32(QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL2_MANTISSA,
                               wake_interval_mantissa);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = ptwtCommand->put_u8(QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_EXP,
                              wake_interval_exp);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = ptwtCommand->put_u32(QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MIN_WAKE_DURATION,
                               request.min_wake_duration_micros);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = ptwtCommand->put_u32(QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX_WAKE_DURATION,
                               request.max_wake_duration_micros);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /*
    * Android Framework configuration doesn't provide the fixed value for wake
    * duration (SP).
    * However, vendor command requires this mandatory parameter for TWT update/create.
    * Hence, calculate the Average based on min/max wake duration provided during
    * TWT session update/create.
    */
    wake_duration = (request.max_wake_duration_micros +
                     request.min_wake_duration_micros) / 2;
    wake_duration /= TWT_WAKE_DURATION_FACTOR;

    ret = ptwtCommand->put_u32(QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_DURATION,
                               wake_duration);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = ptwtCommand->put_u8(QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_TYPE,
                              TWT_FLOW_TYPE_ANNOUNCED);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = ptwtCommand->put_u8(QCA_WLAN_VENDOR_ATTR_TWT_SETUP_REQ_TYPE,
                              QCA_WLAN_VENDOR_TWT_SETUP_SUGGEST);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ptwtCommand->attr_end(nlTwtParams);
    ptwtCommand->attr_end(nlData);

    kernelError = ptwtCommand->requestResponseWithKernelStatus();
    ret = mapKernelErrortoWifiHalError(kernelError);
    if (ret != WIFI_SUCCESS) {
        ptwtCommand->sendTwtFailure(id, kernelError);
        ALOGE("%s: requestResponse Error:%d", __FUNCTION__, ret);
    }

cleanup:
    delete ptwtCommand;
    return WIFI_SUCCESS;
}

wifi_error wifi_twt_session_update(wifi_request_id id,
                                   wifi_interface_handle iface,
                                   int session_id, wifi_twt_request request)
{
    return wifi_twt_send_set_twt(id, iface, session_id, request, true);
}

wifi_error wifi_twt_session_setup(wifi_request_id id,
                                  wifi_interface_handle iface,
                                  wifi_twt_request request)
{
    return wifi_twt_send_set_twt(id, iface, INVALID_TWT_SESSION_ID, request, false);
}

wifi_error wifi_twt_session_teardown(wifi_request_id id, wifi_interface_handle iface,
                                     int session_id)
{
    wifi_error ret;
    int kernelError;
    TwtCommand *twtCommandHandler;
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

    ALOGV("%s Enter", __FUNCTION__);
    handle = getWifiHandle(iface);

    ptwtCommand = new TwtCommand(handle, 0, OUI_QCA,
                                 QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
    if (ptwtCommand == NULL) {
        ALOGE("%s: Error TwtCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    twtCommandHandler = TwtCommand::handlerInstance(handle);
    if (twtCommandHandler) {
        ptwtCommand->mHandler = twtCommandHandler->mHandler;
        twtCommandHandler->setSubCmd(QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
        twtCommandHandler->setReqId(id);
    }

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
    if (!nlData) {
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }

    ret = ptwtCommand->put_u8(QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION,
                              QCA_WLAN_TWT_TERMINATE);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    nlTwtParams = ptwtCommand->attr_start(
    QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS);

    if(!nlTwtParams) {
        ALOGE("%s: nlTwtParams is NULL",  __FUNCTION__);
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }

    ret = ptwtCommand->put_u8(QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID,
                              session_id);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ptwtCommand->attr_end(nlTwtParams);
    ptwtCommand->attr_end(nlData);

    kernelError = ptwtCommand->requestResponseWithKernelStatus();
    ret = mapKernelErrortoWifiHalError(kernelError);
    if (ret != WIFI_SUCCESS) {
        ptwtCommand->sendTwtFailure(id, kernelError);
        ALOGE("%s: requestResponse Error:%d", __FUNCTION__, ret);
    }

    ALOGV("%s: Teardown TWT session:%d", __FUNCTION__, session_id);

cleanup:
    delete ptwtCommand;
    return WIFI_SUCCESS;
}

wifi_error wifi_twt_session_suspend(wifi_request_id id, wifi_interface_handle iface,
                                    int session_id)
{
    wifi_error ret;
    int kernelError;
    TwtCommand *twtCommandHandler;
    TwtCommand *ptwtCommand;
    struct nlattr *nlData, *nlTwtParams;
    interface_info *iinfo;
    wifi_handle handle;

    if(!iface) {
        ALOGE("%s: iface is NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    iinfo = getIfaceInfo(iface);
    if (!iinfo) {
        ALOGE("%s: iinfo is NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    ALOGV("%s Enter", __FUNCTION__);
    handle = getWifiHandle(iface);

    ptwtCommand = new TwtCommand(handle, 0, OUI_QCA,
                                 QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
    if (ptwtCommand == NULL) {
        ALOGE("%s: Error TwtCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    twtCommandHandler = TwtCommand::handlerInstance(handle);
    if (twtCommandHandler) {
        ptwtCommand->mHandler = twtCommandHandler->mHandler;
        twtCommandHandler->setSubCmd(QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT);
        twtCommandHandler->setReqId(id);
    }

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
    if (!nlData) {
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }

    ret = ptwtCommand->put_u8(QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION,
                              QCA_WLAN_TWT_SUSPEND);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    nlTwtParams = ptwtCommand->attr_start(QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS);
    if(!nlTwtParams) {
        ALOGE("%s: nlTwtParams is NULL",  __FUNCTION__);
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }

    ret = ptwtCommand->put_u8(QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID,
                              session_id);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ptwtCommand->attr_end(nlTwtParams);
    ptwtCommand->attr_end(nlData);

    kernelError = ptwtCommand->requestResponseWithKernelStatus();
    ret = mapKernelErrortoWifiHalError(kernelError);
    if (ret != WIFI_SUCCESS) {
        ptwtCommand->sendTwtFailure(id, kernelError);
        ALOGE("%s: requestResponse Error:%d", __FUNCTION__, ret);
    } else {
        ALOGV("%s: Suspend TWT session:%d", __FUNCTION__, session_id);
    }

cleanup:
    delete ptwtCommand;
    return WIFI_SUCCESS;
}

wifi_twt_error_code
TwtCommand::mapDriverStatusToHalErrorCode(enum qca_wlan_vendor_twt_status status)
{
    switch (status) {
        case QCA_WLAN_VENDOR_TWT_STATUS_UNKNOWN_ERROR:
            return WIFI_TWT_ERROR_CODE_FAILURE_UNKNOWN;
        case QCA_WLAN_VENDOR_TWT_STATUS_TWT_ALREADY_RESUMED:
        case QCA_WLAN_VENDOR_TWT_STATUS_ALREADY_SUSPENDED:
            return WIFI_TWT_ERROR_CODE_ALREADY_RESUMED;
        case QCA_WLAN_VENDOR_TWT_STATUS_INVALID_PARAM:
            return WIFI_TWT_ERROR_CODE_INVALID_PARAMS;
        case QCA_WLAN_VENDOR_TWT_STATUS_NO_RESOURCE:
            return WIFI_TWT_ERROR_CODE_MAX_SESSION_REACHED;
        case QCA_WLAN_VENDOR_TWT_STATUS_NOT_READY:
            return WIFI_TWT_ERROR_CODE_NOT_AVAILABLE;
        case QCA_WLAN_VENDOR_TWT_STATUS_TWT_NOT_ENABLED:
            return WIFI_TWT_ERROR_CODE_NOT_SUPPORTED;
        case QCA_WLAN_VENDOR_TWT_STATUS_PEER_REJECTED:
        case QCA_WLAN_VENDOR_TWT_STATUS_DENIED:
            return WIFI_TWT_ERROR_CODE_PEER_REJECTED;
        case QCA_WLAN_VENDOR_TWT_STATUS_TIMEOUT:
        case QCA_WLAN_VENDOR_TWT_STATUS_NO_RESPONSE:
            return WIFI_TWT_ERROR_CODE_TIMEOUT;
        default:
            return WIFI_TWT_ERROR_CODE_FAILURE_UNKNOWN;
    }
}

wifi_twt_teardown_reason_code
TwtCommand::mapTeardownHalReasonCode(enum qca_wlan_vendor_twt_status status)
{
    switch (status) {
        case QCA_WLAN_VENDOR_TWT_STATUS_OK:
            return WIFI_TWT_TEARDOWN_REASON_CODE_LOCALLY_REQUESTED;
        case QCA_WLAN_VENDOR_TWT_STATUS_ROAM_INITIATED_TERMINATE:
        case QCA_WLAN_VENDOR_TWT_STATUS_SCC_MCC_CONCURRENCY_TERMINATE:
        case QCA_WLAN_VENDOR_TWT_STATUS_POWER_SAVE_EXIT_TERMINATE:
            return WIFI_TWT_TEARDOWN_REASON_CODE_INTERNALLY_INITIATED;
        case QCA_WLAN_VENDOR_TWT_STATUS_PEER_INITIATED_TERMINATE:
            return WIFI_TWT_TEARDOWN_REASON_CODE_PEER_INITIATED;
        default:
            return WIFI_TWT_TEARDOWN_REASON_CODE_UNKNOWN;
    }
}

/*
 * This function will be the main handler for incoming event SUBCMD_CONFIG_TWT
 * Call the appropriate callback handler after parsing the vendor data.
 */
int TwtCommand::handleEvent(WifiEvent &event)
{
    struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_MAX + 1];
    struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX + 1];
    int attr_id;
    u8 twt_operation_type;
    enum qca_wlan_vendor_twt_status resp_status;

    WifiVendorCommand::handleEvent(event);
    if (mSubcmd != QCA_NL80211_VENDOR_SUBCMD_CONFIG_TWT)
        return NL_SKIP;

    if (nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_MAX,
                  (struct nlattr *) mVendorData, mDataLen, NULL)) {
        ALOGE("NL80211_ATTR_VENDOR_DATA parse error.");
        return NL_SKIP;
    }

    attr_id = QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION;
    if (!tb_vendor[attr_id]) {
        ALOGE("TWT config operation event is wrong");
        return NL_SKIP;
    }
    twt_operation_type = get_u8(tb_vendor[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_OPERATION]);
    ALOGV("Received a TWT event with twt_operation_type %u", twt_operation_type);

    switch(twt_operation_type) {
        case QCA_WLAN_TWT_SET:
        {
            wifi_twt_session twt_session;
            wifi_twt_error_code error_code;
            u32 exp = 1, mantissa;

            if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS]) {
                ALOGE("TWT session setup nested attributes is null");
                return NL_SKIP;
            }

            if (nla_parse_nested(tb2, QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX,
                                 tb_vendor[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS],
                                 NULL)) {
                ALOGE("nla_parse failed for vendor_data");
                return NL_SKIP;
            }

            if (!tb2[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS]) {
                ALOGE("TWT_SETUP_STATUS attribute is missing");
                if (mHandler.on_twt_failure)
                    (*mHandler.on_twt_failure)(mRequestId,
                                               WIFI_TWT_ERROR_CODE_FAILURE_UNKNOWN);
                else
                    ALOGE("TWT: Failure Callback is not registered");

                return NL_SKIP;
            }

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS;
            resp_status =
                (enum qca_wlan_vendor_twt_status)get_u8(tb2[attr_id]);
            error_code = mapDriverStatusToHalErrorCode(resp_status);
            if (resp_status != QCA_WLAN_VENDOR_TWT_STATUS_OK &&
                mHandler.on_twt_failure) {
                    (*mHandler.on_twt_failure)(mRequestId, error_code);
                    ALOGE("TWT Setup received with failure status:%d error_code:%d",
                          resp_status, error_code);
                    return NL_SKIP;
            }

            memset(&twt_session, 0, sizeof(twt_session));
            //Initializing the invalid MLO_LINK_ID
            twt_session.mlo_link_id = -1;

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID;
            if (tb2[attr_id])
                twt_session.session_id = get_u8(tb2[attr_id]);

            /*
             * Validate if the TWT session update response session_id is same
             * as the user provided session_id
             */
            if (mTwtIsSessionUpdateCmd &&
                (!tb2[attr_id] ||
                 (tb2[attr_id] && twt_session.session_id != mTwtFlowId))) {
                ALOGE("TWT flow id received:%d is invalid expected flow id:%d",
                      twt_session.session_id, mTwtFlowId);
                return NL_SKIP;
            }

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_DURATION;
            if (tb2[attr_id])
                twt_session.wake_duration_micros =
                    get_u32(tb2[attr_id]) * TWT_WAKE_DURATION_FACTOR;
            else
                ALOGE("TWT_SETUP_WAKE_DURATION is missing");

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_EXP;
            if (!tb2[attr_id]) {
                ALOGE("TWT_SETUP_WAKE_INTVL_EXP attribute is missing");
                if (mHandler.on_twt_failure)
                    (*mHandler.on_twt_failure)(mRequestId, WIFI_TWT_ERROR_CODE_INVALID_PARAMS);
                else
                    ALOGE("TWT: Failure Callback is not registered");
                return NL_SKIP;
            }
            exp = pow(2, get_u8(tb2[attr_id]));

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL2_MANTISSA;
            if (!tb2[attr_id]) {
                attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_WAKE_INTVL_MANTISSA;
                if (tb2[attr_id]) {
                    mantissa = get_u32(tb2[attr_id]) * TWT_WAKE_INTERVAL_TU_FACTOR;
                    twt_session.wake_interval_micros = exp * mantissa;
                } else {
                    ALOGE("TWT_SETUP_WAKE_INTVL_MANTISSA is missing");
                }
            } else {
                twt_session.wake_interval_micros = exp * get_u32(tb2[attr_id]);
            }

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_BCAST_ID;
            if (tb2[attr_id])
                twt_session.negotiation_type =
                    nla_get_u8(tb2[attr_id]) ? WIFI_TWT_NEGO_TYPE_BROADCAST : WIFI_TWT_NEGO_TYPE_INDIVIDUAL;

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TRIGGER;
            if(tb2[attr_id])
                twt_session.is_trigger_enabled = nla_get_flag(tb2[attr_id]);
            else
                ALOGV("TWT attribute:%d is not present", attr_id);

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_TYPE;
            if(tb2[attr_id])
                twt_session.is_announced = nla_get_u8(tb2[attr_id]);
            else
                ALOGV("TWT attribute:%d is not present", attr_id);

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_IMPLICIT;
            if (tb2[attr_id])
                twt_session.is_implicit = get_u8(tb2[attr_id]) ? 1 : 0;
            else
                ALOGV("TWT attribute:%d is not present", attr_id);

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_PROTECTION;
            if (tb2[attr_id])
                twt_session.is_protected = nla_get_flag(tb2[attr_id]);
            else
                ALOGV("TWT attribute:%d is not present", attr_id);

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_UPDATABLE;
            if (tb2[attr_id])
                twt_session.is_updatable = get_u8(tb2[attr_id]) ? 1 : 0;
            else
                ALOGV("TWT attribute:%d is not present", attr_id);

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_TWT_INFO_ENABLED;
            if (tb2[attr_id])
                twt_session.is_suspendable = nla_get_flag(tb2[attr_id]);
            else
                ALOGV("TWT attribute:%d is not present", attr_id);

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_RESPONDER_PM_MODE;
            if (tb2[attr_id])
                twt_session.is_responder_pm_mode_enabled = get_u8(tb2[attr_id]) ? 1 : 0;
            else
                ALOGV("TWT attribute:%d is not present", attr_id);
            if (mTwtIsSessionUpdateCmd) {
                if (mHandler.on_twt_session_update)
                    (*mHandler.on_twt_session_update)(mRequestId, twt_session);
                else
                    ALOGE("TWT: Session update callback is not registered:");
            } else {
                if (mHandler.on_twt_session_create)
                    (*mHandler.on_twt_session_create)(mRequestId, twt_session);
                else
                    ALOGE("TWT: Session Setup Callback is not registered:");
            }

            ALOGV("TWT Response: session_id:%d, SP:%" PRIu64 ", SI:%" PRIu32 " %s%s%s%s%s%s%s%s",
                  twt_session.session_id,
                  twt_session.wake_interval_micros,
                  twt_session.wake_duration_micros,
                  twt_session.negotiation_type ? "[Broadcast]" : "[Individual]",
                  twt_session.is_trigger_enabled ? "[Trigger]" : "",
                  twt_session.is_implicit ? "[Implicit]" : "",
                  twt_session.is_suspendable ? "[Suspendable]" : "",
                  twt_session.is_responder_pm_mode_enabled ? "[Responder PM]" : "",
                  twt_session.is_announced ? "[Announced]" : "",
                  twt_session.is_protected ? "[Protected]" : "",
                  twt_session.is_updatable ? "[Updatable]" : "");

            /*
             * Reset the command handler parameters to avoid duplicate command
             * processing and back to back update, setup command sequence.
             */
            mTwtIsSessionUpdateCmd = false;
            mTwtFlowId = -1;
        }
        break;
        case QCA_WLAN_TWT_TERMINATE:
        {
           wifi_twt_teardown_reason_code reason;
           int flow_id = INVALID_TWT_SESSION_ID;
           wifi_twt_error_code error_code;

           if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS]) {
                ALOGE("TWT teardown nested attributes is null");
                return NL_SKIP;
           }

           if (nla_parse_nested(tb2, QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX,
                                tb_vendor[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS],
                                NULL)) {
               ALOGE("nla_parse failed for vendor_data");
               return NL_SKIP;
           }

           attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS;
           if (tb2[attr_id])
               resp_status = (enum qca_wlan_vendor_twt_status)get_u8(tb2[attr_id]);
           else
               resp_status = QCA_WLAN_VENDOR_TWT_STATUS_DENIED;

           if (resp_status != QCA_WLAN_VENDOR_TWT_STATUS_OK &&
               resp_status != QCA_WLAN_VENDOR_TWT_STATUS_PEER_INITIATED_TERMINATE &&
               resp_status != QCA_WLAN_VENDOR_TWT_STATUS_ROAM_INITIATED_TERMINATE &&
               resp_status != QCA_WLAN_VENDOR_TWT_STATUS_SCC_MCC_CONCURRENCY_TERMINATE &&
               resp_status != QCA_WLAN_VENDOR_TWT_STATUS_POWER_SAVE_EXIT_TERMINATE) {
                ALOGE("TWT: Teardown failed resp_status:%d", resp_status);
                error_code = mapDriverStatusToHalErrorCode(resp_status);
                if (mHandler.on_twt_failure)
                    (*mHandler.on_twt_failure)(mRequestId, error_code);
                else
                   ALOGE("TWT: Failure Callback is not registered:");

                return NL_SKIP;
            }

            reason = mapTeardownHalReasonCode(resp_status);

            if (tb2[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID])
                flow_id = get_u8(tb2[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID]);

            ALOGV("TWT: Teardown response flow_id:%d status:%d reason:%d",
                  flow_id, resp_status, reason);

            if (mHandler.on_twt_session_teardown)
                (*mHandler.on_twt_session_teardown)(mRequestId, flow_id, reason);
            else
                ALOGE("TWT: No Callback registered:");
        }
        break;
        case QCA_WLAN_TWT_SUSPEND:
        {
            int flow_id = 0;
            wifi_twt_error_code error_code;

            if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS]) {
                ALOGE("TWT suspend nested attributes is null");
                return NL_SKIP;
            }
            if (nla_parse_nested(tb2, QCA_WLAN_VENDOR_ATTR_TWT_SETUP_MAX,
                                 tb_vendor[QCA_WLAN_VENDOR_ATTR_CONFIG_TWT_PARAMS],
                                 NULL)) {
               ALOGE("nla_parse failed for vendor_data");
               return NL_SKIP;
            }

            attr_id = QCA_WLAN_VENDOR_ATTR_TWT_SETUP_STATUS;
            if (tb2[attr_id])
                resp_status = (enum qca_wlan_vendor_twt_status)get_u8(tb2[attr_id]);
            else
                resp_status = QCA_WLAN_VENDOR_TWT_STATUS_NOT_SUSPENDED;

            if (resp_status != QCA_WLAN_VENDOR_TWT_STATUS_OK &&
                mHandler.on_twt_failure) {
                error_code = mapDriverStatusToHalErrorCode(resp_status);
                if (mHandler.on_twt_failure)
                    (*mHandler.on_twt_failure)(mRequestId, error_code);
                else
                    ALOGE("TWT: Failure Callback is not registered:");
                return NL_SKIP;
            }

            if (tb2[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID])
                flow_id = get_u8(tb2[QCA_WLAN_VENDOR_ATTR_TWT_SETUP_FLOW_ID]);

            if (mHandler.on_twt_session_suspend)
                (*mHandler.on_twt_session_suspend)(mRequestId, flow_id);
            else
               ALOGE("TWT: No Callback registered:");
        }
        break;
        default:
            ALOGV("Unsupported TWT event received");
        break;
    }

    return NL_SKIP;
}
