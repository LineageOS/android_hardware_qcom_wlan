/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc.All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "nan_i.h"
#include "nancommand.h"
#include "vendor_nan_hal.h"

// Vendor Nan Implementation
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */
/*initialize function pointer table with QTI HAL API for Vendor AIDL */
vendor_nan_error init_nan_vendor_aidl_hal_func_table(vendor_nan_fn *fn) {
    if (fn == NULL) {
        return VENDOR_NAN_ERROR_UNKNOWN;
    }
    fn->wifi_nan_register_vendor_handler = nan_register_vendor_handler;
    fn->wifi_nan_set_command = nan_set_command;
    return VENDOR_NAN_SUCCESS;
}
#ifdef __cplusplus
}
#endif /* __cplusplus */

vendor_nan_error NanCommand::setVendorCallbackHandler(VendorNanCallbackHandler nHandler)
{
    mVendorHandler = nHandler;
    return VENDOR_NAN_SUCCESS;
}

//Implementation of the functions exposed in nan.h
vendor_nan_error nan_register_vendor_handler(wifi_handle handle,
                                             VendorNanCallbackHandler handlers)
{
    // Obtain the singleton instance
    vendor_nan_error ret;
    NanCommand *nanCommand = NULL;

    nanCommand = NanCommand::instance(handle);
    if (nanCommand == NULL) {
        ALOGE("%s: Error NanCommand NULL", __FUNCTION__);
        return VENDOR_NAN_ERROR_UNKNOWN;
    }

    ret = nanCommand->setVendorCallbackHandler(handlers);
    return ret;
}

// Vendor Nan Request

static bool is_nan_pairing_cmd(NanVendorCmdData* msg)
{
  /* TBD: check for OUI type in msg->cmd_data */
  return false;
}

vendor_nan_error nan_handle_pairing_command(transaction_id id,
                                            wifi_handle handle,
                                            NanVendorCmdData* msg)
{
  /* TBD */
  return VENDOR_NAN_SUCCESS;
}

vendor_nan_error NanCommand::putNanCommandData(transaction_id id, NanVendorCmdData *pReq)
{
    vendor_nan_error ret;
    struct nlattr *nl_data;
    ALOGV("NAN_SET_COMMAND");

    if (pReq == NULL || pReq->cmd_len == 0) {
        cleanup();
        return VENDOR_NAN_ERROR_INVALID_ARGS;
    }

    size_t message_len =
        sizeof(NanMsgHeader) + SIZEOF_TLV_HDR + pReq->cmd_len;

    pNanFWOemReqMsg pFwReq = (pNanFWOemReqMsg)malloc(message_len);
    if (pFwReq == NULL) {
        cleanup();
        return VENDOR_NAN_ERROR_OUT_OF_MEMORY;
    }

    ALOGV("Message Len %zu", message_len);
    memset (pFwReq, 0, message_len);
    pFwReq->fwHeader.msgVersion = (u16)NAN_MSG_VERSION1;
    pFwReq->fwHeader.msgId = NAN_MSG_ID_OEM_REQ;
    pFwReq->fwHeader.msgLen = message_len;
    pFwReq->fwHeader.transactionId = id;

    u8* tlvs = pFwReq->ptlv;

    if (pReq->cmd_len) {
        tlvs = addTlv(NAN_TLV_TYPE_OEM1_DATA, pReq->cmd_len,
                      (const u8*)&pReq->cmd[0], tlvs);
    }

    mVendorData = (char*)pFwReq;
    mDataLen = message_len;

    ret = VENDOR_NAN_SUCCESS;

    nl_data = attr_start(NL80211_ATTR_VENDOR_DATA);

    if (!nl_data) {
        cleanup();
        return VENDOR_NAN_ERROR_INVALID_ARGS;
    }
    if (mMsg.put_bytes(QCA_WLAN_VENDOR_ATTR_NAN_CMD_DATA,
                         mVendorData, mDataLen)) {
        ALOGE("%s: put attr error", __func__);
        cleanup();
        return VENDOR_NAN_ERROR_INVALID_ARGS;
    }
    attr_end(nl_data);

    hexdump(mVendorData, mDataLen);
    return ret;
}

