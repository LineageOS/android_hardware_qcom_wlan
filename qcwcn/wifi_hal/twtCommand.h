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


class TwtCommand: public WifiVendorCommand
{
private:
    wifi_twt_events mHandler;
    wifi_twt_capabilities *mTWTCapabilities;
    u32 mTWTRequestType;
    wifi_request_id mRequestId;
    int mTwtFlowId;

    TwtCommand(wifi_handle handle, int id, u32 vendor_id, u32 subcmd);

public:
    wifi_twt_session_stats mTWTSessionStats;

    static TwtCommand* instance(wifi_handle handle);
    virtual ~TwtCommand();
    virtual wifi_error setCallbackHandler(wifi_twt_events handler);

    virtual void setSubCmd(u32 subcmd);
    virtual void setTWTRequestType(enum qca_wlan_twt_operation event);
    virtual void setTwtCapabilities(wifi_twt_capabilities* capabilities);
    virtual void setReqId(wifi_request_id reqid);
    virtual void setTwtFlowId(int flowId);

    virtual wifi_error requestResponse();
    virtual int handleResponse(WifiEvent &reply);

};

typedef struct twt_cmd_handler_s {
    TwtCommand *pTwtCommand;
} twt_cmd_handler;

void cleanupTwtCommand(hal_info *info);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __WIFI_HAL_TWT_COMMAND_H__ */
