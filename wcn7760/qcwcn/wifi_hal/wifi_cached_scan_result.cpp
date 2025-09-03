/* Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "sync.h"

#define LOG_TAG  "WifiHAL"

#include <utils/Log.h>

#include <hardware_legacy/wifi_hal.h>
#include "common.h"
#include "cpp_bindings.h"
#include "wifi_cached_scan_result.h"
#include <inttypes.h>

WifiScanResult::WifiScanResult(wifi_handle handle, int id, u32 vendor_id,
                               u32 subcmd)
              : WifiVendorCommand(handle, id, vendor_id, subcmd)
{
    memset(&mHandler, 0, sizeof(mHandler));
    scanReport = NULL;
}

WifiScanResult::~WifiScanResult()
{
}

void WifiScanResult::setHandler(wifi_cached_scan_result_handler handler)
{
    mHandler = handler;
}

void WifiScanResult::setSubCmd(u32 subcmd)
{
    mSubcmd = subcmd;
}

wifi_error WifiScanResult::requestResponse()
{
    return WifiCommand::requestResponse(mMsg);
}

/* Sets wifi cached scan result attribute */
wifi_error  wifi_get_cached_scan_results(wifi_interface_handle iface,
                                  wifi_cached_scan_result_handler handler)
{

    wifi_error ret = WIFI_ERROR_NONE;
    int requestId;
    struct nlattr *nl_data;
    WifiScanResult *wifiScanResult;

    if (handler.on_cached_scan_results == NULL) {
        ALOGE("On cached scan results is null");
        return WIFI_ERROR_UNKNOWN;
    }

    interface_info *ifaceInfo = getIfaceInfo(iface);
    wifi_handle wifihandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifihandle);

    if (!wifihandle) {
        ALOGE("%s: Error wifi_handle NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    if (!ifaceInfo) {
        ALOGE("%s: Error ifaceInfo NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    if (!(info->supported_feature_set & WIFI_FEATURE_CACHED_SCAN_RESULTS)){
        ALOGE("%s: Wifi Cached Scan Result not supported by driver",
           __FUNCTION__);
        return WIFI_ERROR_NOT_SUPPORTED;
    }

    ALOGV("%s: Interface Name :%s", __FUNCTION__, ifaceInfo->name);

   /* Get randomly generated request id*/
   requestId = get_requestid();

   wifiScanResult = new WifiScanResult(
                         wifihandle,
                         requestId,
                         OUI_QCA,
                         QCA_NL80211_VENDOR_SUBCMD_GET_FW_SCAN_REPORT);

    if (wifiScanResult == NULL) {
        ALOGE("%s: Error wifiScanResult is NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    wifiScanResult->setHandler(handler);
    wifiScanResult->setSubCmd(QCA_NL80211_VENDOR_SUBCMD_GET_FW_SCAN_REPORT);

    /* Create the NL message. */
    ret = wifiScanResult->create();
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: Failed create, Error:%d", __FUNCTION__, ret);
        goto cleanup;
    }

    /* Set the interface Id of the message. */
    ret = wifiScanResult->set_iface_id(ifaceInfo->name);
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: Failed to set iface id Error:%d", __FUNCTION__, ret);
        goto cleanup;
    }

    ret = wifiScanResult->requestResponse();
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: Failed to send wifi cached scan request, Error:%d",
             __FUNCTION__, ret);
        goto cleanup;
    }

    ret = wifiScanResult->notifyResponse();
    if (ret != WIFI_SUCCESS)
        ALOGE("%s: Failed in NotifyResponse : %d", __FUNCTION__, ret);

cleanup:
    wifiScanResult->clearReport();
    delete wifiScanResult;
    return ret;
}

/* check bit is set or not in flag */
static int flag_isset( u8 *features, int features_len,
                       qca_wlan_fw_scan_bss_flags index)
{
    u8 set_byte;

    if ((int) index / 8 >= features_len)
        return 0;

    set_byte = features[index / 8];
    return (set_byte & BIT(index % 8)) != 0;
}

