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

 * Changes from Qualcomm Innovation Center are provided under the following license:

 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "sync.h"

#define LOG_TAG  "WifiHAL"

#include <utils/Log.h>

#include <hardware_legacy/wifi_hal.h>
#include "common.h"
#include "cpp_bindings.h"
#include "llstatscommand.h"

//Singleton Static Instance
LLStatsCommand* LLStatsCommand::mLLStatsCommandInstance  = NULL;

// This function implements creation of Vendor command
// For LLStats just call base Vendor command create
wifi_error LLStatsCommand::create() {
    wifi_error ret = mMsg.create(NL80211_CMD_VENDOR, 0, 0);
    if (ret != WIFI_SUCCESS)
        return ret;

    // insert the oui in the msg
    ret = mMsg.put_u32(NL80211_ATTR_VENDOR_ID, mVendor_id);
    if (ret != WIFI_SUCCESS)
        return ret;

    // insert the subcmd in the msg
    ret = mMsg.put_u32(NL80211_ATTR_VENDOR_SUBCMD, mSubcmd);

    return ret;
}

LLStatsCommand::LLStatsCommand(wifi_handle handle, int id, u32 vendor_id, u32 subcmd)
        : WifiVendorCommand(handle, id, vendor_id, subcmd)
{
    memset(&mClearRspParams, 0,sizeof(LLStatsClearRspParams));
    memset(&mResultsParams, 0,sizeof(LLStatsResultsParams));
    memset(&mPeerResultsParams, 0,sizeof(LinkPeerStatsResultsParams));
    memset(&mHandler, 0,sizeof(mHandler));
    mRadioStatsSize = 0;
    mNumRadios = 0;
    mNumRadiosAllocated = 0;
    mRequestId = 0;
}

LLStatsCommand::~LLStatsCommand()
{
    mLLStatsCommandInstance = NULL;
}

LLStatsCommand* LLStatsCommand::instance(wifi_handle handle)
{
    if (handle == NULL) {
        ALOGE("Interface Handle is invalid");
        return NULL;
    }
    if (mLLStatsCommandInstance == NULL) {
        mLLStatsCommandInstance = new LLStatsCommand(handle, 0,
                OUI_QCA,
                QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET);
        return mLLStatsCommandInstance;
    }
    else
    {
        if (handle != getWifiHandle(mLLStatsCommandInstance->mInfo))
        {
            /* upper layer must have cleaned up the handle and reinitialized,
               so we need to update the same */
            ALOGE("Handle different, update the handle");
            mLLStatsCommandInstance->mInfo = (hal_info *)handle;
        }
    }
    return mLLStatsCommandInstance;
}

void LLStatsCommand::initGetContext(u32 reqId)
{
    mRequestId = reqId;
    memset(&mHandler, 0,sizeof(mHandler));
}

void LLStatsCommand::setSubCmd(u32 subcmd)
{
    mSubcmd = subcmd;
}

void LLStatsCommand::setHandler(wifi_stats_result_handler handler)
{
    mHandler = handler;
}

static wifi_error get_wifi_interface_info(wifi_interface_link_layer_info *stats,
                                          struct nlattr **tb_vendor)
{
    u32 len = 0;

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MODE])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MODE not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->mode = (wifi_interface_mode)nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MODE]);


    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MAC_ADDR])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MAC_ADDR not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    len = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MAC_ADDR]);
    len = ((sizeof(stats->mac_addr) <= len) ? sizeof(stats->mac_addr) : len);
    memcpy(&stats->mac_addr[0], nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MAC_ADDR]), len);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_STATE])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_STATE not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->state = (wifi_connection_state)nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_STATE]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_ROAMING])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_ROAMING not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->roaming = (wifi_roam_state)nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_ROAMING]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_CAPABILITIES])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_CAPABILITIES not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->capabilities = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_CAPABILITIES]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_SSID])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_SSID not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    len = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_SSID]);
    len = ((sizeof(stats->ssid) <= len) ? sizeof(stats->ssid) : len);
    memcpy(&stats->ssid[0], nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_SSID]), len);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_BSSID])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_BSSID not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    len = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_BSSID]);
    len = ((sizeof(stats->bssid) <= len) ? sizeof(stats->bssid) : len);
    memcpy(&stats->bssid[0], nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_BSSID]), len);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_AP_COUNTRY_STR])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_AP_COUNTRY_STR not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    len = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_AP_COUNTRY_STR]);
    len = ((sizeof(stats->ap_country_str) <= len) ? sizeof(stats->ap_country_str) : len);
    memcpy(&stats->ap_country_str[0], nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_AP_COUNTRY_STR]),
           len);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_COUNTRY_STR])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_COUNTRY_STR not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    len = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_COUNTRY_STR]);
    len = ((sizeof(stats->country_str) < len) ? sizeof(stats->country_str) : len);
    memcpy(&stats->country_str[0], nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_COUNTRY_STR]),
           len);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_TS_DUTY_CYCLE])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_TS_DUTY_CYCLE not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->time_slicing_duty_cycle_percent = nla_get_u8(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_TS_DUTY_CYCLE]);
#if QC_HAL_DEBUG
    ALOGV("Mode : %d\n"
          "Mac addr : "
          MAC_ADDR_STR
          "\nState : %d\n"
          "Roaming : %d\n"
          "capabilities : %0x\n"
          "SSID :%s\n"
          "BSSID : "
          MAC_ADDR_STR
          "\nAP country str : %c%c%c\n"
          "Country String for this Association : %c%c%c\n"
	  "Time slicing duty cycle : %d",
          stats->mode,
          MAC_ADDR_ARRAY(stats->mac_addr),
          stats->state,
          stats->roaming,
          stats->capabilities,
          stats->ssid,
          MAC_ADDR_ARRAY(stats->bssid),
          stats->ap_country_str[0],
          stats->ap_country_str[1],
          stats->ap_country_str[2],
          stats->country_str[0],
          stats->country_str[1],
          stats->country_str[2],
	  stats->time_slicing_duty_cycle_percent);
#endif
    return WIFI_SUCCESS;
}

static wifi_error get_wifi_wmm_ac_stat(wifi_wmm_ac_stat *stats,
                                       struct nlattr **tb_vendor)
{

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_AC])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_AC not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->ac                     = (wifi_traffic_ac)nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_AC]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MPDU])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MPDU not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->tx_mpdu                = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MPDU]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MPDU])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MPDU not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->rx_mpdu                = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MPDU]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MCAST])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MCAST not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->tx_mcast               = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MCAST]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MCAST])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MCAST not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->rx_mcast               = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MCAST]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_AMPDU])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_AMPDU not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->rx_ampdu               = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_AMPDU]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_AMPDU])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_AMPDU not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->tx_ampdu               = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_AMPDU]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_MPDU_LOST])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_MPDU_LOST not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->mpdu_lost              = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_MPDU_LOST]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->retries                = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_SHORT])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_SHORT not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->retries_short          = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_SHORT]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_LONG])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_LONG not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->retries_long           = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_LONG]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MIN])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MIN not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->contention_time_min    = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MIN]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MAX])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MAX not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->contention_time_max    = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MAX]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_AVG])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_AVG not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->contention_time_avg    = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_AVG]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_NUM_SAMPLES])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_NUM_SAMPLES not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->contention_num_samples = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_NUM_SAMPLES]);
#ifdef QC_HAL_DEBUG
    ALOGV("%4u | %6u | %6u | %7u | %7u | %7u |"
          " %7u | %8u | %7u | %12u |"
          " %11u | %17u | %17u |"
          " %17u | %20u",
          stats->ac,
          stats->tx_mpdu,
          stats->rx_mpdu,
          stats->tx_mcast,
          stats->rx_mcast,
          stats->rx_ampdu,
          stats->tx_ampdu,
          stats->mpdu_lost,
          stats->retries,
          stats->retries_short,
          stats->retries_long,
          stats->contention_time_min,
          stats->contention_time_max,
          stats->contention_time_avg,
          stats->contention_num_samples);
