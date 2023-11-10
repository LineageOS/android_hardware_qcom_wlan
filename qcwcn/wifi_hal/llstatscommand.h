/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __WIFI_HAL_LLSTATSCOMMAND_H__
#define __WIFI_HAL_LLSTATSCOMMAND_H__

#include <stdint.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <linux/rtnetlink.h>
#include <netpacket/packet.h>
#include <linux/filter.h>
#include <linux/errqueue.h>

#include <linux/pkt_sched.h>
#include <netlink/object-api.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <net/if.h>

#include "nl80211_copy.h"
#include "common.h"
#include "cpp_bindings.h"
#include <hardware_legacy/link_layer_stats.h>

#ifdef __GNUC__
#define PRINTF_FORMAT(a,b) __attribute__ ((format (printf, (a), (b))))
#define STRUCT_PACKED __attribute__ ((packed))
#else
#define PRINTF_FORMAT(a,b)
#define STRUCT_PACKED
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef struct{
    u32 stats_clear_rsp_mask;
    u8 stop_rsp;
} LLStatsClearRspParams;

typedef struct{
    wifi_iface_stat *iface_stat;
    int num_radios;
    wifi_radio_stat *radio_stat;
    wifi_iface_ml_stat *iface_ml_stat;
} LLStatsResultsParams;

typedef struct{
    int num_peers;
    int num_rates;
    u8 *link_ids;
    wifi_peer_info *peers_info;
} LinkPeerStatsResultsParams;

typedef enum{
    eLLStatsSetParamsInvalid = 0,
    eLLStatsClearRspParams,
} eLLStatsRspRarams;

class LLStatsCommand: public WifiVendorCommand
{
private:
    static LLStatsCommand *mLLStatsCommandInstance;

    LLStatsClearRspParams mClearRspParams;

    LLStatsResultsParams mResultsParams;

    LinkPeerStatsResultsParams mPeerResultsParams;

    wifi_stats_result_handler mHandler;

    wifi_request_id mRequestId;

    u32 mRadioStatsSize;

    // mNumRadios is decoded from tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_NUM_RADIOS]
    // nNumRadiosAllocated is the actual radio stats received.
    u8 mNumRadios;
    u8 mNumRadiosAllocated;

    LLStatsCommand(wifi_handle handle, int id, u32 vendor_id, u32 subcmd);

public:
    static LLStatsCommand* instance(wifi_handle handle);

    virtual ~LLStatsCommand();

    // This function implements creation of LLStats specific Request
    // based on  the request type
    virtual wifi_error create();

    virtual void setSubCmd(u32 subcmd);

    virtual void initGetContext(u32 reqid);

    virtual wifi_error requestResponse();

    virtual wifi_error notifyResponse();

    virtual int handleResponse(WifiEvent &reply);

    virtual void getClearRspParams(u32 *stats_clear_rsp_mask, u8 *stop_rsp);

    virtual wifi_error get_wifi_iface_stats(wifi_iface_stat *stats,
                                            struct nlattr **tb_vendor);

    virtual bool isMlo();

    virtual wifi_error copyMloStats();

    virtual wifi_link_stat * copyMloPeerStats(wifi_link_stat *linkInfo, u8 *resultsBufEnd);

    virtual int get_wifi_ml_iface_numlinks(struct nlattr **tb_vendor);

    virtual wifi_error get_wifi_ml_iface_stats(wifi_iface_ml_stat *stats,
                                               struct nlattr **tb_vendor);

    virtual wifi_error get_wifi_ml_iface_link_states(wifi_iface_ml_stat *stats,
                                                    struct nlattr **tb_link_vendor);

    virtual void setHandler(wifi_stats_result_handler handler);

    virtual void clearStats();
};

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