/* parse wifi cached scan result per BSS List */
static wifi_error parse_wifi_cached_scan_result(wifi_cached_scan_report *report,
                                            struct nlattr **tb_vendor)
{
    struct nlattr *scanInfo = NULL;
    wifi_cached_scan_result *scan_result = NULL;
    int rem = 0;
    wifi_error status = WIFI_ERROR_NONE;
    u8 result_cnt = 0;

    for (scanInfo = (struct nlattr *) nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_FW_SCAN_REPORT_BSS_LIST]),
         rem = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_FW_SCAN_REPORT_BSS_LIST]);
         nla_ok(scanInfo, rem);
         scanInfo = nla_next(scanInfo, &(rem))) {
         result_cnt += 1;
    }

    scan_result = (wifi_cached_scan_result *)malloc(
                                result_cnt * sizeof(wifi_cached_scan_result));
    if (scan_result == NULL) {
            ALOGE("%s: Scan Result malloc failed", __FUNCTION__);
            return WIFI_ERROR_OUT_OF_MEMORY;
    }
    memset(scan_result, 0, result_cnt * sizeof(wifi_cached_scan_result));

    rem = 0;
    result_cnt = 0;
    scanInfo = NULL;

    for (scanInfo = (struct nlattr *) nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_FW_SCAN_REPORT_BSS_LIST]),
         rem = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_FW_SCAN_REPORT_BSS_LIST]);
         nla_ok(scanInfo, rem);
         scanInfo = nla_next(scanInfo, &(rem)))
    {
        int len = 0;
        u8 *feature_flag;
        struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_MAX + 1];
        nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_MAX, (struct nlattr *) nla_data(scanInfo),
                    nla_len(scanInfo), NULL);

        /* Age_Ms */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_MS_AGO])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_MS_AGO not found",
                    __FUNCTION__);
            status = WIFI_ERROR_INVALID_ARGS;
            goto cleanup;
        }
        scan_result[result_cnt].age_ms = nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_MS_AGO]);

        /* BSSID */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_BSSID])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_BSSID not found",
                    __FUNCTION__);
            status = WIFI_ERROR_INVALID_ARGS;
            goto cleanup;
        }
        len = nla_len(tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_BSSID]);
        len = ((sizeof(scan_result[result_cnt].bssid) <= len) ? sizeof(scan_result[result_cnt].bssid) : len);
        memcpy(&scan_result[result_cnt].bssid, nla_data(tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_BSSID]),
                len);

        /* SSID */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_SSID])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_SSID not found", __FUNCTION__);
            status = WIFI_ERROR_INVALID_ARGS;
            goto cleanup;
        }
        len = nla_len(tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_SSID]);
        len = ((sizeof(scan_result[result_cnt].ssid) <= len) ? sizeof(scan_result[result_cnt].ssid) : len);

        scan_result[result_cnt].ssid_len = (u8)len;
        memcpy(&scan_result[result_cnt].ssid, nla_data(tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_SSID]),
              len);

        /* RSSI */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_RSSI])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_RSSI not found", __FUNCTION__);
            status = WIFI_ERROR_INVALID_ARGS;
            goto cleanup;
        }
        scan_result[result_cnt].rssi = nla_get_s8(tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_RSSI]);

        /* Capability */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_CAPABILITY])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_CAPABILITY not found", __FUNCTION__);
            status = WIFI_ERROR_INVALID_ARGS;
            goto cleanup;
        }
        scan_result[result_cnt].capability = nla_get_u16(tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_CAPABILITY]);

        /* Flags */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_FLAGS])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_FLAGS not found", __FUNCTION__);
            status = WIFI_ERROR_INVALID_ARGS;
            goto cleanup;
        }
        feature_flag = (u8*)nla_data(tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_FLAGS]);
        len = nla_len(tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_FLAGS]);
        if (flag_isset(feature_flag, len, QCA_WLAN_FW_SCAN_BSS_HT_OPS))
            scan_result[result_cnt].flags |= WIFI_CACHED_SCAN_RESULT_FLAGS_HT_OPS_PRESENT;

        if (flag_isset(feature_flag, len, QCA_WLAN_FW_SCAN_BSS_VHT_OPS))
            scan_result[result_cnt].flags |= WIFI_CACHED_SCAN_RESULT_FLAGS_VHT_OPS_PRESENT;

        if (flag_isset(feature_flag, len, QCA_WLAN_FW_SCAN_BSS_HE_OPS))
            scan_result[result_cnt].flags |= WIFI_CACHED_SCAN_RESULT_FLAGS_HE_OPS_PRESENT;

        if (flag_isset(feature_flag, len, QCA_WLAN_FW_SCAN_BSS_EHT_OPS))
            scan_result[result_cnt].flags |= WIFI_CACHED_SCAN_RESULT_FLAGS_EHT_OPS_PRESENT;

        if (flag_isset(feature_flag, len, QCA_WLAN_FW_SCAN_BSS_FTM_RESPONDER))
            scan_result[result_cnt].flags |= WIFI_CACHED_SCAN_RESULT_FLAGS_IS_FTM_RESPONDER;

        /* primary frequency */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_PRIMARY_FREQ])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_PRIMARY_FREQ not found", __FUNCTION__);
            status = WIFI_ERROR_INVALID_ARGS;
            goto cleanup;
        }
        scan_result[result_cnt].chanspec.primary_frequency = (int)nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_PRIMARY_FREQ]);

        /* channel width */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_CHAN_WIDTH])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_CHAN_WIDTH not found", __FUNCTION__);
            status = WIFI_ERROR_INVALID_ARGS;
            goto cleanup;
        }
        scan_result[result_cnt].chanspec.width = (wifi_channel_width)nla_get_u8(tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_CHAN_WIDTH]);

        /* center frequency 1 */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_CENTER_FREQ1])
        {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_CENTER_FREQ1 not found", __FUNCTION__);
            status =  WIFI_ERROR_INVALID_ARGS;
            goto cleanup;
        }
        scan_result[result_cnt].chanspec.center_frequency0 = (int)nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_CENTER_FREQ1]);

        /* center frequency 2 */
        if (tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_CENTER_FREQ2])
        {
            scan_result[result_cnt].chanspec.center_frequency1 = (int)nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_FW_SCAN_BSS_CENTER_FREQ2]);
        } else {
            scan_result[result_cnt].chanspec.center_frequency1 = 0;
        }

        ALOGV("Age_Ms: %" PRIu32 "\n"
              "BSSID: "
               MAC_ADDR_STR
              "\nSSID: %s\n"
              "SSID_Len: %u\n"
              "RSSI : %d\n"
              "Capability : %u\n"
              "Flags: %x\n"
              "Primary_frequency: %d\n"
              "Width: %u\n"
              "Center_frequency0: %d\n"
              "Center_frequency1: %d",
              scan_result[result_cnt].age_ms,
              MAC_ADDR_ARRAY(scan_result[result_cnt].bssid),
              scan_result[result_cnt].ssid,
              scan_result[result_cnt].ssid_len,
              scan_result[result_cnt].rssi,
              scan_result[result_cnt].capability,
              scan_result[result_cnt].flags,
              scan_result[result_cnt].chanspec.primary_frequency,
              scan_result[result_cnt].chanspec.width,
              scan_result[result_cnt].chanspec.center_frequency0 ,
              scan_result[result_cnt].chanspec.center_frequency1);

        result_cnt += 1;
    }

    report->result_cnt = result_cnt;
    ALOGV("Result Count: %d", report->result_cnt);
    report->results = scan_result;
    return WIFI_SUCCESS;