#endif
    return WIFI_SUCCESS;
}

static wifi_error get_wifi_rate_stat(wifi_rate_stat *stats,
                                     struct nlattr **tb_vendor)
{

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_PREAMBLE])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_PREAMBLE not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->rate.preamble        = nla_get_u8(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_PREAMBLE]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_NSS])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_NSS not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->rate.nss             = nla_get_u8(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_NSS]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BW])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BW not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->rate.bw              = nla_get_u8(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BW]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MCS_INDEX])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MCS_INDEX not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->rate.rateMcsIdx      = nla_get_u8(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MCS_INDEX]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BIT_RATE])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BIT_RATE not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->rate.bitrate         = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BIT_RATE]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_TX_MPDU])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_TX_MPDU not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->tx_mpdu              = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_TX_MPDU]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RX_MPDU])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RX_MPDU not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->rx_mpdu              = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RX_MPDU]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MPDU_LOST])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MPDU_LOST not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->mpdu_lost            = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MPDU_LOST]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->retries              = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_SHORT])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_SHORT not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->retries_short        = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_SHORT]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_LONG])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_LONG not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->retries_long         = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_LONG]);
#ifdef QC_HAL_DEBUG
    ALOGV("%8u | %3u | %2u | %10u | %7u | %6u | %6u | %8u | %7u | %12u | %11u",
          stats->rate.preamble,
          stats->rate.nss,
          stats->rate.bw,
          stats->rate.rateMcsIdx,
          stats->rate.bitrate,
          stats->tx_mpdu,
          stats->rx_mpdu,
          stats->mpdu_lost,
          stats->retries,
          stats->retries_short,
          stats->retries_long);
#endif
    return WIFI_SUCCESS;
}

static wifi_error get_wifi_peer_info(wifi_peer_info *stats,
                                     struct nlattr **tb_vendor)
{
    u32 i = 0, len = 0;
    int rem;
    wifi_rate_stat * pRateStats;
    struct nlattr *rateInfo;
    wifi_error ret = WIFI_ERROR_UNKNOWN;

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_TYPE])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_TYPE not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->type                   = (wifi_peer_type)nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_TYPE]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_MAC_ADDRESS])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_MAC_ADDRESS not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    len = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_MAC_ADDRESS]);
    len = ((sizeof(stats->peer_mac_address) <= len) ? sizeof(stats->peer_mac_address) : len);
    memcpy((void *)&stats->peer_mac_address[0], nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_MAC_ADDRESS]),
            len);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_CAPABILITIES])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_CAPABILITIES not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->capabilities           = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_CAPABILITIES]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_NUM_RATES])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_NUM_RATES not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->num_rate               = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_NUM_RATES]);
#ifdef QC_HAL_DEBUG
    ALOGV("numPeers %u  Peer MAC addr :" MAC_ADDR_STR " capabilities %0x numRate %u",
           stats->type, MAC_ADDR_ARRAY(stats->peer_mac_address),
           stats->capabilities, stats->num_rate);
#endif

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_RATE_INFO])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_RATE_INFO not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
#ifdef QC_HAL_DEBUG
    ALOGV("%8s | %3s | %2s | %10s | %7s | %6s | %6s | %8s | %7s | %12s | %11s",
          "preamble", "nss", "bw", "rateMcsIdx", "bitrate", "txMpdu", "rxMpdu", "mpduLost", "retries", "retriesShort", "retriesLong");
#endif
    for (rateInfo = (struct nlattr *) nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_RATE_INFO]), rem = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_RATE_INFO]);
            nla_ok(rateInfo, rem);
            rateInfo = nla_next(rateInfo, &(rem)))
    {
        struct nlattr *tb2[ QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX+ 1];
        if (i >= stats->num_rate) {
             ALOGE("%s: Number of rates more than expected %d", __FUNCTION__,
                   stats->num_rate);
             return WIFI_ERROR_INVALID_ARGS;
        }
        pRateStats = (wifi_rate_stat *) ((u8 *)stats->rate_stats + (i++ * sizeof(wifi_rate_stat)));

        nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX, (struct nlattr *) nla_data(rateInfo), nla_len(rateInfo), NULL);
        ret = get_wifi_rate_stat(pRateStats, tb2);
        if(ret != WIFI_SUCCESS)
        {
            return ret;
        }
    }
    return WIFI_SUCCESS;
}

wifi_error LLStatsCommand::get_wifi_ml_iface_link_states(wifi_iface_ml_stat *stats,
                                                struct nlattr **tb_link_vendor)
{
    struct nlattr *linkInfo;
    int rem, i;

    for (linkInfo = (struct nlattr *) nla_data(tb_link_vendor[
         QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG]),
         rem = nla_len(tb_link_vendor[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG]);
                                 nla_ok(linkInfo, rem);
                                 linkInfo = nla_next(linkInfo, &(rem)))
    {
        struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_MAX + 1];
        int link_state, link_id;

        nla_parse(tb, QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_MAX,
                  (struct nlattr *) nla_data(linkInfo),
                   nla_len(linkInfo), NULL);

        if (!tb[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_LINK_ID] ||
            !tb[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_STATE])
        {
            ALOGE("%s: link ID or link state not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }

        link_id = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_LINK_ID]);
        link_state = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_STATE]);
#ifdef QC_HAL_DEBUG
        ALOGV("%s: link id %d, link_state %d", __FUNCTION__, link_id, link_state);
#endif
        for (i = 0; i < stats->num_links; i++) {
            if (stats->links[i].link_id == link_id) {
                stats->links[i].state =
                        (link_state == QCA_WLAN_VENDOR_LINK_STATE_INACTIVE) ?
                            WIFI_LINK_STATE_NOT_IN_USE : WIFI_LINK_STATE_IN_USE;
                break;
            }
        }
    }
    return WIFI_SUCCESS;
}

int LLStatsCommand::get_wifi_ml_iface_numlinks(struct nlattr **tb_vendor)
{
    int rem, numlink = 0;
    struct nlattr *mloInfo;

    for (mloInfo = (struct nlattr *) nla_data(tb_vendor[
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK]),
                             rem = nla_len(tb_vendor[
                             QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK]);
                                nla_ok(mloInfo, rem);
                                mloInfo = nla_next(mloInfo, &(rem)))
    {
        struct nlattr *tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX + 1];

        nla_parse(tb_vendor2, QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX,
                  (struct nlattr *) nla_data(mloInfo),
                   nla_len(mloInfo), NULL);

        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK_ID])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK_ID"
                    "not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        if (nla_get_u8(tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK_ID]) >=
                       MAX_NUM_MLO_LINKS) {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK_ID value %d is not valid",
                  __FUNCTION__, nla_get_u8(tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK_ID]));
            return WIFI_ERROR_INVALID_ARGS;
        }
        numlink++;
    }
    return numlink;
}

wifi_error LLStatsCommand::get_wifi_ml_iface_stats(wifi_iface_ml_stat *stats,
                                                struct nlattr **tb_vendor)
{
    wifi_error ret = WIFI_ERROR_UNKNOWN;
    struct nlattr *mloInfo;
    int rem, numlink = 0;

    for (mloInfo = (struct nlattr *) nla_data(tb_vendor[
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK]),
                             rem = nla_len(tb_vendor[
                             QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK]);
                                nla_ok(mloInfo, rem);
                                mloInfo = nla_next(mloInfo, &(rem)))
    {
        struct nlattr *wmmInfo;
        wifi_wmm_ac_stat *pWmmStats;
        int i=0, rem1;
        struct nlattr *tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX + 1];

