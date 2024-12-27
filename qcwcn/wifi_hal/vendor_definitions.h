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

#ifndef __VENDOR_DEFINITIONS_H__
#define __VENDOR_DEFINITIONS_H__

#include "qca-vendor_copy.h"

/*
 * @QCA_WLAN_VENDOR_ATTR_FEATURE_SET_INVALID: Invalid initial value
 * @QCA_WLAN_VENDOR_ATTR_FEATURE_SET: Unsigned 32bit value
 * @QCA_WLAN_VENDOR_ATTR_FEATURE_SET_EXT: Byte array value. The flags are
 * identified by their bit index (refer WIFI_FEATURE_* defines) with the first
 * byte being the least significant one and the last one being the most
 * significant one.
 * @QCA_WLAN_VENDOR_ATTR_FEATURE_SET_AFTER_LAST: After last
 * @QCA_WLAN_VENDOR_ATTR_FEATURE_SET_MAX: Max value
 */
enum qca_wlan_vendor_attr_get_supported_features
{
    QCA_WLAN_VENDOR_ATTR_FEATURE_SET_INVALID = 0,
    /* Unsigned 32-bit value */
    QCA_WLAN_VENDOR_ATTR_FEATURE_SET = 1,
    QCA_WLAN_VENDOR_ATTR_FEATURE_SET_EXT = 2,
    /* keep last */
    QCA_WLAN_VENDOR_ATTR_FEATURE_SET_AFTER_LAST,
    QCA_WLAN_VENDOR_ATTR_FEATURE_SET_MAX =
        QCA_WLAN_VENDOR_ATTR_FEATURE_SET_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_set_scanning_mac_oui
{
    QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI_INVALID = 0,
    /* An array of 3 x Unsigned 8-bit value */
    QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI = 1,
    /* keep last */
    QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI_AFTER_LAST,
    QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI_MAX =
        QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_set_no_dfs_flag
{
    QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_INVALID = 0,
    /* Unsigned 32-bit value */
    QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG = 1,
    /* keep last */
    QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_AFTER_LAST,
    QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_MAX =
        QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_AFTER_LAST - 1,
};

/* NL attributes for data used by
 * QCA_NL80211_VENDOR_SUBCMD_GET_CONCURRENCY_MATRIX sub command.
 */
enum qca_wlan_vendor_attr_get_concurrency_matrix
{
    QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_INVALID = 0,
    /* Unsigned 32-bit value */
    QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_CONFIG_PARAM_SET_SIZE_MAX = 1,
    /* Unsigned 32-bit value */
    QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_RESULTS_SET_SIZE = 2,
    /* An array of SET_SIZE x Unsigned 32bit values representing
     * concurrency combinations.
     */
    QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_RESULTS_SET = 3,
    /* keep last */
    QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_AFTER_LAST,
    QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_MAX =
        QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_AFTER_LAST - 1,
};

/* These are not used currently but we might need these in future */
enum qca_wlan_epno_type
{
    QCA_WLAN_EPNO,
    QCA_WLAN_PNO
};

enum qca_wlan_vendor_attr_ndp_cfg_security
{
   /* Security info will be added when proposed in the specification */
   QCA_WLAN_VENDOR_ATTR_NDP_SECURITY_TYPE = 1,

};
#endif