cleanup:
    if (scan_result) {
        free(scan_result);
    }
    return status;
}

int WifiScanResult::handleResponse(WifiEvent &reply)
{
    // Parse the vendor data
    struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_FW_SCAN_REPORT_MAX+1];
    int status = WIFI_ERROR_NONE;
    int len = 0;

    WifiVendorCommand::handleResponse(reply);

    nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_FW_SCAN_REPORT_MAX,
                (struct nlattr *)mVendorData,
                mDataLen, NULL);

    scanReport = (wifi_cached_scan_report *)
                    malloc(sizeof(wifi_cached_scan_report));
    if (scanReport == NULL) {
        ALOGE("%s: Wifi Scan Report malloc failed", __FUNCTION__);
        status = WIFI_ERROR_OUT_OF_MEMORY;
        goto cleanup;
    }
    memset(scanReport, 0, sizeof(wifi_cached_scan_report));

    /* Timestamp */
    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_FW_SCAN_REPORT_TIMESTAMP]) {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_FW_SCAN_REPORT_TIMESTAMP not found",
                __FUNCTION__);
        status = WIFI_ERROR_INVALID_ARGS;
        goto cleanup;
    }
    scanReport->ts = nla_get_u64(tb_vendor[QCA_WLAN_VENDOR_ATTR_FW_SCAN_REPORT_TIMESTAMP]);
    ALOGV("Scan Report Timestamp:%" PRId64, scanReport->ts);

    /* Frequency List */
    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_FW_SCAN_REPORT_FREQ_LIST]) {
        ALOGV("%s: No or All Channels Were Scanned", __FUNCTION__);
        scanReport->scanned_freq_list = NULL;
        scanReport->scanned_freq_num = 0;
    } else {
            int rem = 0;
            u16 scanned_freq_num = 0;
            struct nlattr *scanInfo = NULL;
            u32 *freq_list = NULL;

            nla_for_each_nested(scanInfo,
                                tb_vendor[QCA_WLAN_VENDOR_ATTR_FW_SCAN_REPORT_FREQ_LIST],
                                rem) {
                                scanned_freq_num += 1;
            }
            ALOGV("Scanned Freq Num : %u", scanned_freq_num);

            freq_list = (u32 * )malloc(
                                scanned_freq_num * sizeof(u32));
            if (freq_list == NULL) {
                ALOGE("%s: Freq List malloc failed", __FUNCTION__);
                status = WIFI_ERROR_OUT_OF_MEMORY;
                goto cleanup;
            }

            scanInfo = NULL;
            rem = 0;
            scanned_freq_num = 0;
            nla_for_each_nested(scanInfo,
                                tb_vendor[QCA_WLAN_VENDOR_ATTR_FW_SCAN_REPORT_FREQ_LIST],
                                rem) {
                freq_list[scanned_freq_num] = nla_get_u32(scanInfo);
                ALOGV("Scan Freq List : %" PRIu32, freq_list[scanned_freq_num]);
                scanned_freq_num += 1;
            }

            scanReport->scanned_freq_list = freq_list;
            scanReport->scanned_freq_num = scanned_freq_num;
    }

    /* check for BSS List */
    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_FW_SCAN_REPORT_BSS_LIST]) {
        ALOGV("%s: No BSS List Found", __FUNCTION__);
        scanReport->results = NULL;
        scanReport->result_cnt = 0;
    } else {
        status = parse_wifi_cached_scan_result(scanReport, tb_vendor);
        if (status != WIFI_SUCCESS)
            goto cleanup;
    }
    return status;

cleanup:
    clearReport();
    return status;
}

wifi_error WifiScanResult::notifyResponse()
{
    wifi_error ret = WIFI_ERROR_UNKNOWN;
    if (scanReport) {
        mHandler.on_cached_scan_results(scanReport);
        ret = WIFI_SUCCESS;
    }
    return ret;
}

void WifiScanResult::clearReport()
{

    if (scanReport) {
        if (scanReport->scanned_freq_list) {
            free((void*)scanReport->scanned_freq_list);
            scanReport->scanned_freq_list = NULL;
        }

        if (scanReport->results) {
            free((void *)scanReport->results);
            scanReport->results = NULL;
        }

        free(scanReport);
        scanReport = NULL;
    }
}