        if (numlink >= stats->num_links) {
             ALOGE("%s: Number of links more than expected", __FUNCTION__);
             return WIFI_ERROR_INVALID_ARGS;
        }
        nla_parse(tb_vendor2, QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX,
                  (struct nlattr *) nla_data(mloInfo),
                   nla_len(mloInfo), NULL);

        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK_ID])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK_ID"
                    "not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        stats->links[numlink].link_id = nla_get_u8(tb_vendor2[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK_ID]);

        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ID])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ID"
                    "not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        stats->links[numlink].radio = nla_get_u32(tb_vendor2[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ID]);

        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ"
                    "not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        stats->links[numlink].frequency = (wifi_channel)nla_get_u32(tb_vendor2[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ]);

        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_BEACON_RX])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_BEACON_RX"
                    "not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        stats->links[numlink].beacon_rx = nla_get_u32(tb_vendor2[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_BEACON_RX]);

        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_AVERAGE_TSF_OFFSET])
        {
            stats->links[numlink].average_tsf_offset = 0;
        } else {
            stats->links[numlink].average_tsf_offset = nla_get_u64(tb_vendor2[
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_AVERAGE_TSF_OFFSET]);
        }

        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_DETECTED])
        {
            stats->links[numlink].leaky_ap_detected = 0;
        } else {
            stats->links[numlink].leaky_ap_detected = nla_get_u32(tb_vendor2[
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_DETECTED]);
        }

        if (!tb_vendor2[
            QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_AVG_NUM_FRAMES_LEAKED])
        {
            stats->links[numlink].leaky_ap_avg_num_frames_leaked = 0;
        } else {
            stats->links[numlink].leaky_ap_avg_num_frames_leaked = nla_get_u32(tb_vendor2[
               QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_AVG_NUM_FRAMES_LEAKED]);
        }

        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_GUARD_TIME])
        {
            stats->links[numlink].leaky_ap_guard_time = 0;
        } else {
            stats->links[numlink].leaky_ap_guard_time = nla_get_u32(tb_vendor2[
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_GUARD_TIME]);
        }

        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_RX])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_RX"
                    " not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        stats->links[numlink].mgmt_rx = nla_get_u32(tb_vendor2[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_RX]);

        if (!tb_vendor2[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_RX])
        {
            ALOGE("%s: "
                    "QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_RX"
                    " not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        stats->links[numlink].mgmt_action_rx  = nla_get_u32(tb_vendor2[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_RX]);

        if (!tb_vendor2[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_TX])
        {
            ALOGE("%s: "
                    "QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_TX"
                    " not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        stats->links[numlink].mgmt_action_tx  = nla_get_u32(tb_vendor2[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_TX]);

        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_MGMT])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_MGMT"
                    " not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        stats->links[numlink].rssi_mgmt = get_s32(tb_vendor2[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_MGMT]);

        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_DATA])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_DATA"
                    " not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        stats->links[numlink].rssi_data = get_s32(tb_vendor2[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_DATA]);

        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_ACK])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_ACK"
                    " not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        stats->links[numlink].rssi_ack = get_s32(tb_vendor2[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_ACK]);
#ifdef QC_HAL_DEBUG
        ALOGV("WMM STATS");
        ALOGV("linkId : %d "
              "radio : %d "
              "beaconRx : %u "
              "mgmtRx : %u "
              "mgmtActionRx  : %u "
              "mgmtActionTx : %u "
              "rssiMgmt : %d "
              "rssiData : %d "
              "rssiAck  : %d ",
              stats->links[numlink].link_id,
              stats->links[numlink].radio,
              stats->links[numlink].beacon_rx,
              stats->links[numlink].mgmt_rx,
              stats->links[numlink].mgmt_action_rx,
              stats->links[numlink].mgmt_action_tx,
              stats->links[numlink].rssi_mgmt,
              stats->links[numlink].rssi_data,
              stats->links[numlink].rssi_ack);
#endif
        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_INFO])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_INFO"
                    " not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
#ifdef QC_HAL_DEBUG
        ALOGV("%2s | %6s | %6s | %7s | %7s | %7s |"
              " %7s | %8s | %7s | %12s |"
              " %11s | %17s | %17s |"
              " %17s | %20s",
              "ac","txMpdu", "rxMpdu", "txMcast", "rxMcast", "rxAmpdu",
              "txAmpdu", "mpduLost", "retries", "retriesShort",
              "retriesLong", "contentionTimeMin", "contentionTimeMax",
              "contentionTimeAvg", "contentionNumSamples");
#endif
        for (wmmInfo = (struct nlattr *) nla_data(tb_vendor2[
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_INFO]),
                rem1 = nla_len(tb_vendor2[
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_INFO]);
                nla_ok(wmmInfo, rem1);
                wmmInfo = nla_next(wmmInfo, &(rem1)))
        {
            struct nlattr *tb2[ QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX+ 1];
            if (i >= WIFI_AC_MAX) {
                ALOGE("%s: Number of WMM AC stats more than expected", __FUNCTION__);
                return WIFI_ERROR_INVALID_ARGS;
            }
            pWmmStats = (wifi_wmm_ac_stat *) ((u8 *)stats->links[numlink].ac
                    + (i++ * sizeof(wifi_wmm_ac_stat)));
            nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX,
                    (struct nlattr *) nla_data(wmmInfo),
                    nla_len(wmmInfo), NULL);
            ret = get_wifi_wmm_ac_stat(pWmmStats, tb2);
            if(ret != WIFI_SUCCESS)
            {
                return ret;
            }
        }

        if (!tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_TS_DUTY_CYCLE])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_TS_DUTY_CYCLE not found",
                  __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        stats->links[numlink].time_slicing_duty_cycle_percent =
            nla_get_u8(tb_vendor2[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_TS_DUTY_CYCLE]);
        numlink++;
    }

    return WIFI_SUCCESS;
}

wifi_error LLStatsCommand::get_wifi_iface_stats(wifi_iface_stat *stats,
                                                struct nlattr **tb_vendor)
{
    struct nlattr *wmmInfo;
    wifi_wmm_ac_stat *pWmmStats;
    int i=0, rem;
    wifi_error ret = WIFI_ERROR_UNKNOWN;

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_BEACON_RX])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_BEACON_RX"
                "not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->beacon_rx = nla_get_u32(tb_vendor[
            QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_BEACON_RX]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_AVERAGE_TSF_OFFSET])
    {
        stats->average_tsf_offset = 0;
    } else {
        stats->average_tsf_offset = nla_get_u64(tb_vendor[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_AVERAGE_TSF_OFFSET]);
    }

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_DETECTED])
    {
        stats->leaky_ap_detected = 0;
    } else {
        stats->leaky_ap_detected = nla_get_u32(tb_vendor[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_DETECTED]);
    }

    if (!tb_vendor[
        QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_AVG_NUM_FRAMES_LEAKED])
    {
        stats->leaky_ap_avg_num_frames_leaked = 0;
    } else {
        stats->leaky_ap_avg_num_frames_leaked = nla_get_u32(tb_vendor[
           QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_AVG_NUM_FRAMES_LEAKED]);
    }

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_GUARD_TIME])
    {
        stats->leaky_ap_guard_time = 0;
    } else {
        stats->leaky_ap_guard_time = nla_get_u32(tb_vendor[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_GUARD_TIME]);
    }

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_RX])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_RX"
                " not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->mgmt_rx         = nla_get_u32(tb_vendor[
            QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_RX]);

    if (!tb_vendor[
            QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_RX])
    {
        ALOGE("%s: "
                "QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_RX"
                " not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->mgmt_action_rx  = nla_get_u32(tb_vendor[
            QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_RX]);

    if (!tb_vendor[
            QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_TX])
    {
        ALOGE("%s: "
                "QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_TX"
                " not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->mgmt_action_tx  = nla_get_u32(tb_vendor[
            QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_TX]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_MGMT])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_MGMT"
                " not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->rssi_mgmt       = get_s32(tb_vendor[
            QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_MGMT]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_DATA])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_DATA"
                " not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->rssi_data       = get_s32(tb_vendor[
            QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_DATA]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_ACK])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_ACK"
                " not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->rssi_ack        = get_s32(tb_vendor[
            QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_ACK]);