/*  Function to send NAN command data to the wifi driver.*/
vendor_nan_error nan_set_command(transaction_id id, wifi_handle handle,
                                 NanVendorCmdData* msg)
{
    wifi_error error;
    vendor_nan_error ret;
    NanCommand *nanCommand = NULL;
    hal_info *info = getHalInfo(handle);

    if (info == NULL) {
        ALOGE("%s: Error hal_info NULL", __FUNCTION__);
        return VENDOR_NAN_ERROR_UNKNOWN;
    }

    /* Check cmd OUI type of NAN command */
    if (is_nan_pairing_cmd(msg))
        return nan_handle_pairing_command(id, handle, msg);

    nanCommand = new NanCommand(handle,
                                0,
                                OUI_QCA,
                                info->support_nan_ext_cmd?
                                QCA_NL80211_VENDOR_SUBCMD_NAN_EXT :
                                QCA_NL80211_VENDOR_SUBCMD_NAN);
    if (nanCommand == NULL) {
        ALOGE("%s: Error NanCommand NULL", __FUNCTION__);
        return VENDOR_NAN_ERROR_UNKNOWN;
    }

    if (nanCommand->create() != WIFI_SUCCESS) {
        ret = VENDOR_NAN_ERROR_FAILURE;
        goto cleanup;
    }

    // FIXME: Use wifi-aware0 iface
    error = nanCommand->set_iface_id("wlan0");

    /* Set the interface Id of the message. */
    if (error != WIFI_SUCCESS) {
        ret = VENDOR_NAN_ERROR_FAILURE;
        goto cleanup;
    }

    ret = nanCommand->putNanCommandData(id, msg);
    if (ret != VENDOR_NAN_SUCCESS) {
        ALOGE("%s: putNanCommandData Error:%d",__FUNCTION__, ret);
        goto cleanup;
    }

    if (nanCommand->requestEvent() != WIFI_SUCCESS) {
        ret = VENDOR_NAN_ERROR_FAILURE;
        ALOGE("%s: requestEvent Error",__FUNCTION__);
    }

cleanup:
    delete nanCommand;
    return ret;
}

// Vendor Nan Response

int NanCommand::getNanVendorResponse(transaction_id *id, NanVendorResponseMsg *pRsp)
{
    hal_info *info = getHalInfo(wifiHandle());

    if (mNanVendorEvent == NULL || pRsp == NULL) {
        ALOGE("NULL check failed");
        return VENDOR_NAN_ERROR_INVALID_ARGS;
    }

    pNanFWOemRspMsg pFwRsp = (pNanFWOemRspMsg)mNanVendorEvent;

    u8 *pInputTlv = pFwRsp->ptlv;
    NanTlv outputTlv;
    u16 readLen = 0;
    int remainingLen = (mNanDataLen - \
        (offsetof(NanFWOemRspMsg, ptlv)));
    int ret = 0, idx = 0;
    *id = (transaction_id)pFwRsp->fwHeader.transactionId;

    if (remainingLen <= 0) {
        ALOGV("%s: No TLV's present",__func__);
        return VENDOR_NAN_ERROR_INVALID_ARGS;
    }
    ALOGV("%s: TLV remaining Len:%d",__func__, remainingLen);
    while (remainingLen >= 4) {
        memset(&outputTlv, 0, sizeof(outputTlv));
        readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen);
        if (!readLen)
            break;

        ALOGV("%s: Remaining Len:%d readLen:%d type:%d length:%d",
              __func__, remainingLen, readLen, outputTlv.type,
              outputTlv.length);
        switch (outputTlv.type) {
        case NAN_TLV_TYPE_OEM1_DATA:
            if (outputTlv.length <= NAN_OEM1_DATA_MAX_LEN) {
                pRsp->rsp_len = outputTlv.length;
                pRsp->rsp_data = outputTlv.value;
            }
            break;
        default:
            ALOGV("Unknown TLV type skipped");
            break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
    }
    return VENDOR_NAN_SUCCESS;
}

// Vendor Nan Event
int NanCommand::getNanVendorEventInd(NanVendorEventInd *event)
{
    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return VENDOR_NAN_ERROR_INVALID_ARGS;
    }

    pNanFWOemIndMsg pRsp = (pNanFWOemIndMsg)mNanVendorEvent;

    u8 *pInputTlv = pRsp->ptlv;
    NanTlv outputTlv;
    u16 readLen = 0;
    int remainingLen = (mNanDataLen - \
        (offsetof(NanFWOemIndMsg, ptlv)));
    int ret = 0, idx = 0;

    if (remainingLen <= 0) {
        ALOGV("%s: No TLV's present",__func__);
        return VENDOR_NAN_ERROR_INVALID_ARGS;
    }
    ALOGV("%s: TLV remaining Len:%d",__func__, remainingLen);
    while (remainingLen >= 4) {
        memset(&outputTlv, 0, sizeof(outputTlv));
        readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen);
        if (!readLen)
            break;

        ALOGV("%s: Remaining Len:%d readLen:%d type:%d length:%d",
              __func__, remainingLen, readLen, outputTlv.type,
              outputTlv.length);
        switch (outputTlv.type) {
        case NAN_TLV_TYPE_OEM1_DATA:
            if (outputTlv.length <= NAN_OEM1_DATA_MAX_LEN) {
                event->event_len = outputTlv.length;
                event->event_data = outputTlv.value;
            }
            break;
        default:
            ALOGV("Unknown TLV type skipped");
            break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
    }
    return VENDOR_NAN_SUCCESS;
}
