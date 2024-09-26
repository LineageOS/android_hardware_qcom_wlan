/* Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __WIFI_CACHED_SCAN_RESULT_H__
#define __WIFI_CACHED_SCAN_RESULT_H__

#include <errno.h>
#include "common.h"
#include "cpp_bindings.h"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */
wifi_error wifi_get_cached_scan_results(wifi_interface_handle iface,
                                  wifi_cached_scan_result_handler handler);

class WifiScanResult: public WifiVendorCommand
{
private:
	wifi_cached_scan_result_handler mHandler;
	wifi_cached_scan_report *scanReport;

public:
	WifiScanResult(wifi_handle handle, int id, u32 vendor_id, u32 subcmd);
	virtual ~WifiScanResult();


	virtual wifi_error requestResponse();

	virtual int handleResponse(WifiEvent & reply);

	virtual void setHandler(wifi_cached_scan_result_handler handler);

	virtual void setSubCmd(u32 subcmd);

	virtual wifi_error notifyResponse();

	void clearReport();
};
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