#ifdef QC_HAL_DEBUG
    ALOGV("WMM STATS");
    ALOGV("beaconRx : %u "
          "mgmtRx : %u "
          "mgmtActionRx  : %u "
          "mgmtActionTx : %u "
          "rssiMgmt : %d "
          "rssiData : %d "
          "rssiAck  : %d ",
          stats->beacon_rx,
          stats->mgmt_rx,
          stats->mgmt_action_rx,
          stats->mgmt_action_tx,
          stats->rssi_mgmt,
          stats->rssi_data,
          stats->rssi_ack);
#endif
    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_INFO])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_INFO"
                " not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
#ifdef QC_HAL_DEBUG
    ALOGV("%4s | %6s | %6s | %7s | %7s | %7s |"
          " %7s | %8s | %7s | %12s |"
          " %11s | %17s | %17s |"
          " %17s | %20s",
          "ac","txMpdu", "rxMpdu", "txMcast", "rxMcast", "rxAmpdu",
          "txAmpdu", "mpduLost", "retries", "retriesShort",
          "retriesLong", "contentionTimeMin", "contentionTimeMax",
          "contentionTimeAvg", "contentionNumSamples");
#endif
    for (wmmInfo = (struct nlattr *) nla_data(tb_vendor[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_INFO]),
            rem = nla_len(tb_vendor[
                QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_INFO]);
            nla_ok(wmmInfo, rem);
            wmmInfo = nla_next(wmmInfo, &(rem)))
    {
        struct nlattr *tb2[ QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX+ 1];
        pWmmStats = (wifi_wmm_ac_stat *) ((u8 *)stats->ac
                + (i++ * sizeof(wifi_wmm_ac_stat)));
        nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX,
                (struct nlattr *) nla_data(wmmInfo),
                nla_len(wmmInfo), NULL);
        ret = get_wifi_wmm_ac_stat(pWmmStats, tb2);
        if(ret != WIFI_SUCCESS)
        {
            return ret;
        }
    }

    return WIFI_SUCCESS;
}

static wifi_error get_wifi_radio_stats(wifi_radio_stat *stats,
                                       struct nlattr **tb_vendor)
{
    u32 i = 0;
    struct nlattr *chInfo;
    wifi_channel_stat *pChStats;
    int rem;

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ID])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ID not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->radio             = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ID]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->on_time           = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_TX_TIME])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_TX_TIME not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->tx_time           = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_TX_TIME]);

    if (stats->num_tx_levels) {
        if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_TX_TIME_PER_LEVEL]) {
            ALOGE("%s: num_tx_levels is %u but QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_TX_TIME_PER_LEVEL not found", __func__, stats->num_tx_levels);
            stats->num_tx_levels = 0;
            return WIFI_ERROR_INVALID_ARGS;
        }
        stats->tx_time_per_levels =
                             (u32 *) malloc(sizeof(u32) * stats->num_tx_levels);
        if (!stats->tx_time_per_levels) {
            ALOGE("%s: radio_stat: tx_time_per_levels malloc Failed", __func__);
            stats->num_tx_levels = 0;
            return WIFI_ERROR_OUT_OF_MEMORY;
        }

        nla_memcpy(stats->tx_time_per_levels,
            tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_TX_TIME_PER_LEVEL],
            sizeof(u32) * stats->num_tx_levels);
    }

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_RX_TIME])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_RX_TIME not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->rx_time           = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_RX_TIME]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_SCAN])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_SCAN not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->on_time_scan      = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_SCAN]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_NBD])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_NBD not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->on_time_nbd       = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_NBD]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_GSCAN])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_GSCAN not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->on_time_gscan     = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_GSCAN]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_ROAM_SCAN])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_ROAM_SCAN not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->on_time_roam_scan = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_ROAM_SCAN]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_PNO_SCAN])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_PNO_SCAN not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->on_time_pno_scan  = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_PNO_SCAN]);

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_HS20])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_HS20 not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->on_time_hs20      = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_HS20]);


    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_CHANNELS])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_CHANNELS not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    stats->num_channels                           = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_CHANNELS]);

    if (stats->num_channels == 0) {
        return WIFI_SUCCESS;
    }

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CH_INFO])
    {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_CH_INFO not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    for (chInfo = (struct nlattr *) nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CH_INFO]), rem = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CH_INFO]);
            nla_ok(chInfo, rem);
            chInfo = nla_next(chInfo, &(rem)))
    {
        struct nlattr *tb2[ QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX+ 1];
        pChStats = (wifi_channel_stat *) ((u8 *)stats->channels + (i++ * (sizeof(wifi_channel_stat))));
        nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX, (struct nlattr *) nla_data(chInfo), nla_len(chInfo), NULL);

        if (!tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_WIDTH])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_WIDTH not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        pChStats->channel.width                  = (wifi_channel_width)nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_WIDTH]);

        if (!tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        pChStats->channel.center_freq            = (wifi_channel)nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ]);

        if (!tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ0])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ0 not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        pChStats->channel.center_freq0           = (wifi_channel)nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ0]);

        if (!tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ1])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ1 not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        pChStats->channel.center_freq1           = (wifi_channel)nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ1]);

        if (!tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_ON_TIME])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_ON_TIME not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        pChStats->on_time                = nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_ON_TIME]);

        if (!tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_CCA_BUSY_TIME])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_CCA_BUSY_TIME not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        pChStats->cca_busy_time          = nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_CCA_BUSY_TIME]);

        if (tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_RX_TIME] &&
            nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_RX_TIME]) <= pChStats->cca_busy_time)
            pChStats->cca_busy_time -= nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_RX_TIME]);

    }
    return WIFI_SUCCESS;
}

void LLStatsCommand::getClearRspParams(u32 *stats_clear_rsp_mask, u8 *stop_rsp)
{
    *stats_clear_rsp_mask =  mClearRspParams.stats_clear_rsp_mask;
    *stop_rsp = mClearRspParams.stop_rsp;
}

wifi_error LLStatsCommand::requestResponse()
{
    return WifiCommand::requestResponse(mMsg);
}

bool LLStatsCommand::isMlo()
{
    return mResultsParams.iface_ml_stat ? true : false;
}

wifi_link_stat * LLStatsCommand::copyMloPeerStats(wifi_link_stat *linkInfo, u8 *resultsBufEnd)
{
    int i, peer_offset;
    int link_num_peers = 0, total_num_rates = 0;
    wifi_peer_info *peer, *linkPeerStats;

    linkPeerStats = linkInfo->peer_info;
    if (!mPeerResultsParams.num_peers)
        goto out;

    for (i = 0; i < mPeerResultsParams.num_peers; i++) {

        peer_offset = (i * sizeof(wifi_peer_info)) +
                       (total_num_rates * sizeof(wifi_rate_stat));
        peer = (wifi_peer_info *) ((u8 *)(mPeerResultsParams.peers_info) +
                                   peer_offset);

        if (mPeerResultsParams.link_ids &&
            mPeerResultsParams.link_ids[i] == linkInfo->link_id) {
            u8 *nextLinkPeerOffset = ((u8 *) linkPeerStats) +
                sizeof(wifi_peer_info) + (peer->num_rate * sizeof(wifi_rate_stat));

#ifdef QC_HAL_DEBUG
            ALOGV("%s: link peer pointer %p, num_rates %d", __FUNCTION__,
                  linkPeerStats, peer->num_rate);
#endif
            if (nextLinkPeerOffset > resultsBufEnd) {
                ALOGE("%s: Buffer overflow while preparing response data %p",
                      __FUNCTION__, nextLinkPeerOffset);
                return NULL;
            }

            memcpy(linkPeerStats, peer, sizeof(wifi_peer_info));
            memcpy(linkPeerStats->rate_stats, peer->rate_stats,
                   peer->num_rate * sizeof(wifi_rate_stat));

            linkPeerStats = (wifi_peer_info *) nextLinkPeerOffset;
            link_num_peers++;
        }
        total_num_rates += peer->num_rate;
    }

out:
    linkInfo->num_peers = link_num_peers;
#ifdef QC_HAL_DEBUG
    ALOGV("%s: link ID: %d, num_peers:%d ", __FUNCTION__,
           linkInfo->link_id, linkInfo->num_peers);
#endif

    return (wifi_link_stat *) linkPeerStats;
}

