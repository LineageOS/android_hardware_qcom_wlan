/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc.All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __VENDOR_NAN_HAL_H__
#define __VENDOR_NAN_HAL_H__

#include <stdint.h>
#include <functional>

typedef enum {
    VENDOR_NAN_SUCCESS                         = 0,
    VENDOR_NAN_ERROR_NONE                      = 0,
    VENDOR_NAN_ERROR_UNKNOWN                   = -1,
    VENDOR_NAN_ERROR_FAILURE                   = -2,
    VENDOR_NAN_ERROR_INVALID_ARGS              = -3,
    VENDOR_NAN_ERROR_OUT_OF_MEMORY             = -4
} vendor_nan_error;

/* typedefs */
typedef unsigned char byte;
typedef unsigned char u8;
typedef signed char s8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;
typedef u16 transaction_id;

/* forward declarations */
struct wifi_info;
struct wifi_interface_info;
typedef struct wifi_info *wifi_handle;

/***************************************************
 * Framework and Wi-Fi HAL interface
 ***************************************************/

/*
 Set command with data as byte array
*/
typedef struct {
     /* length of command data */
    u16 cmd_len;

    /* command data */
    u8 *cmd;
} NanVendorCmdData;

/*
  NAN Vendor Response messages
*/
typedef struct {
     /* length of response data */
    u16 rsp_len;

    /* response data */
    u8 *rsp_data;
} NanVendorResponseMsg;

/*
 Event indication with data as byte array
*/
typedef struct {
     /* length of event data */
    u16 event_len;

    /* event data */
    u8 *event_data;
} NanVendorEventInd;

// NAN vendor response and event callbacks struct.
struct VendorNanCallbackHandlers {
    // Notify Vendor Response invoked to notify the status of the Request.
    std::function<void(transaction_id, const NanVendorResponseMsg&)> on_notify_nan_cmd_response;
    // Notify Vendor event callback.
    std::function<void(const NanVendorEventInd&)> on_notify_nan_event_ind;
};

/* Response and Event Callbacks for vendor Aidl */
typedef struct {
    /* NotifyVendorResponse invoked to notify the status of the Request */
    void (*NotifyVendorResponse)(transaction_id id, NanVendorResponseMsg* vendor_rsp_data);
    /* Callbacks for various Vendor Events */
    void (*VendorEventIndication) (NanVendorEventInd* event);
} VendorNanCallbackHandler;

//nan vendor aidl function pointer table
typedef struct {
    vendor_nan_error (*wifi_nan_register_vendor_handler)(wifi_handle handle,
                      VendorNanCallbackHandler handlers);
    /**@brief wifi_nan_set_command
     * Set NAN command with command data as byte array.
     * @param transaction_id: NAN transaction id
     * @param wifi_handle
     * @param NanVendorCmdData request message
     * @return Synchronous vendor_nan_error
     */
    vendor_nan_error (*wifi_nan_set_command)(transaction_id id,
                      wifi_handle handle, NanVendorCmdData* msg);

} vendor_nan_fn;

/* Register NAN vendor callbacks. */
vendor_nan_error nan_register_vendor_handler(wifi_handle handle,
                                             VendorNanCallbackHandler handlers);

/**@brief nan_set_command
 *         Set NAN command with command data as byte array.
 *
 * @param transaction_id:
 * @param wifi_handle:
 * @param NanVendorCmdData:
 * @return Synchronous vendor_nan_error
 * @return Asynchronous NotifyResponse CB
 */
vendor_nan_error nan_set_command(transaction_id id, wifi_handle handle,
                                 NanVendorCmdData* msg);


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */
vendor_nan_error init_nan_vendor_aidl_hal_func_table(vendor_nan_fn *fn);
typedef vendor_nan_error (*init_nan_vendor_aidl_hal_func_table_t)(vendor_nan_fn *fn);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VENDOR_NAN_HAL_H__ */
