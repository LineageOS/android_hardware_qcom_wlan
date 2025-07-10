/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __WIFI_HAL_TWT_COMMAND_H__
#define __WIFI_HAL_TWT_COMMAND_H__

#include "nl80211_copy.h"
#include "common.h"
#include "cpp_bindings.h"
#include <hardware_legacy/wifi_twt.h>
#include <hardware_legacy/wifi_hal.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define TWT_WAKE_INTERVAL_TU_FACTOR 1024
#define TWT_WAKE_DURATION_FACTOR 256

/* TWT Flow Types */
#define TWT_FLOW_TYPE_ANNOUNCED 0
/* Maximum Mantissa value*/
#define TWT_SETUP_WAKE_INTVL_MANTISSA_MAX 0xFFFF

class TwtCommand: public WifiVendorCommand
{
private:
    wifi_twt_capabilities *mTWTCapabilities;
    u32 mTWTRequestType;
    wifi_request_id mRequestId;
    int mTwtFlowId;
    bool mWakeTwtCapabilities;


public:
    wifi_twt_session_stats mTWTSessionStats;
    wifi_twt_events mHandler;

    static TwtCommand* handlerInstance(wifi_handle handle);
    TwtCommand(wifi_handle handle, int id, u32 vendor_id, u32 subcmd);
    virtual ~TwtCommand();
    virtual wifi_error setCallbackHandler(wifi_twt_events handler);
    virtual void removeCallbackHandler();

    virtual void setSubCmd(u32 subcmd);
    virtual void setTWTRequestType(enum qca_wlan_twt_operation event);
    virtual void setTwtCapabilities(wifi_twt_capabilities* capabilities);
    virtual void setReqId(wifi_request_id reqid);
    virtual void setTwtFlowId(int flowId);
    virtual bool getWakeTwtCapabilities();
    virtual void setWakeTwtCapabilities(bool WakeTwtCapabilities);


    virtual wifi_twt_error_code
    mapDriverStatusToHalErrorCode(enum qca_wlan_vendor_twt_status status);
    virtual wifi_twt_teardown_reason_code
    mapTeardownHalReasonCode(enum qca_wlan_vendor_twt_status status);
    virtual wifi_error requestResponse();
    virtual int requestResponseWithKernelStatus();
    virtual int handleResponse(WifiEvent &reply);
    virtual int handleEvent(WifiEvent &event);
    virtual void sendTwtFailure(wifi_request_id id, int ret);
};

typedef struct twt_cmd_handler_s {
    TwtCommand *pTwtCommand;
} twt_cmd_handler;

void cleanupTwtCommand(hal_info *info);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __WIFI_HAL_TWT_COMMAND_H__ */