wifi_error LLStatsCommand::copyMloStats()
{
    wifi_iface_ml_stat *pMlIfaceStat = NULL;
    wifi_link_stat *linkInfo;
    wifi_error status = WIFI_ERROR_NONE;
    u32 mlResultsBufSize;
    u8 *resultsBufEnd;
    int i;

    if (!mResultsParams.iface_ml_stat) {
        ALOGE("%s: MLO stats not found");
        goto cleanup;
    }

    mlResultsBufSize = sizeof(wifi_iface_ml_stat) +
                       (mResultsParams.iface_ml_stat->num_links *
                        sizeof(wifi_link_stat));
    if (mPeerResultsParams.num_peers) {
        mlResultsBufSize += (mPeerResultsParams.num_peers *
                             sizeof(wifi_peer_info)) +
                            (mPeerResultsParams.num_rates *
                             sizeof(wifi_rate_stat));
    } else {
        ALOGI("%s:Peers info not reported", __FUNCTION__);
    }


    pMlIfaceStat = (wifi_iface_ml_stat *) malloc(mlResultsBufSize);
    if (!pMlIfaceStat)
    {
        ALOGE("%s: pMlIfaceStat: malloc failed", __FUNCTION__);
        status = WIFI_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }

    resultsBufEnd = ((u8 *) pMlIfaceStat) + mlResultsBufSize;
#ifdef QC_HAL_DEBUG
    ALOGV("%s: mlResultsBufSize %d, result buffer start: %p, result buffer end: %p",
          __FUNCTION__, mlResultsBufSize, pMlIfaceStat, resultsBufEnd);
#endif

    memset(pMlIfaceStat, 0, mlResultsBufSize);
    memcpy(pMlIfaceStat, mResultsParams.iface_ml_stat,
           sizeof(wifi_iface_ml_stat));

    linkInfo = pMlIfaceStat->links;
    for (i = 0; i < pMlIfaceStat->num_links; i++) {
        if ((((u8 *) linkInfo) + sizeof(wifi_link_stat)) > resultsBufEnd) {
            ALOGE("%s: Buffer overflow while preparing response data", __FUNCTION__);
            status = WIFI_ERROR_UNKNOWN;
            goto cleanup;
        }

        memcpy(linkInfo, &(mResultsParams.iface_ml_stat->links[i]),
               sizeof(wifi_link_stat));
#ifdef QC_HAL_DEBUG
        ALOGV("%s: link ID: %d, state:%d pointer %p", __FUNCTION__,
              linkInfo->link_id, linkInfo->state, linkInfo);
#endif
        linkInfo = copyMloPeerStats(linkInfo, resultsBufEnd);
        if (!linkInfo) {
            status = WIFI_ERROR_UNKNOWN;
            goto cleanup;
        }
    }

    free(mResultsParams.iface_ml_stat);
    mResultsParams.iface_ml_stat = pMlIfaceStat;

    return WIFI_SUCCESS;

cleanup:
    if (pMlIfaceStat)
        free(pMlIfaceStat);
    return status;
}

wifi_error LLStatsCommand::notifyResponse()
{
    wifi_error ret = WIFI_ERROR_UNKNOWN;

    /* Indicate stats to framework only if both radio and iface stats
     * are present */
    if (mResultsParams.radio_stat && (mResultsParams.iface_stat ||
        mResultsParams.iface_ml_stat)) {
        if (mNumRadios > mNumRadiosAllocated) {
            ALOGE("%s: Force reset mNumRadios=%d to allocated=%d",
                    __FUNCTION__, mNumRadios, mNumRadiosAllocated);
            mNumRadios = mNumRadiosAllocated;
        }
        if (mResultsParams.iface_ml_stat) {
            mHandler.on_multi_link_stats_results(mRequestId,
                                                 mResultsParams.iface_ml_stat,
                                                 mNumRadios,
                                                 mResultsParams.radio_stat);
        } else {
            mHandler.on_link_stats_results(mRequestId,
                                           mResultsParams.iface_stat,
                                           mNumRadios,
                                           mResultsParams.radio_stat);
        }
        ret = WIFI_SUCCESS;
    } else {
        ret = WIFI_ERROR_INVALID_ARGS;
    }

    clearStats();

    return ret;
}


void LLStatsCommand::clearStats()
{
    if(mResultsParams.radio_stat)
    {
        wifi_radio_stat *radioStat = mResultsParams.radio_stat;
        if (mNumRadios > mNumRadiosAllocated) {
            ALOGE("%s: Force reset mNumRadios=%d to allocated=%d",
                    __FUNCTION__, mNumRadios, mNumRadiosAllocated);
            mNumRadios = mNumRadiosAllocated;
        }
        for (u8 radio = 0; radio < mNumRadios; radio++) {
            if (radioStat->tx_time_per_levels) {
                free(radioStat->tx_time_per_levels);
                radioStat->tx_time_per_levels = NULL;
            }
            radioStat = (wifi_radio_stat *)((u8 *)radioStat +
                sizeof(wifi_radio_stat) +  (sizeof(wifi_channel_stat) *
                    radioStat->num_channels));
        }
        free(mResultsParams.radio_stat);
        mResultsParams.radio_stat = NULL;
        mRadioStatsSize = 0;
        mNumRadios = 0;
        mNumRadiosAllocated = 0;
     }
     if(mResultsParams.iface_stat)
     {
        free(mResultsParams.iface_stat);
        mResultsParams.iface_stat = NULL;
     }
     if(mResultsParams.iface_ml_stat)
     {
        free(mResultsParams.iface_ml_stat);
        mResultsParams.iface_ml_stat = NULL;
     }
     if(mPeerResultsParams.link_ids)
     {
        free(mPeerResultsParams.link_ids);
        mPeerResultsParams.link_ids = NULL;
     }
     if(mPeerResultsParams.peers_info)
     {
        free(mPeerResultsParams.peers_info);
        mPeerResultsParams.peers_info = NULL;
     }
     mPeerResultsParams.num_peers = 0;
     mPeerResultsParams.num_rates = 0;
}


int LLStatsCommand::handleResponse(WifiEvent &reply)
{
    unsigned i=0;
    int status = WIFI_ERROR_NONE;
    WifiVendorCommand::handleResponse(reply);

    // Parse the vendordata and get the attribute

    switch(mSubcmd)
    {
        case QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET:
        {
            u32 resultsBufSize = 0;
            struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX + 1];
            int rem;
            wifi_radio_stat *radioStatsBuf;

            nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX,
                    (struct nlattr *)mVendorData,
                    mDataLen, NULL);

            if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE])
            {
                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE not found",
                        __FUNCTION__);
                status = WIFI_ERROR_INVALID_ARGS;
                goto cleanup;
            }

            switch(nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE]))
            {
                case QCA_NL80211_VENDOR_SUBCMD_LL_STATS_TYPE_RADIO:
                {
                    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_NUM_RADIOS])
                    {
                        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_NUM_RADIOS"
                              " not found", __FUNCTION__);
                        return WIFI_ERROR_INVALID_ARGS;
                    }
                    mNumRadios = nla_get_u32(tb_vendor[
                                    QCA_WLAN_VENDOR_ATTR_LL_STATS_NUM_RADIOS]);

                    if (!tb_vendor[
                        QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_CHANNELS
                        ])
                    {
                        ALOGE("%s:"
                            "QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_CHANNELS"
                            " not found", __FUNCTION__);
                        status = WIFI_ERROR_INVALID_ARGS;
                        goto cleanup;
                    }

                    resultsBufSize += (nla_get_u32(tb_vendor[
                            QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_CHANNELS])
                            * sizeof(wifi_channel_stat)
                            + sizeof(wifi_radio_stat));

                    radioStatsBuf = (wifi_radio_stat *)realloc(
                                              mResultsParams.radio_stat,
                                              mRadioStatsSize + resultsBufSize);
                    if (!radioStatsBuf)
                    {
                        ALOGE("%s: radio_stat: malloc Failed", __FUNCTION__);
                        status = WIFI_ERROR_OUT_OF_MEMORY;
                        goto cleanup;
                    }
                    mResultsParams.radio_stat = radioStatsBuf;

                    //Move the buffer to populate current radio stats
                    radioStatsBuf = (wifi_radio_stat *)(
                                                (u8 *)mResultsParams.radio_stat
                                                            + mRadioStatsSize);
                    memset(radioStatsBuf, 0, resultsBufSize);
                    mRadioStatsSize += resultsBufSize;
                    mNumRadiosAllocated ++;

                    if (tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_TX_LEVELS])
                        radioStatsBuf->num_tx_levels = nla_get_u32(tb_vendor[
                                            QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_TX_LEVELS]);

                    wifi_channel_stat *pWifiChannelStats;
                    status = get_wifi_radio_stats(radioStatsBuf,
                              tb_vendor);
                    if(status != WIFI_SUCCESS)
                    {
                        goto cleanup;
                    }
#ifdef QC_HAL_DEBUG
                    ALOGV("radio :%u onTime :%u txTime :%u rxTime :%u"
                          " onTimeScan :%u onTimeNbd :%u onTimeGscan :%u"
                          " onTimeRoamScan :%u onTimePnoScan :%u"
                          " onTimeHs20 :%u numChannels :%u num_tx_levels: %u",
                          radioStatsBuf->radio,
                          radioStatsBuf->on_time,
                          radioStatsBuf->tx_time,
                          radioStatsBuf->rx_time,
                          radioStatsBuf->on_time_scan,
                          radioStatsBuf->on_time_nbd,
                          radioStatsBuf->on_time_gscan,
                          radioStatsBuf->on_time_roam_scan,
                          radioStatsBuf->on_time_pno_scan,
                          radioStatsBuf->on_time_hs20,
                          radioStatsBuf->num_channels,
                          radioStatsBuf->num_tx_levels);
#ifdef QC_HAL_DEBUG
                    for (i = 0; i < radioStatsBuf->num_tx_levels; i++) {
                        ALOGV("Power level: %u  tx_time: %u", i,
                              radioStatsBuf->tx_time_per_levels[i]);
                    }
#endif
                    ALOGV("%5s | %10s | %11s | %11s | %6s | %11s", "width",
                          "CenterFreq", "CenterFreq0", "CenterFreq1",
                          "onTime", "ccaBusyTime");
#endif
                    for ( i=0; i < radioStatsBuf->num_channels; i++)
                    {
                        pWifiChannelStats =
                            (wifi_channel_stat *) (
                                (u8 *)radioStatsBuf->channels
                                + (i * sizeof(wifi_channel_stat)));

#ifdef QC_HAL_DEBUG
                        ALOGV("%5u | %10u | %11u | %11u | %6u | %11u",
                              pWifiChannelStats->channel.width,
                              pWifiChannelStats->channel.center_freq,
                              pWifiChannelStats->channel.center_freq0,
                              pWifiChannelStats->channel.center_freq1,
                              pWifiChannelStats->on_time,
                              pWifiChannelStats->cca_busy_time);
#endif
                    }
                }
                break;

                case QCA_NL80211_VENDOR_SUBCMD_LL_STATS_TYPE_IFACE:
                {
                    if (mHandler.on_multi_link_stats_results &&
                        tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK]) {
                        int numLink;

                        numLink = get_wifi_ml_iface_numlinks(tb_vendor);
                        ALOGE("%s: Number of MLO links %d", __FUNCTION__, numLink);
                        if(numLink <= 0 || numLink > MAX_NUM_MLO_LINKS)
                        {
                            status = WIFI_ERROR_INVALID_ARGS;
                            goto cleanup;
                        }
                        resultsBufSize = (numLink * sizeof(wifi_link_stat)
                                + sizeof(wifi_iface_ml_stat));
                        mResultsParams.iface_ml_stat =
                            (wifi_iface_ml_stat *) malloc (resultsBufSize);
                        if (!mResultsParams.iface_ml_stat)
                        {
                            ALOGE("%s: iface_ml_stat: malloc failed", __FUNCTION__);
                            status = WIFI_ERROR_OUT_OF_MEMORY;
                            goto cleanup;
                        }

#ifdef QC_HAL_DEBUG
                        ALOGV("%s: resultsBufSize %d, mResultsParams.iface_ml_stat %p,"
                              " wifi_link_stat %d, wifi_iface_ml_stat %d,"
                              " mResultsParams.iface_ml_stat end %p",
                              __FUNCTION__, resultsBufSize, mResultsParams.iface_ml_stat,
                              sizeof(wifi_link_stat), sizeof(wifi_iface_ml_stat),
                              (u8 *) mResultsParams.iface_ml_stat + resultsBufSize);
#endif
                        memset(mResultsParams.iface_ml_stat, 0, resultsBufSize);
                        status = get_wifi_interface_info(
                                &mResultsParams.iface_ml_stat->info, tb_vendor);
                        if(status != WIFI_SUCCESS)
                        {
                            goto cleanup;
                        }
                        mResultsParams.iface_ml_stat->num_links = numLink;
                        status = get_wifi_ml_iface_stats(
                                 mResultsParams.iface_ml_stat, tb_vendor);
                        if(status != WIFI_SUCCESS)
                        {
                            goto cleanup;
                        }
                       for (i = 0; i < mResultsParams.iface_ml_stat->num_links; i++) {
                           mResultsParams.iface_ml_stat->links[i].state = WIFI_LINK_STATE_UNKNOWN;
                       }
                    } else {
                        resultsBufSize = sizeof(wifi_iface_stat);
                        mResultsParams.iface_stat =
                            (wifi_iface_stat *) malloc (resultsBufSize);
                        if (!mResultsParams.iface_stat)
                        {
                            ALOGE("%s: iface_stat: malloc Failed", __FUNCTION__);
                            status = WIFI_ERROR_OUT_OF_MEMORY;
                            goto cleanup;
                        }
                        memset(mResultsParams.iface_stat, 0, resultsBufSize);
                        status = get_wifi_interface_info(
                                &mResultsParams.iface_stat->info, tb_vendor);
                        if(status != WIFI_SUCCESS)
                        {
                            goto cleanup;
                        }
                        status = get_wifi_iface_stats(mResultsParams.iface_stat,
                                tb_vendor);
                        if(status != WIFI_SUCCESS)
                        {
                            goto cleanup;
                        }
                    }
                }
                break;

                case QCA_NL80211_VENDOR_SUBCMD_LL_STATS_TYPE_PEERS:
                {
                    struct nlattr *peerInfo;
                    u32 numPeers, numRates = 0;
                    bool isMlo = false;

                    if (!tb_vendor[
                            QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_NUM_PEERS])
                    {
                        ALOGE("%s:QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_NUM_PEERS"
                              " not found", __FUNCTION__);
                        status = WIFI_ERROR_INVALID_ARGS;
                        goto cleanup;
                    }
#ifdef QC_HAL_DEBUG
                    ALOGV(" numPeers is %u in %s\n",
                            nla_get_u32(tb_vendor[
                            QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_NUM_PEERS]),
                            __FUNCTION__);
#endif
                    if((numPeers = nla_get_u32(tb_vendor[
                        QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_NUM_PEERS])) > 0)
                    {
                        if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO])
                        {
                            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO"
                                    " not found", __FUNCTION__);
                            status = WIFI_ERROR_INVALID_ARGS;
                            goto cleanup;
                        }
                        for (peerInfo = (struct nlattr *) nla_data(tb_vendor[
                             QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO]),
                             rem = nla_len(tb_vendor[
                             QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO]);
                                nla_ok(peerInfo, rem);
                                peerInfo = nla_next(peerInfo, &(rem)))
                        {
                            struct nlattr *tb2[
                                QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX+ 1];

                            nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX,
                                    (struct nlattr *) nla_data(peerInfo),
                                    nla_len(peerInfo), NULL);

                            if (tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK_ID])
                                isMlo = true;

                            if (!tb2[
                             QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_NUM_RATES])
                            {
                                ALOGE("%s:"
                             "QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_NUM_RATES"
                             " not found", __FUNCTION__);
                                status = WIFI_ERROR_INVALID_ARGS;
                                goto cleanup;
                            }
                            numRates += nla_get_u32(tb2[
                            QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_NUM_RATES]);
                        }

                        if (isMlo && mHandler.on_multi_link_stats_results) {
                            wifi_peer_info *pPeerStats, *pMloPeerStats = NULL;
                            u8 *pPeerLinkIDs = NULL;

                            resultsBufSize = (numPeers * sizeof(wifi_peer_info)
                                    + numRates * sizeof(wifi_rate_stat));

                            pMloPeerStats = (wifi_peer_info *) malloc (resultsBufSize);
                            pPeerLinkIDs = (u8 *) malloc (numPeers * sizeof(u8));

                            if (!pMloPeerStats || !pPeerLinkIDs)
                            {
                                ALOGE("%s: pMloPeerStats or pPeerLinkIDs: "
                                      "malloc Failed", __FUNCTION__);
                                status = WIFI_ERROR_OUT_OF_MEMORY;
                                goto cleanup;
                            }
#ifdef QC_HAL_DEBUG
                            ALOGV("%s: numPeers %d, total numRates %d",
                                  __FUNCTION__, numPeers, numRates);
#endif
                            memset(pMloPeerStats, 0, resultsBufSize);
                            memset(pPeerLinkIDs, 0, (numPeers * sizeof(u8)));

                            mPeerResultsParams.link_ids = pPeerLinkIDs;
                            mPeerResultsParams.peers_info = pMloPeerStats;
                            mPeerResultsParams.num_peers = numPeers;
                            mPeerResultsParams.num_rates = numRates;

                            numRates = 0;
                            numPeers = 0;
                            for (peerInfo = (struct nlattr *) nla_data(tb_vendor[
                                QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO]),
                                rem = nla_len(tb_vendor[
                                        QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO]);
                                nla_ok(peerInfo, rem);
                                peerInfo = nla_next(peerInfo, &(rem)))
                            {
                                struct nlattr *tb2[
                                    QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX+ 1];

                                nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX,
                                    (struct nlattr *) nla_data(peerInfo),
                                    nla_len(peerInfo), NULL);

                                if (numPeers >= mPeerResultsParams.num_peers) {
                                    ALOGE("%s: Number of peers more than expected",
                                          __FUNCTION__);
                                    status = WIFI_ERROR_INVALID_ARGS;
                                    goto cleanup;
                                }
                                if (tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK_ID]){
                                    pPeerLinkIDs[numPeers] =
                                        nla_get_u8(tb2[QCA_WLAN_VENDOR_ATTR_LL_STATS_MLO_LINK_ID]);
                                } else {
                                    ALOGE("%s: No link id for peer in MLO "
                                          "connection", __FUNCTION__);
                                    status = WIFI_ERROR_INVALID_ARGS;
                                    goto cleanup;
                                }

                                pPeerStats = (wifi_peer_info *) ((u8 *) pMloPeerStats
                                               + (numPeers * sizeof(wifi_peer_info))
                                               + (numRates * sizeof(wifi_rate_stat)));

                                status = get_wifi_peer_info(pPeerStats, tb2);
                                if(status != WIFI_SUCCESS)
                                {
                                    goto cleanup;
                                }

                                numRates += pPeerStats->num_rate;
                                numPeers++;
                            }
                        } else {
                            wifi_iface_stat *pIfaceStat;

                            resultsBufSize += (numPeers * sizeof(wifi_peer_info)
                                    + numRates * sizeof(wifi_rate_stat)
                                    + sizeof (wifi_iface_stat));
                            pIfaceStat = (wifi_iface_stat *) malloc (
                                    resultsBufSize);
                            if (!pIfaceStat)
                            {
                                ALOGE("%s: pIfaceStat: malloc Failed", __FUNCTION__);
                                status = WIFI_ERROR_OUT_OF_MEMORY;
                                goto cleanup;
                            }

                            memset(pIfaceStat, 0, resultsBufSize);
                            if(mResultsParams.iface_stat) {
                                if(resultsBufSize >= sizeof(wifi_iface_stat)) {
                                    memcpy ( pIfaceStat, mResultsParams.iface_stat,
                                        sizeof(wifi_iface_stat));
                                    free (mResultsParams.iface_stat);
                                    mResultsParams.iface_stat = pIfaceStat;
                                } else {
                                    ALOGE("%s: numPeers = %u, numRates= %u, "
                                          "either numPeers or numRates is invalid",
                                          __FUNCTION__,numPeers,numRates);
                                    status = WIFI_ERROR_UNKNOWN;
                                    free(pIfaceStat);
                                    goto cleanup;
                                }
                            }
                            wifi_peer_info *pPeerStats;
                            pIfaceStat->num_peers = numPeers;

                            if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO])
                            {
                                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO"
                                      " not found", __FUNCTION__);
                                status = WIFI_ERROR_INVALID_ARGS;
                                goto cleanup;
                            }
                            numRates = 0;
                            for (peerInfo = (struct nlattr *) nla_data(tb_vendor[
                                QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO]),
                                rem = nla_len(tb_vendor[
                                    QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO]);
                                    nla_ok(peerInfo, rem);
                                    peerInfo = nla_next(peerInfo, &(rem)))
                            {
                                struct nlattr *tb2[
                                    QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX+ 1];
                                pPeerStats = (wifi_peer_info *) (
                                               (u8 *)pIfaceStat->peer_info
                                               + (i++ * sizeof(wifi_peer_info))
                                               + (numRates * sizeof(wifi_rate_stat)));
                                nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX,
                                    (struct nlattr *) nla_data(peerInfo),
                                    nla_len(peerInfo), NULL);
                                status = get_wifi_peer_info(pPeerStats, tb2);
                                if(status != WIFI_SUCCESS)
                                {
                                    goto cleanup;
                                }
                                numRates += pPeerStats->num_rate;
                            }
                        }
                   }
                }
                break;

                case QCA_NL80211_VENDOR_SUBCMD_LL_STATS_TYPE_INVALID:
                default:
                    //error case should not happen print log
                    ALOGE("%s: Wrong LLStats subcmd received %d", __FUNCTION__,
                           mSubcmd);
            }
        }
        break;

        case QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR:
        {
            struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX + 1];
            nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX,
                    (struct nlattr *)mVendorData,
                    mDataLen, NULL);

            if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK])
            {
                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK not found", __FUNCTION__);
                return WIFI_ERROR_INVALID_ARGS;
            }
            ALOGI("Resp mask : %d\n", nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK]));

            if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP])
            {
                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP not found", __FUNCTION__);
                return WIFI_ERROR_INVALID_ARGS;
            }
            ALOGI("STOP resp : %d\n", nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP]));

            if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK])
            {
                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK not found", __FUNCTION__);
                return WIFI_ERROR_INVALID_ARGS;
            }
            mClearRspParams.stats_clear_rsp_mask = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK]);

            if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP])
            {
                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP not found", __FUNCTION__);
                return WIFI_ERROR_INVALID_ARGS;
            }
            mClearRspParams.stop_rsp = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP]);
            break;
        }

        case QCA_NL80211_VENDOR_SUBCMD_MLO_LINK_STATE:
        {
            struct nlattr *tb_link_vendor[QCA_WLAN_VENDOR_ATTR_LINK_STATE_MAX + 1];
            if (mResultsParams.iface_ml_stat) {
                nla_parse(tb_link_vendor, QCA_WLAN_VENDOR_ATTR_LINK_STATE_MAX,
                          (struct nlattr *)mVendorData,
                           mDataLen, NULL);

                if (!tb_link_vendor[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG])
                {
                    ALOGE("%s: Link state config information missing",
                           __FUNCTION__);
                    return WIFI_ERROR_INVALID_ARGS;
                }

                status = get_wifi_ml_iface_link_states(mResultsParams.iface_ml_stat,
                                                      tb_link_vendor);
                if(status != WIFI_SUCCESS)
                {
                    goto cleanup;
                }
            }
            break;
        }

        default :
            ALOGE("%s: Wrong LLStats subcmd received %d", __FUNCTION__, mSubcmd);
    }
    return NL_SKIP;

cleanup:
    clearStats();
    return status;
}

//Implementation of the functions exposed in linklayer.h
wifi_error wifi_set_link_stats(wifi_interface_handle iface,
                               wifi_link_layer_params params)
{
    wifi_error ret;
    LLStatsCommand *LLCommand;
    struct nlattr *nl_data;
    interface_info *iinfo = getIfaceInfo(iface);
    wifi_handle handle = getWifiHandle(iface);
    hal_info *info = getHalInfo(handle);

    if (!(info->supported_feature_set & WIFI_FEATURE_LINK_LAYER_STATS)) {
        ALOGI("%s: LLS is not supported by driver", __FUNCTION__);
        return WIFI_ERROR_NOT_SUPPORTED;
    }

    ALOGI("mpdu_size_threshold : %u, aggressive_statistics_gathering : %u",
          params.mpdu_size_threshold, params.aggressive_statistics_gathering);
    LLCommand = LLStatsCommand::instance(handle);
    if (LLCommand == NULL) {
        ALOGE("%s: Error LLStatsCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }
    LLCommand->setSubCmd(QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET);

    /* create the message */
    ret = LLCommand->create();
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = LLCommand->set_iface_id(iinfo->name);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /*add the attributes*/
    nl_data = LLCommand->attr_start(NL80211_ATTR_VENDOR_DATA);
    if (!nl_data){
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }
    /**/
    ret = LLCommand->put_u32(QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_MPDU_SIZE_THRESHOLD,
                                  params.mpdu_size_threshold);
    if (ret != WIFI_SUCCESS)
        goto cleanup;
    /**/
    ret = LLCommand->put_u32(
                QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_AGGRESSIVE_STATS_GATHERING,
                params.aggressive_statistics_gathering);
    if (ret != WIFI_SUCCESS)
        goto cleanup;
    LLCommand->attr_end(nl_data);

    ret = LLCommand->requestResponse();
    if (ret != WIFI_SUCCESS)
        ALOGE("%s: requestResponse Error:%d",__FUNCTION__, ret);

cleanup:
    return ret;
}

//Implementation of the functions exposed in LLStats.h
wifi_error wifi_get_link_stats(wifi_request_id id,
                               wifi_interface_handle iface,
                               wifi_stats_result_handler handler)
{
    wifi_error ret;
    LLStatsCommand *LLCommand;
    struct nlattr *nl_data;
    interface_info *iinfo = getIfaceInfo(iface);
    wifi_handle handle = getWifiHandle(iface);
    hal_info *info = getHalInfo(handle);

    if (!(info->supported_feature_set & WIFI_FEATURE_LINK_LAYER_STATS)) {
        ALOGI("%s: LLS is not supported by driver", __FUNCTION__);
        return WIFI_ERROR_NOT_SUPPORTED;
    }

    LLCommand = LLStatsCommand::instance(handle);
    if (LLCommand == NULL) {
        ALOGE("%s: Error LLStatsCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }
    LLCommand->setSubCmd(QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET);

    LLCommand->initGetContext(id);

    LLCommand->setHandler(handler);

    /* create the message */
    ret = LLCommand->create();
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = LLCommand->set_iface_id(iinfo->name);
    if (ret != WIFI_SUCCESS)
        goto cleanup;
    /*add the attributes*/
    nl_data = LLCommand->attr_start(NL80211_ATTR_VENDOR_DATA);
    if (!nl_data){
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }
    ret = LLCommand->put_u32(QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_ID,
                                  id);
    if (ret != WIFI_SUCCESS)
        goto cleanup;
    ret = LLCommand->put_u32(QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_MASK,
                                  7);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /**/
    LLCommand->attr_end(nl_data);

    ret = LLCommand->requestResponse();
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: requestResponse Error:%d",__FUNCTION__, ret);
        goto cleanup;
    }

    if (LLCommand->isMlo()) {
        LLCommand->setSubCmd(QCA_NL80211_VENDOR_SUBCMD_MLO_LINK_STATE);

        /* create the message */
        ret = LLCommand->create();
        if (ret != WIFI_SUCCESS)
            goto cleanup;

        ret = LLCommand->set_iface_id(iinfo->name);
        if (ret != WIFI_SUCCESS)
            goto cleanup;

        /*add the attributes*/
        nl_data = LLCommand->attr_start(NL80211_ATTR_VENDOR_DATA);
        if (!nl_data){
            ret = WIFI_ERROR_UNKNOWN;
            goto cleanup;
        }

        ret = LLCommand->put_u32(QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE,
                                 QCA_WLAN_VENDOR_LINK_STATE_OP_GET);
        if (ret != WIFI_SUCCESS)
            goto cleanup;
        /**/
        LLCommand->attr_end(nl_data);

        ret = LLCommand->requestResponse();
        if (ret != WIFI_SUCCESS) {
            ALOGE("%s: requestResponse Error while fetching ML link state: %d",
                  __FUNCTION__, ret);
        }

        ret = LLCommand->copyMloStats();
        if (ret != WIFI_SUCCESS)
            goto cleanup;
     }
     ret = LLCommand->notifyResponse();

cleanup:
    LLCommand->clearStats();
    return ret;
}


//Implementation of the functions exposed in LLStats.h
wifi_error wifi_clear_link_stats(wifi_interface_handle iface,
                                 u32 stats_clear_req_mask,
                                 u32 *stats_clear_rsp_mask,
                                 u8 stop_req, u8 *stop_rsp)
{
    wifi_error ret;
    LLStatsCommand *LLCommand;
    struct nlattr *nl_data;
    interface_info *iinfo = getIfaceInfo(iface);
    wifi_handle handle = getWifiHandle(iface);
    hal_info *info = getHalInfo(handle);

    if (!(info->supported_feature_set & WIFI_FEATURE_LINK_LAYER_STATS)) {
        ALOGI("%s: LLS is not supported by driver", __FUNCTION__);
        return WIFI_ERROR_NOT_SUPPORTED;
    }

    ALOGI("clear_req : %x, stop_req : %u", stats_clear_req_mask, stop_req);
    LLCommand = LLStatsCommand::instance(handle);
    if (LLCommand == NULL) {
        ALOGE("%s: Error LLStatsCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }
    LLCommand->setSubCmd(QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR);

    /* create the message */
    ret = LLCommand->create();
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = LLCommand->set_iface_id(iinfo->name);
    if (ret != WIFI_SUCCESS)
        goto cleanup;
    /*add the attributes*/
    nl_data = LLCommand->attr_start(NL80211_ATTR_VENDOR_DATA);
    if (!nl_data){
        ret = WIFI_ERROR_UNKNOWN;
        goto cleanup;
    }
    /**/
    ret = LLCommand->put_u32(QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_REQ_MASK,
                                  stats_clear_req_mask);
    if (ret != WIFI_SUCCESS)
        goto cleanup;
    /**/
    ret = LLCommand->put_u8(QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_REQ,
                                   stop_req);
    if (ret != WIFI_SUCCESS)
        goto cleanup;
    LLCommand->attr_end(nl_data);

    ret = LLCommand->requestResponse();
    if (ret != WIFI_SUCCESS)
        ALOGE("%s: requestResponse Error:%d",__FUNCTION__, ret);

    LLCommand->getClearRspParams(stats_clear_rsp_mask, stop_rsp);

cleanup:
    delete LLCommand;
    return ret;
}
