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
 *
 * Changes from Qualcomm Innovation Center are provided under the following license:
 *
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the
 * disclaimer below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *
 *   * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
 * GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
 * HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __NAN_I_H__
#define __NAN_I_H__

#include "common.h"
#include "cpp_bindings.h"
#include <hardware_legacy/wifi_hal.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifdef WPA_PASN_LIB
#include "utils/os.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "common/defs.h"
#include "common/wpa_common.h"
#include "common/sae.h"
#include "common/ieee802_11_common.h"
#include "common/ptksa_cache.h"
#include "crypto/sha256.h"
#include "crypto/sha384.h"
#include "crypto/aes_wrap.h"
#include "eap_peer/eap_config.h"
#include "crypto/random.h"
#include "pasn/pasn_common.h"
#endif

#ifndef PACKED
#define PACKED  __attribute__((packed))
#endif
#define BIT_NONE            0x00
#define BIT_0               0x01
#define BIT_1               0x02
#define BIT_2               0x04
#define BIT_3               0x08
#define BIT_4               0x10
#define BIT_5               0x20
#define BIT_6               0x40
#define BIT_7               0x80
#define BIT_8               0x0100
#define BIT_9               0x0200
#define BIT_10              0x0400
#define BIT_11              0x0800
#define BIT_12              0x1000
#define BIT_13              0x2000
#define BIT_14              0x4000
#define BIT_15              0x8000
#define BIT_16              0x010000
#define BIT_17              0x020000
#define BIT_18              0x040000
#define BIT_19              0x080000
#define BIT_20              0x100000
#define BIT_21              0x200000
#define BIT_22              0x400000
#define BIT_23              0x800000
#define BIT_24              0x01000000
#define BIT_25              0x02000000
#define BIT_26              0x04000000
#define BIT_27              0x08000000
#define BIT_28              0x10000000
#define BIT_29              0x20000000
#define BIT_30              0x40000000
#define BIT_31              0x80000000

#define NAN_F_MS(_v, _f)                                            \
            ( ((_v) & (_f)) >> (_f##_S) )

#define NAN_MAX_GROUP_KEY_LEN 32
#define NAN_MAX_GROUP_KEY_RSC_LEN 6

#define NAN_CSIA_GRPKEY_SUPPORT_S 1
#define NAN_CSIA_GRPKEY_SUPPORT (0x2 << NAN_CSIA_GRPKEY_SUPPORT_S)
#define NAN_CSIA_GRPKEY_SUPPORT_GET(x) NAN_F_MS(x,NAN_CSIA_GRPKEY_SUPPORT)

#define NAN_CSIA_GRPKEY_LEN_S 4
#define NAN_CSIA_GRPKEY_LEN (0x1 << NAN_CSIA_GRPKEY_LEN_S)
#define NAN_CSIA_GRPKEY_LEN_GET(x) NAN_F_MS(x,NAN_CSIA_GRPKEY_LEN)
#define NAN_CSIA_GRPKEY_LEN_16   16
#define NAN_CSIA_GRPKEY_LEN_32   32
#define NAN_MAX_SD_ATTRS_PER_FRAME 20
#define NAN_SD_ATTR_SERVICE_ID_LEN  6
#define NAN_SDF_MAX_LEN 750
#define NAN_SD_ATTR_MAX_LEN         \
    (NAN_SDF_MAX_LEN -              \
     24 - /* MAC Header */          \
      6)  /* PAF header */
#define NAN_SD_ATTR_MIN_LEN         \
    (1 + /* Attribute ID     */     \
     2 + /* Attribute Length */     \
     NAN_SD_ATTR_SERVICE_ID_LEN +   \
     1 + /* Instance ID      */     \
     1 + /* Requestor ID     */     \
     1)  /* Service Control  */
#define NAN_SDE_ATTR_MIN_LEN 3
#define NAN_SDE_ATTR_SERVICE_INFO_HEADER_LEN    \
    (3 + /* OUI */                              \
     1 /* Service Protocol Type */ )

/* NAN TLV Maximum Lengths */
#define NAN_MAX_EXT_SERVICE_SPECIFIC_INFO_LEN 270
#define NAN_FOLLOWUP_MAX_EXT_SERVICE_SPECIFIC_INFO_LEN 1400
#define NAN_MAX_BOOTSTRAPPING_COOKIE_LEN 255
#define NAN_MAX_SHARED_KEY_DESC_ATTR_LEN 256
#define NAN_MAX_FOLLOWUP_IND_SIZE                                    \
    (                                                                \
        sizeof(NanMsgHeader)                                     +   \
        sizeof(NanFollowupIndParams)                             +   \
        SIZEOF_TLV_HDR + (sizeof(u8) * NAN_MAC_ADDR_LEN)         +   \
        SIZEOF_TLV_HDR + NAN_MAX_SERVICE_SPECIFIC_INFO_LEN           \
    )
#define NAN_MAX_FOLLOWUP_IND_SIZE_EXT_SSI                                \
    (                                                                    \
        sizeof(NanMsgHeader)                                     +       \
        sizeof(NanFollowupIndParams)                             +       \
        SIZEOF_TLV_HDR + (sizeof(u8) * NAN_MAC_ADDR_LEN)         +       \
        SIZEOF_TLV_HDR + NAN_FOLLOWUP_MAX_EXT_SERVICE_SPECIFIC_INFO_LEN  \
    )

/* Service Descriptor Attribute Constants */

/* Service Control Flags */
#define NAN_SVC_CTRL_FLAG_MATCH_FILTER          0x04
#define NAN_SVC_CTRL_FLAG_SERVICE_RSP           0x08
#define NAN_SVC_CTRL_FLAG_SERVICE_INFO          0x10
#define NAN_SVC_CTRL_FLAG_BINDING_BITMAP        0x40

#define NAN_SDE_ATTR_LEN_OFFSET 1
#define NAN_SDE_ATTR_CTRL_RANGE_LIMIT_OFFSET 8
#define NAN_SDE_ATTR_CTRL_SERVICE_UPDATE_INDI_PRESENT 9

#define NAN_NPBA_ATTR_MIN_LEN       \
    (1 + /* Attribute ID     */     \
     2 + /* Attribute Length */     \
     1 + /* Dialog Token     */     \
     1 + /* Type and Status  */     \
     1 + /* Reason code      */     \
     2 ) /* Pairing Bootstrapping Method */

/** macro to convert FW MAC address from WMI word format to User Space MAC char array */
#define FW_MAC_ADDR_TO_CHAR_ARRAY(fw_mac_addr, mac_addr) do { \
     (mac_addr)[0] =    ((fw_mac_addr).mac_addr31to0) & 0xff; \
     (mac_addr)[1] =  (((fw_mac_addr).mac_addr31to0) >> 8) & 0xff; \
     (mac_addr)[2] =  (((fw_mac_addr).mac_addr31to0) >> 16) & 0xff; \
     (mac_addr)[3] =  (((fw_mac_addr).mac_addr31to0) >> 24) & 0xff; \
     (mac_addr)[4] =    ((fw_mac_addr).mac_addr47to32) & 0xff; \
     (mac_addr)[5] =  (((fw_mac_addr).mac_addr47to32) >> 8) & 0xff; \
} while (0)

/** macro to convert User space MAC address from char array to FW WMI word format */
#define CHAR_ARRAY_TO_MAC_ADDR(mac_addr, fw_mac_addr)  do { \
    (fw_mac_addr).mac_addr31to0  =                   \
         ((mac_addr)[0] | ((mac_addr)[1] << 8)            \
           | ((mac_addr)[2] << 16) | ((mac_addr)[3] << 24));          \
    (fw_mac_addr).mac_addr47to32  =                  \
         ((mac_addr)[4] | ((mac_addr)[5] << 8));          \
} while (0)

#ifndef WPA_PASN_LIB
/* Macros for handling unaligned memory accesses */

static inline u16 WPA_GET_BE16(const u8 *a)
{
	return (a[0] << 8) | a[1];
}

static inline void WPA_PUT_BE16(u8 *a, u16 val)
{
	a[0] = val >> 8;
	a[1] = val & 0xff;
}

static inline u16 WPA_GET_LE16(const u8 *a)
{
	return (a[1] << 8) | a[0];
}

static inline void WPA_PUT_LE16(u8 *a, u16 val)
{
	a[1] = val >> 8;
	a[0] = val & 0xff;
}

static inline u32 WPA_GET_BE24(const u8 *a)
{
	return (a[0] << 16) | (a[1] << 8) | a[2];
}

static inline void WPA_PUT_BE24(u8 *a, u32 val)
{
	a[0] = (val >> 16) & 0xff;
	a[1] = (val >> 8) & 0xff;
	a[2] = val & 0xff;
}

static inline u32 WPA_GET_LE24(const u8 *a)
{
	return (a[2] << 16) | (a[1] << 8) | a[0];
}

static inline void WPA_PUT_LE24(u8 *a, u32 val)
{
	a[2] = (val >> 16) & 0xff;
	a[1] = (val >> 8) & 0xff;
	a[0] = val & 0xff;
}

static inline u32 WPA_GET_BE32(const u8 *a)
{
	return ((u32) a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
}

static inline void WPA_PUT_BE32(u8 *a, u32 val)
{
	a[0] = (val >> 24) & 0xff;
	a[1] = (val >> 16) & 0xff;
	a[2] = (val >> 8) & 0xff;
	a[3] = val & 0xff;
}

static inline u32 WPA_GET_LE32(const u8 *a)
{
	return ((u32) a[3] << 24) | (a[2] << 16) | (a[1] << 8) | a[0];
}

static inline void WPA_PUT_LE32(u8 *a, u32 val)
{
	a[3] = (val >> 24) & 0xff;
	a[2] = (val >> 16) & 0xff;
	a[1] = (val >> 8) & 0xff;
	a[0] = val & 0xff;
}

static inline u64 WPA_GET_BE64(const u8 *a)
{
	return (((u64) a[0]) << 56) | (((u64) a[1]) << 48) |
		(((u64) a[2]) << 40) | (((u64) a[3]) << 32) |
		(((u64) a[4]) << 24) | (((u64) a[5]) << 16) |
		(((u64) a[6]) << 8) | ((u64) a[7]);
}

static inline void WPA_PUT_BE64(u8 *a, u64 val)
{
	a[0] = val >> 56;
	a[1] = val >> 48;
	a[2] = val >> 40;
	a[3] = val >> 32;
	a[4] = val >> 24;
	a[5] = val >> 16;
	a[6] = val >> 8;
	a[7] = val & 0xff;
}

static inline u64 WPA_GET_LE64(const u8 *a)
{
	return (((u64) a[7]) << 56) | (((u64) a[6]) << 48) |
		(((u64) a[5]) << 40) | (((u64) a[4]) << 32) |
		(((u64) a[3]) << 24) | (((u64) a[2]) << 16) |
		(((u64) a[1]) << 8) | ((u64) a[0]);
}

static inline void WPA_PUT_LE64(u8 *a, u64 val)
{
	a[7] = val >> 56;
	a[6] = val >> 48;
	a[5] = val >> 40;
	a[4] = val >> 32;
	a[3] = val >> 24;
	a[2] = val >> 16;
	a[1] = val >> 8;
	a[0] = val & 0xff;
}

struct ieee80211_hdr {
	u16 frame_control;
	u16 duration_id;
	u8 addr1[6];
	u8 addr2[6];
	u8 addr3[6];
	u16 seq_ctrl;
	/* followed by 'u8 addr4[6];' if ToDS and FromDS is set in data frame
	 */
} PACKED;

static inline unsigned short wpa_swap_16(unsigned short v)
{
	return ((v & 0xff) << 8) | (v >> 8);
}

static inline unsigned int wpa_swap_32(unsigned int v)
{
	return ((v & 0xff) << 24) | ((v & 0xff00) << 8) |
		((v & 0xff0000) >> 8) | (v >> 24);
}

#define le_to_host16(n) (n)
#define host_to_le16(n) (n)
#define be_to_host16(n) wpa_swap_16(n)
#define host_to_be16(n) wpa_swap_16(n)
#define le_to_host32(n) (n)
#define host_to_le32(n) (n)
#define be_to_host32(n) wpa_swap_32(n)
#define host_to_be32(n) wpa_swap_32(n)
#define host_to_le64(n) (n)
#define WLAN_FC_GET_TYPE(fc)	(((fc) & 0x000c) >> 2)
#define WLAN_FC_GET_STYPE(fc)	(((fc) & 0x00f0) >> 4)
#define IEEE80211_HDRLEN (sizeof(struct ieee80211_hdr))

#define WLAN_FC_STYPE_AUTH		11
#define WLAN_FC_STYPE_ACTION		13

#define WLAN_PA_VENDOR_SPECIFIC 9

#define WLAN_FC_TYPE_MGMT  0
#define OUI_WFA 0x506f9a
#define WLAN_ACTION_PUBLIC 4
#define WLAN_ACTION_PROTECTED_DUAL 9

#endif
/*---------------------------------------------------------------------------
* WLAN NAN CONSTANTS
*--------------------------------------------------------------------------*/

typedef enum
{
    NAN_MSG_ID_ERROR_RSP                    = 0,
    NAN_MSG_ID_CONFIGURATION_REQ            = 1,
    NAN_MSG_ID_CONFIGURATION_RSP            = 2,
    NAN_MSG_ID_PUBLISH_SERVICE_REQ          = 3,
    NAN_MSG_ID_PUBLISH_SERVICE_RSP          = 4,
    NAN_MSG_ID_PUBLISH_SERVICE_CANCEL_REQ   = 5,
    NAN_MSG_ID_PUBLISH_SERVICE_CANCEL_RSP   = 6,
    NAN_MSG_ID_PUBLISH_REPLIED_IND          = 7,
    NAN_MSG_ID_PUBLISH_TERMINATED_IND       = 8,
    NAN_MSG_ID_SUBSCRIBE_SERVICE_REQ        = 9,
    NAN_MSG_ID_SUBSCRIBE_SERVICE_RSP        = 10,
    NAN_MSG_ID_SUBSCRIBE_SERVICE_CANCEL_REQ = 11,
    NAN_MSG_ID_SUBSCRIBE_SERVICE_CANCEL_RSP = 12,
    NAN_MSG_ID_MATCH_IND                    = 13,
    NAN_MSG_ID_MATCH_EXPIRED_IND            = 14,
    NAN_MSG_ID_SUBSCRIBE_TERMINATED_IND     = 15,
    NAN_MSG_ID_DE_EVENT_IND                 = 16,
    NAN_MSG_ID_TRANSMIT_FOLLOWUP_REQ        = 17,
    NAN_MSG_ID_TRANSMIT_FOLLOWUP_RSP        = 18,
    NAN_MSG_ID_FOLLOWUP_IND                 = 19,
    NAN_MSG_ID_STATS_REQ                    = 20,
    NAN_MSG_ID_STATS_RSP                    = 21,
    NAN_MSG_ID_ENABLE_REQ                   = 22,
    NAN_MSG_ID_ENABLE_RSP                   = 23,
    NAN_MSG_ID_DISABLE_REQ                  = 24,
    NAN_MSG_ID_DISABLE_RSP                  = 25,
    NAN_MSG_ID_DISABLE_IND                  = 26,
    NAN_MSG_ID_TCA_REQ                      = 27,
    NAN_MSG_ID_TCA_RSP                      = 28,
    NAN_MSG_ID_TCA_IND                      = 29,
    NAN_MSG_ID_BEACON_SDF_REQ               = 30,
    NAN_MSG_ID_BEACON_SDF_RSP               = 31,
    NAN_MSG_ID_BEACON_SDF_IND               = 32,
    NAN_MSG_ID_CAPABILITIES_REQ             = 33,
    NAN_MSG_ID_CAPABILITIES_RSP             = 34,
    NAN_MSG_ID_SELF_TRANSMIT_FOLLOWUP_IND   = 35,
    NAN_MSG_ID_RANGING_REQUEST_RECEVD_IND   = 36,
    NAN_MSG_ID_RANGING_RESULT_IND           = 37,
    NAN_MSG_ID_IDENTITY_RESOLUTION_IND      = 38,
    NAN_MSG_ID_PAIRING_IND                  = 39,
    NAN_MSG_ID_UNPAIRING_IND                = 40,
    NAN_MSG_ID_OEM_REQ                      = 41,
    NAN_MSG_ID_OEM_RSP                      = 42,
    NAN_MSG_ID_OEM_IND                      = 43,
    NAN_MSG_ID_GROUP_KEY_INSTALL_REQ        = 44,
    NAN_MSG_ID_GROUP_KEY_INSTALL_RSP        = 45,
    NAN_MSG_ID_GET_TX_PN_REQ                = 46,
    NAN_MSG_ID_GET_TX_PN_RSP                = 47,
    NAN_MSG_ID_TESTMODE_REQ                 = 1025,
    NAN_MSG_ID_TESTMODE_RSP                 = 1026
} NanMsgId;

/*
  Various TLV Type ID sent as part of NAN Stats Response
  or NAN TCA Indication
*/
typedef enum
{
    NAN_TLV_TYPE_FIRST = 0,

    /* Service Discovery Frame types */
    NAN_TLV_TYPE_SDF_FIRST = NAN_TLV_TYPE_FIRST,
    NAN_TLV_TYPE_SERVICE_NAME = NAN_TLV_TYPE_SDF_FIRST,
    NAN_TLV_TYPE_SDF_MATCH_FILTER,
    NAN_TLV_TYPE_TX_MATCH_FILTER,
    NAN_TLV_TYPE_RX_MATCH_FILTER,
    NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO,
    NAN_TLV_TYPE_EXT_SERVICE_SPECIFIC_INFO =5,
    NAN_TLV_TYPE_VENDOR_SPECIFIC_ATTRIBUTE_TRANSMIT = 6,
    NAN_TLV_TYPE_VENDOR_SPECIFIC_ATTRIBUTE_RECEIVE = 7,
    NAN_TLV_TYPE_POST_NAN_CONNECTIVITY_CAPABILITIES_RECEIVE = 8,
    NAN_TLV_TYPE_POST_NAN_DISCOVERY_ATTRIBUTE_RECEIVE = 9,
    NAN_TLV_TYPE_BEACON_SDF_PAYLOAD_RECEIVE = 10,
    NAN_TLV_TYPE_NAN_DATA_PATH_PARAMS = 11,
    NAN_TLV_TYPE_NAN_DATA_SUPPORTED_BAND = 12,
    NAN_TLV_TYPE_2G_COMMITTED_DW = 13,
    NAN_TLV_TYPE_5G_COMMITTED_DW = 14,
    NAN_TLV_TYPE_NAN_DATA_RESPONDER_MODE = 15,
    NAN_TLV_TYPE_NAN_DATA_ENABLED_IN_MATCH = 16,
    NAN_TLV_TYPE_NAN_SERVICE_ACCEPT_POLICY = 17,
    NAN_TLV_TYPE_NAN_CSID = 18,
    NAN_TLV_TYPE_NAN_SCID = 19,
    NAN_TLV_TYPE_NAN_PMK = 20,
    NAN_TLV_TYPE_SDEA_CTRL_PARAMS = 21,
    NAN_TLV_TYPE_NAN_RANGING_CFG = 22,
    NAN_TLV_TYPE_CONFIG_DISCOVERY_INDICATIONS = 23,
    NAN_TLV_TYPE_NAN20_RANGING_REQUEST = 24,
    NAN_TLV_TYPE_NAN20_RANGING_RESULT = 25,
    NAN_TLV_TYPE_NAN20_RANGING_REQUEST_RECEIVED = 26,
    NAN_TLV_TYPE_NAN_PASSPHRASE = 27,
    NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO = 28,
    NAN_TLV_TYPE_DEV_CAP_ATTR_CAPABILITY = 29,
    NAN_TLV_TYPE_IP_TRANSPORT_PARAM = 30,
    NAN_TLV_TYPE_SERVICE_ID = 31,
    NAN_TLV_TYPE_PAIRING_CONFIGURATION = 32,
    NAN_TLV_TYPE_PAIRING_MATCH_PARAMS = 33,
    NAN_TLV_TYPE_BOOTSTRAPPING_PARAMS = 34,
    NAN_TLV_TYPE_BOOTSTRAPPING_COOKIE = 35,
    NAN_TLV_TYPE_NIRA_NONCE  = 36,
    NAN_TLV_TYPE_NIRA_TAG = 37,
    NAN_TLV_TYPE_NAN_CSID_EXT = 38,
    NAN_TLV_TYPE_CSIA_CAP = 39,
    NAN_TLV_TYPE_SDF_LAST = 4095,

    /* Configuration types */
    NAN_TLV_TYPE_CONFIG_FIRST = 4096,
    NAN_TLV_TYPE_24G_SUPPORT = NAN_TLV_TYPE_CONFIG_FIRST,
    NAN_TLV_TYPE_24G_BEACON,
    NAN_TLV_TYPE_24G_SDF,
    NAN_TLV_TYPE_24G_RSSI_CLOSE,
    NAN_TLV_TYPE_24G_RSSI_MIDDLE = 4100,
    NAN_TLV_TYPE_24G_RSSI_CLOSE_PROXIMITY,
    NAN_TLV_TYPE_5G_SUPPORT,
    NAN_TLV_TYPE_5G_BEACON,
    NAN_TLV_TYPE_5G_SDF,
    NAN_TLV_TYPE_5G_RSSI_CLOSE,
    NAN_TLV_TYPE_5G_RSSI_MIDDLE,
    NAN_TLV_TYPE_5G_RSSI_CLOSE_PROXIMITY,
    NAN_TLV_TYPE_SID_BEACON,
    NAN_TLV_TYPE_HOP_COUNT_LIMIT,
    NAN_TLV_TYPE_MASTER_PREFERENCE = 4110,
    NAN_TLV_TYPE_CLUSTER_ID_LOW,
    NAN_TLV_TYPE_CLUSTER_ID_HIGH,
    NAN_TLV_TYPE_RSSI_AVERAGING_WINDOW_SIZE,
    NAN_TLV_TYPE_CLUSTER_OUI_NETWORK_ID,
    NAN_TLV_TYPE_SOURCE_MAC_ADDRESS,
    NAN_TLV_TYPE_CLUSTER_ATTRIBUTE_IN_SDF,
    NAN_TLV_TYPE_SOCIAL_CHANNEL_SCAN_PARAMS,
    NAN_TLV_TYPE_DEBUGGING_FLAGS,
    NAN_TLV_TYPE_POST_NAN_CONNECTIVITY_CAPABILITIES_TRANSMIT,
    NAN_TLV_TYPE_POST_NAN_DISCOVERY_ATTRIBUTE_TRANSMIT = 4120,
    NAN_TLV_TYPE_FURTHER_AVAILABILITY_MAP,
    NAN_TLV_TYPE_HOP_COUNT_FORCE,
    NAN_TLV_TYPE_RANDOM_FACTOR_FORCE,
    NAN_TLV_TYPE_RANDOM_UPDATE_TIME = 4124,
    NAN_TLV_TYPE_EARLY_WAKEUP,
    NAN_TLV_TYPE_PERIODIC_SCAN_INTERVAL,
    NAN_TLV_TYPE_DW_INTERVAL = 4128,
    NAN_TLV_TYPE_DB_INTERVAL,
    NAN_TLV_TYPE_FURTHER_AVAILABILITY,
    NAN_TLV_TYPE_24G_CHANNEL,
    NAN_TLV_TYPE_5G_CHANNEL,
    NAN_TLV_TYPE_DISC_MAC_ADDR_RANDOM_INTERVAL,
    NAN_TLV_TYPE_RANGING_AUTO_RESPONSE_CFG = 4134,
    NAN_TLV_TYPE_SUBSCRIBE_SID_BEACON = 4135,
    NAN_TLV_TYPE_DW_EARLY_TERMINATION = 4136,
    NAN_TLV_TYPE_TX_RX_CHAINS = 4137,
    NAN_TLV_TYPE_ENABLE_DEVICE_RANGING = 4138,
    NAN_TLV_TYPE_UNSYNC_DISCOVERY_ENABLED = 4139,
    NAN_TLV_TYPE_FOLLOWUP_MGMT_RX_ENABLED = 4140,

    NAN_TLV_TYPE_CONFIG_LAST = 8191,

    /* Attributes types */
    NAN_TLV_TYPE_ATTRS_FIRST = 8192,
    NAN_TLV_TYPE_AVAILABILITY_INTERVALS_MAP = NAN_TLV_TYPE_ATTRS_FIRST,
    NAN_TLV_TYPE_WLAN_MESH_ID,
    NAN_TLV_TYPE_MAC_ADDRESS,
    NAN_TLV_TYPE_RECEIVED_RSSI_VALUE,
    NAN_TLV_TYPE_CLUSTER_ATTRIBUTE,
    NAN_TLV_TYPE_WLAN_INFRA_SSID,
    NAN_TLV_TYPE_NAN_SHARED_KEY_DESC_ATTR,
    NAN_TLV_TYPE_ATTRS_LAST = 12287,

    /* Events Type */
    NAN_TLV_TYPE_EVENTS_FIRST = 12288,
    NAN_TLV_TYPE_EVENT_SELF_STATION_MAC_ADDRESS = NAN_TLV_TYPE_EVENTS_FIRST,
    NAN_TLV_TYPE_EVENT_STARTED_CLUSTER,
    NAN_TLV_TYPE_EVENT_JOINED_CLUSTER,
    NAN_TLV_TYPE_EVENT_CLUSTER_SCAN_RESULTS,
    NAN_TLV_TYPE_FAW_MEM_AVAIL,
    NAN_TLV_TYPE_EVENTS_LAST = 16383,

    /* TCA types */
    NAN_TLV_TYPE_TCA_FIRST = 16384,
    NAN_TLV_TYPE_CLUSTER_SIZE_REQ = NAN_TLV_TYPE_TCA_FIRST,
    NAN_TLV_TYPE_CLUSTER_SIZE_RSP,
    NAN_TLV_TYPE_TCA_LAST = 32767,

    /* Statistics types */
    NAN_TLV_TYPE_STATS_FIRST = 32768,
    NAN_TLV_TYPE_DE_PUBLISH_STATS = NAN_TLV_TYPE_STATS_FIRST,
    NAN_TLV_TYPE_DE_SUBSCRIBE_STATS,
    NAN_TLV_TYPE_DE_MAC_STATS,
    NAN_TLV_TYPE_DE_TIMING_SYNC_STATS,
    NAN_TLV_TYPE_DE_DW_STATS,
    NAN_TLV_TYPE_DE_STATS,
    NAN_TLV_TYPE_STATS_LAST = 36863,

    /* Testmode types */
    NAN_TLV_TYPE_TESTMODE_FIRST = 36864,
    NAN_TLV_TYPE_TESTMODE_GENERIC_CMD = NAN_TLV_TYPE_TESTMODE_FIRST,
    NAN_TLV_TYPE_TESTMODE_LAST = 37000,

    /* NAN Security types */
    NAN_TLV_TYPE_SEC_FIRST = 37001,
    NAN_TLV_TYPE_SEC_IGTK_KDE = NAN_TLV_TYPE_SEC_FIRST,
    NAN_TLV_TYPE_SEC_BIGTK_KDE,
    NAN_TLV_TYPE_SEC_NM_TK,
    NAN_TLV_TYPE_GROUP_KEYS_PARAM,
    NAN_TLV_TYPE_SEC_LAST = 37100,

    /* NAN OEM Configuration types */
    NAN_TLV_TYPE_OEM_DATA_FIRST = 37101,
    NAN_TLV_TYPE_OEM1_DATA = NAN_TLV_TYPE_OEM_DATA_FIRST,
    NAN_TLV_TYPE_OEM_DATA_LAST  = 37150,

    NAN_TLV_TYPE_LAST = 65535
} NanTlvType;

/* 8-byte control message header used by NAN*/
typedef struct PACKED
{
   u16 msgVersion:4;
   u16 msgId:12;
   u16 msgLen;
   u16 handle;
   u16 transactionId;
} NanMsgHeader, *pNanMsgHeader;

/* Enumeration for Version */
typedef enum
{
   NAN_MSG_VERSION1 = 1,
}NanMsgVersion;

typedef struct PACKED
{
    u16 type;
    u16 length;
    u8* value;
} NanTlv, *pNanTlv;

#define SIZEOF_TLV_HDR (sizeof(NanTlv::type) + sizeof(NanTlv::length))
/* NAN TLV Groups and Types */
typedef enum
{
    NAN_TLV_GROUP_FIRST = 0,
    NAN_TLV_GROUP_SDF = NAN_TLV_GROUP_FIRST,
    NAN_TLV_GROUP_CONFIG,
    NAN_TLV_GROUP_STATS,
    NAN_TLV_GROUP_ATTRS,
    NAN_TLV_NUM_GROUPS,
    NAN_TLV_GROUP_LAST = NAN_TLV_NUM_GROUPS
} NanTlvGroup;

/* NAN Miscellaneous Constants */
#define NAN_TTL_INFINITE            0
#define NAN_REPLY_COUNT_INFINITE    0

/* NAN Confguration 5G Channel Access Bit */
#define NAN_5G_CHANNEL_ACCESS_UNSUPPORTED   0
#define NAN_5G_CHANNEL_ACCESS_SUPPORTED     1

/* NAN Configuration Service IDs Enclosure Bit */
#define NAN_SIDS_NOT_ENCLOSED_IN_BEACONS    0
#define NAN_SIBS_ENCLOSED_IN_BEACONS        1

/* NAN Configuration Priority */
#define NAN_CFG_PRIORITY_SERVICE_DISCOVERY  0
#define NAN_CFG_PRIORITY_DATA_CONNECTION    1

/* NAN Configuration 5G Channel Usage */
#define NAN_5G_CHANNEL_USAGE_SYNC_AND_DISCOVERY 0
#define NAN_5G_CHANNEL_USAGE_DISCOVERY_ONLY     1

/* NAN Configuration TX_Beacon Content */
#define NAN_TX_BEACON_CONTENT_OLD_AM_INFO       0
#define NAN_TX_BEACON_CONTENT_UPDATED_AM_INFO   1

/* NAN Configuration Miscellaneous Constants */
#define NAN_MAC_INTERFACE_PERIODICITY_MIN   30
#define NAN_MAC_INTERFACE_PERIODICITY_MAX   255

#define NAN_DW_RANDOM_TIME_MIN  120
#define NAN_DW_RANDOM_TIME_MAX  240

#define NAN_INITIAL_SCAN_MIN_IDEAL_PERIOD   200
#define NAN_INITIAL_SCAN_MAX_IDEAL_PERIOD   300

#define NAN_ONGOING_SCAN_MIN_PERIOD 10
#define NAN_ONGOING_SCAN_MAX_PERIOD 30

#define NAN_HOP_COUNT_LIMIT 5

#define NAN_WINDOW_DW   0
#define NAN_WINDOW_FAW  1

#define NAN_TLV_HEADER_SIZE 4

/* NAN Error Rsp */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    u16 status;
    u16 value;
} NanErrorRspMsg, *pNanErrorRspMsg;

//* NAN Publish Service Req */
typedef struct PACKED
{
    u16 ttl;
    u16 period;
    u32 replyIndFlag:1;
    u32 publishType:2;
    u32 txType:1;
    u32 rssiThresholdFlag:1;
    u32 ota_flag:1;
    u32 matchAlg:2;
    u32 count:8;
    u32 connmap:8;
    u32 pubTerminatedIndDisableFlag:1;
    u32 pubMatchExpiredIndDisableFlag:1;
    u32 followupRxIndDisableFlag:1;
    u32 reserved2:5;
    /*
     * Excludes TLVs
     *
     * Required: Service Name,
     * Optional: Tx Match Filter, Rx Match Filter, Service Specific Info,
     */
} NanPublishServiceReqParams, *pNanPublishServiceReqParams;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    NanPublishServiceReqParams publishServiceReqParams;
    u8 ptlv[];
} NanPublishServiceReqMsg, *pNanPublishServiceReqMsg;

/* NAN Publish Service Rsp */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /* status of the request */
    u16 status;
    u16 value;
} NanPublishServiceRspMsg, *pNanPublishServiceRspMsg;

/* NAN Publish Service Cancel Req */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
} NanPublishServiceCancelReqMsg, *pNanPublishServiceCancelReqMsg;

/* NAN Publish Service Cancel Rsp */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /* status of the request */
    u16 status;
    u16 value;
} NanPublishServiceCancelRspMsg, *pNanPublishServiceCancelRspMsg;

/* NAN Publish Terminated Ind */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /* reason for the termination */
    u16 reason;
    u16 reserved;
} NanPublishTerminatedIndMsg, *pNanPublishTerminatedIndMsg;

/* Params for NAN Publish Replied Ind */
typedef struct PACKED
{
  u32  matchHandle;
} NanPublishRepliedIndParams;

/* NAN Publish Replied Ind */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    NanPublishRepliedIndParams publishRepliedIndParams;
    /*
     * Excludes TLVs
     *
     * Required: MAC Address
     * Optional: Received RSSI Value
     *
     */
    u8 ptlv[];
} NanPublishRepliedIndMsg, *pNanPublishRepliedIndMsg;

/* NAN Device Capability Attribute */
typedef struct PACKED
{
    u32 dfs_master:1;
    u32 ext_key_id:1;
    u32 simu_ndp_data_recept:1;
    u32 ndpe_attr_supp:1;
    u32 reserved:28;
} NanDevCapAttrCap, *pNanDevCapAttrCap;

/* NAN Subscribe Service Req */
typedef struct PACKED
{
    u16 ttl;
    u16 period;
    u32 subscribeType:1;
    u32 srfAttr:1;
    u32 srfInclude:1;
    u32 srfSend:1;
    u32 ssiRequired:1;
    u32 matchAlg:2;
    u32 xbit:1;
    u32 count:8;
    u32 rssiThresholdFlag:1;
    u32 ota_flag:1;
    u32 subTerminatedIndDisableFlag:1;
    u32 subMatchExpiredIndDisableFlag:1;
    u32 followupRxIndDisableFlag:1;
    u32 reserved:3;
    u32 connmap:8;
    /*
     * Excludes TLVs
     *
     * Required: Service Name
     * Optional: Rx Match Filter, Tx Match Filter, Service Specific Info,
     */
} NanSubscribeServiceReqParams, *pNanSubscribeServiceReqParams;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    NanSubscribeServiceReqParams subscribeServiceReqParams;
    u8 ptlv[];
} NanSubscribeServiceReqMsg, *pNanSubscribeServiceReqMsg;

/* NAN Subscribe Service Rsp */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /* status of the request */
    u16 status;
    u16 value;
} NanSubscribeServiceRspMsg, *pNanSubscribeServiceRspMsg;

/* NAN Subscribe Service Cancel Req */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
} NanSubscribeServiceCancelReqMsg, *pNanSubscribeServiceCancelReqMsg;

/* NAN Subscribe Service Cancel Rsp */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /* status of the request */
    u16 status;
    u16 value;
} NanSubscribeServiceCancelRspMsg, *pNanSubscribeServiceCancelRspMsg;

/* NAN Subscribe Match Ind */
typedef struct PACKED
{
    u32 matchHandle;
    u32 matchOccuredFlag:1;
    u32 outOfResourceFlag:1;
    u32 reserved:30;
} NanMatchIndParams;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    NanMatchIndParams matchIndParams;
    u8 ptlv[];
} NanMatchIndMsg, *pNanMatchIndMsg;

/* NAN Subscribe Unmatch Ind */
typedef struct PACKED
{
    u32 matchHandle;
} NanmatchExpiredIndParams;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    NanmatchExpiredIndParams matchExpiredIndParams;
} NanMatchExpiredIndMsg, *pNanMatchExpiredIndMsg;

/* NAN Subscribe Terminated Ind */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /* reason for the termination */
    u16 reason;
    u16 reserved;
} NanSubscribeTerminatedIndMsg, *pNanSubscribeTerminatedIndMsg;

/* Event Ind */
typedef struct PACKED
{
    u32 eventId:8;
    u32 reserved:24;
} NanEventIndParams;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    u8 ptlv[];
} NanEventIndMsg, *pNanEventIndMsg;

/* NAN Transmit Followup Req */
typedef struct PACKED
{
    u32 matchHandle;
    u32 priority:4;
    u32 window:1;
    u32 followupTxRspDisableFlag:1;
    u32 reserved:26;
    /*
     * Excludes TLVs
     *
     * Required: Service Specific Info or Extended Service Specific Info
     */
} NanTransmitFollowupReqParams;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    NanTransmitFollowupReqParams transmitFollowupReqParams;
    u8 ptlv[];
} NanTransmitFollowupReqMsg, *pNanTransmitFollowupReqMsg;

/* NAN Transmit Followup Rsp */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /* status of the request */
    u16 status;
    u16 value;
} NanTransmitFollowupRspMsg, *pNanTransmitFollowupRspMsg;

/* NAN Publish Followup Ind */
typedef struct PACKED
{
    u32 matchHandle;
    u32 window:1;
    u32 reserved:31;
    /*
     * Excludes TLVs
     *
     * Required: Service Specific Info or Extended Service Specific Info
     */
} NanFollowupIndParams;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    NanFollowupIndParams followupIndParams;
    u8 ptlv[];
} NanFollowupIndMsg, *pNanFollowupIndMsg;

/* NAN Statistics Req */
typedef struct PACKED
{
    u32 statsType:8;
    u32 clear:1;
    u32 reserved:23;
} NanStatsReqParams, *pNanStatsReqParams;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    NanStatsReqParams statsReqParams;
} NanStatsReqMsg, *pNanStatsReqMsg;

/* NAN Statistics Rsp */
typedef struct PACKED
{
    /* status of the request */
    u16 status;
    u16 value;
    u8 statsType;
    u8 reserved;
} NanStatsRspParams, *pNanStatsRspParams;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    NanStatsRspParams statsRspParams;
    u8 ptlv[];
} NanStatsRspMsg, *pNanStatsRspMsg;

typedef struct PACKED
{
    u8 count:7;
    u8 s:1;
} NanSidAttr, *pSidAttr;

/* type + length + oui + oui type */
#define NAN_IE_HEADER 6
#define NAN_IE_VENDOR_TYPE 0x506f9a13
#define DCEA_PARING_SETUP_ENABLED  BIT(8)
#define DCEA_NPK_CACHING_ENABLED  BIT(9)
#define WPA_NDP_PMK_MAX_LEN 32
#define WPA_OPP_NPK_MAX_LEN 32

/* As per NAN spec, below are the values defined for CSID attribute */
#define NCS_SK_128          1
#define NCS_SK_256          2
#define NCS_PK_2WDH_128     3
#define NCS_PK_2WDH_256     4
#define NCS_GTK_CCMP_128    5
#define NCS_GTK_GCMP_256    6
#define NCS_PK_PASN_128     7
#define NCS_PK_PASN_256     8

enum nan_attr_id {
    NAN_ATTR_ID_SERVICE_DESCRIPTOR = 0x3,
    NAN_ATTR_ID_SDE                = 0xE,
    NAN_ATTR_ID_CSIA               = 0x22,
    NAN_ATTR_ID_SHARED_KEY_DESC    = 0x24,
    NAN_ATTR_ID_DCEA               = 0x2A,
    NAN_ATTR_ID_NPBA               = 0x2C,
    NAN_ATTR_ID_NIRA               = 0x2B,
};

#define NAN_GTK_LEN 16
#define NAN_GTK_MAX_LEN 32
#define NAN_IGTK_LEN 16
#define NAN_IGTK_MAX_LEN 32
#define NAN_BIGTK_LEN 16
#define NAN_BIGTK_MAX_LEN 32
#define NAN_IGTK_KDE_PREFIX_LEN (2 + 6)
#define NAN_BIGTK_KDE_PREFIX_LEN (2 + 6)
#define NAN_MAX_SHARED_KEY_ATTR_LEN 1024
#define MAX_IGTK_KDE_LEN 70
#define MAX_BIGTK_KDE_LEN 70
#define NAN_SHARED_KEY_ATTR_ID 0x24
#define NAN_ENCRYPT_KEY_DATA   BIT(12)
#define NAN_VENDOR_ATTR_TYPE   0xdd

#define NAN_KDE_TYPE_IGTK                 0x02
#define NAN_KDE_TYPE_BIGTK                0x03
#define NAN_KDE_TYPE_IGTK_LIFETIME        0x06
#define NAN_KDE_TYPE_BIGTK_LIFETIME       0x07
#define NAN_KDE_TYPE_NIK                  0x24
#define NAN_KDE_TYPE_NIK_LIFETIME         0x25

#define NAN_IGTK_KEY_IDX                   4
#define NAN_BIGTK_KEY_IDX                  6

#define NAN_PN_REQ_BITMAP_IGTK             BIT(0)
#define NAN_PN_REQ_BITMAP_BIGTK            BIT(1)

/* sub attribute iteration helpers */
#define for_each_nan_subattr(_subattr, _data, _datalen)                    \
        for (_subattr = (const nan_subattr *) (_data);                  \
             (const u8 *) (_data) + (_datalen) - (const u8 *) _subattr >=  \
                (int) sizeof(*_subattr) &&                                 \
             (const u8 *) (_data) + (_datalen) - (const u8 *) _subattr >=  \
                (int) sizeof(*_subattr) + _subattr->datalen;                  \
             _subattr = (const nan_subattr *) (_subattr->data + _subattr->datalen))

#define for_each_nan_subattr_id(subattr, _id, data, datalen)            \
        for_each_nan_subattr(subattr, data, datalen)                    \
                if (subattr->id == (_id))

typedef struct PACKED {
         u8 id;
         u16 datalen;
         u8 data[];
} nan_subattr;

typedef struct PACKED {
        u8 attr_id;
        u16 len;
        u16 cap_info;
} nan_dcea;

typedef struct PACKED {
        u8 cipher;
        u8 pub_id;
} nan_csa;

typedef struct PACKED {
        u8 attr_id;
        u16 len;
        u8 caps;
        nan_csa csa[0];
} nan_csia;

typedef struct PACKED {
        u8 attr_id;
        u16 len;
        u8 dialog_token;
        u8 type_status;
        u8 reason_code;
        u16 bootstrapping_method;
} nan_npba;

typedef struct PACKED {
        u8 attr_id;
        u16 len;
        u8 cipher_ver;
        u8 nonce_tag[32];
} nan_nira;

typedef struct PACKED {
        u8 attr_id;
        u16 len;
        u8 service_id[6];
        u8 instance_id;
        u8 requestor_id;
        u8 service_control_flags;
} nan_sda;

/* NAN Configuration Req */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /*
     * TLVs:
     *
     * Required: None.
     * Optional: SID, Random Time, Master Preference, WLAN Intra Attr,
     *           P2P Operation Attr, WLAN IBSS Attr, WLAN Mesh Attr
     */
    u8 ptlv[];
} NanConfigurationReqMsg, *pNanConfigurationReqMsg;

/*
 * Because the Configuration Req message has TLVs in it use the macro below
 * for the size argument to buffer allocation functions (vs. sizeof(msg)).
 */
#define NAN_MAX_CONFIGURATION_REQ_SIZE                       \
    (                                                        \
        sizeof(NanMsgHeader)                             +   \
        SIZEOF_TLV_HDR + sizeof(u8)  /* SID Beacon    */ +   \
        SIZEOF_TLV_HDR + sizeof(u8)  /* Random Time   */ +   \
        SIZEOF_TLV_HDR + sizeof(u8)  /* Master Pref   */     \
    )

/* NAN Configuration Rsp */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /* status of the request */
    u16 status;
    u16 value;
} NanConfigurationRspMsg, *pNanConfigurationRspMsg;

/*
 * Because the Enable Req message has TLVs in it use the macro below for
 * the size argument to buffer allocation functions (vs. sizeof(msg)).
 */
#define NAN_MAX_ENABLE_REQ_SIZE                                 \
    (                                                           \
        sizeof(NanMsgHeader)                                +   \
        SIZEOF_TLV_HDR + sizeof(u16) /* Cluster Low   */    +   \
        SIZEOF_TLV_HDR + sizeof(u16) /* Cluster High  */    +   \
        SIZEOF_TLV_HDR + sizeof(u8)  /* Master Pref   */        \
    )

/* Config Discovery Indication */
 typedef struct PACKED
 {
    u32 disableDiscoveryMacAddressEvent:1;
    u32 disableDiscoveryStartedClusterEvent:1;
    u32 disableDiscoveryJoinedClusterEvent:1;
    u32 reserved:29;
 } NanConfigDiscoveryIndications;

/* NAN Enable Req */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /*
     * TLVs:
     *
     * Required: Cluster Low, Cluster High, Master Preference,
     * Optional: 5G Support, SID, 5G Sync Disc, RSSI Close, RSSI Medium,
     *           Hop Count Limit, Random Time, Master Preference,
     *           WLAN Intra Attr, P2P Operation Attr, WLAN IBSS Attr,
     *           WLAN Mesh Attr
     */
    u8 ptlv[];
} NanEnableReqMsg, *pNanEnableReqMsg;

/* NAN Enable Rsp */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /* status of the request */
    u16 status;
    u16 value;
} NanEnableRspMsg, *pNanEnableRspMsg;

/* NAN Disable Req */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
} NanDisableReqMsg, *pNanDisableReqMsg;

/* NAN Disable Rsp */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /* status of the request */
    u16 status;
    u16 reserved;
} NanDisableRspMsg, *pNanDisableRspMsg;

/* NAN Disable Ind */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /* reason for the termination */
    u16 reason;
    u16 reserved;
} NanDisableIndMsg, *pNanDisableIndMsg;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    u8 ptlv[];
} NanTcaReqMsg, *pNanTcaReqMsg;

/* NAN TCA Rsp */
typedef struct PACKED
{
    NanMsgHeader   fwHeader;
    /* status of the request */
    u16 status;
    u16 value;
} NanTcaRspMsg, *pNanTcaRspMsg;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /*
     * TLVs:
     *
     * Optional: Cluster size.
     */
    u8 ptlv[];
} NanTcaIndMsg, *pNanTcaIndMsg;

/*
 * Because the TCA Ind message has TLVs in it use the macro below for the
 * size argument to buffer allocation functions (vs. sizeof(msg)).
 */
#define NAN_MAX_TCA_IND_SIZE                                 \
    (                                                        \
        sizeof(NanMsgHeader)                             +   \
        sizeof(NanTcaIndParams)                          +   \
        SIZEOF_TLV_HDR + sizeof(u16) /* Cluster Size */      \
    )

/* Function Declarations */
u8* addTlv(u16 type, u16 length, const u8* value, u8* pOutTlv);
u16 NANTLV_ReadTlv(u8 *pInTlv, pNanTlv pOutTlv, int inBufferSize);
u16 NANTLV_WriteTlv(pNanTlv pInTlv, u8 *pOutTlv);

/* NAN Beacon Sdf Payload Req */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /*
     * TLVs:
     *
     * Optional: Vendor specific attribute
     */
    u8 ptlv[];
} NanBeaconSdfPayloadReqMsg, *pNanBeaconSdfPayloadReqMsg;

/* NAN Beacon Sdf Payload Rsp */
typedef struct PACKED
{
    NanMsgHeader   fwHeader;
    /* status of the request */
    u16 status;
    u16 reserved;
} NanBeaconSdfPayloadRspMsg, *pNanBeaconSdfPayloadRspMsg;

/* NAN Beacon Sdf Payload Ind */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /*
     * TLVs:
     *
     * Required: Mac address
     * Optional: Vendor specific attribute, sdf payload
     * receive
     */
    u8 ptlv[];
} NanBeaconSdfPayloadIndMsg, *pNanBeaconSdfPayloadIndMsg;

typedef struct PACKED
{
    u8 availIntDuration:2;
    u8 mapId:4;
    u8 reserved:2;
} NanApiEntryCtrl;

/*
 * Valid Operating Classes were derived from IEEE Std. 802.11-2012 Annex E
 * Table E-4 Global Operating Classe and, filtered by channel, are: 81, 83,
 * 84, 103, 114, 115, 116, 124, 125.
 */
typedef struct PACKED
{
    NanApiEntryCtrl entryCtrl;
    u8 opClass;
    u8 channel;
    u8 availIntBitmap[4];
} NanFurtherAvailabilityChan, *pNanFurtherAvailabilityChan;

typedef struct PACKED
{
    u8 numChan;
    u8 pFaChan[];
} NanFurtherAvailabilityMapAttrTlv, *pNanFurtherAvailabilityMapAttrTlv;

/* Publish statistics. */
typedef struct PACKED
{
    u32 validPublishServiceReqMsgs;
    u32 validPublishServiceRspMsgs;
    u32 validPublishServiceCancelReqMsgs;
    u32 validPublishServiceCancelRspMsgs;
    u32 validPublishRepliedIndMsgs;
    u32 validPublishTerminatedIndMsgs;
    u32 validActiveSubscribes;
    u32 validMatches;
    u32 validFollowups;
    u32 invalidPublishServiceReqMsgs;
    u32 invalidPublishServiceCancelReqMsgs;
    u32 invalidActiveSubscribes;
    u32 invalidMatches;
    u32 invalidFollowups;
    u32 publishCount;
    u32 publishNewMatchCount;
    u32 pubsubGlobalNewMatchCount;
} FwNanPublishStats, *pFwNanPublishStats;

/* Subscribe statistics. */
typedef struct PACKED
{
    u32 validSubscribeServiceReqMsgs;
    u32 validSubscribeServiceRspMsgs;
    u32 validSubscribeServiceCancelReqMsgs;
    u32 validSubscribeServiceCancelRspMsgs;
    u32 validSubscribeTerminatedIndMsgs;
    u32 validSubscribeMatchIndMsgs;
    u32 validSubscribeUnmatchIndMsgs;
    u32 validSolicitedPublishes;
    u32 validMatches;
    u32 validFollowups;
    u32 invalidSubscribeServiceReqMsgs;
    u32 invalidSubscribeServiceCancelReqMsgs;
    u32 invalidSubscribeFollowupReqMsgs;
    u32 invalidSolicitedPublishes;
    u32 invalidMatches;
    u32 invalidFollowups;
    u32 subscribeCount;
    u32 bloomFilterIndex;
    u32 subscribeNewMatchCount;
    u32 pubsubGlobalNewMatchCount;
} FwNanSubscribeStats, *pFwNanSubscribeStats;

/* NAN MAC Statistics. Used for MAC and DW statistics. */
typedef struct PACKED
{
    /* RX stats */
    u32 validFrames;
    u32 validActionFrames;
    u32 validBeaconFrames;
    u32 ignoredActionFrames;
    u32 ignoredBeaconFrames;
    u32 invalidFrames;
    u32 invalidActionFrames;
    u32 invalidBeaconFrames;
    u32 invalidMacHeaders;
    u32 invalidPafHeaders;
    u32 nonNanBeaconFrames;

    u32 earlyActionFrames;
    u32 inDwActionFrames;
    u32 lateActionFrames;

    /* TX stats */
    u32 framesQueued;
    u32 totalTRSpUpdates;
    u32 completeByTRSp;
    u32 completeByTp75DW;
    u32 completeByTendDW;
    u32 lateActionFramesTx;

    /* Misc stats - ignored for DW. */
    u32 twIncreases;
    u32 twDecreases;
    u32 twChanges;
    u32 twHighwater;
    u32 bloomFilterIndex;
} FwNanMacStats, *pFwNanMacStats;

/* NAN Sync and DW Statistics*/
typedef struct PACKED
{
    u64 currTsf;
    u64 myRank;
    u64 currAmRank;
    u64 lastAmRank;
    u32 currAmBTT;
    u32 lastAmBTT;
    u8  currAmHopCount;
    u8  currRole;
    u16 currClusterId;
    u32 reserved1;

    u64 timeSpentInCurrRole;
    u64 totalTimeSpentAsMaster;
    u64 totalTimeSpentAsNonMasterSync;
    u64 totalTimeSpentAsNonMasterNonSync;
    u32 transitionsToAnchorMaster;
    u32 transitionsToMaster;
    u32 transitionsToNonMasterSync;
    u32 transitionsToNonMasterNonSync;
    u32 amrUpdateCount;
    u32 amrUpdateRankChangedCount;
    u32 amrUpdateBTTChangedCount;
    u32 amrUpdateHcChangedCount;
    u32 amrUpdateNewDeviceCount;
    u32 amrExpireCount;
    u32 mergeCount;
    u32 beaconsAboveHcLimit;
    u32 beaconsBelowRssiThresh;
    u32 beaconsIgnoredNoSpace;
    u32 beaconsForOurCluster;
    u32 beaconsForOtherCluster;
    u32 beaconCancelRequests;
    u32 beaconCancelFailures;
    u32 beaconUpdateRequests;
    u32 beaconUpdateFailures;
    u32 syncBeaconTxAttempts;
    u32 syncBeaconTxFailures;
    u32 discBeaconTxAttempts;
    u32 discBeaconTxFailures;
    u32 amHopCountExpireCount;
    u32 ndpChannelFreq;
    u32 ndpChannelFreq2;
    u32 schedUpdateChannelFreq;
} FwNanSyncStats, *pFwNanSyncStats;

/* NAN Misc DE Statistics */
typedef struct PACKED
{
    u32 validErrorRspMsgs;
    u32 validTransmitFollowupReqMsgs;
    u32 validTransmitFollowupRspMsgs;
    u32 validFollowupIndMsgs;
    u32 validConfigurationReqMsgs;
    u32 validConfigurationRspMsgs;
    u32 validStatsReqMsgs;
    u32 validStatsRspMsgs;
    u32 validEnableReqMsgs;
    u32 validEnableRspMsgs;
    u32 validDisableReqMsgs;
    u32 validDisableRspMsgs;
    u32 validDisableIndMsgs;
    u32 validEventIndMsgs;
    u32 validTcaReqMsgs;
    u32 validTcaRspMsgs;
    u32 validTcaIndMsgs;
    u32 invalidTransmitFollowupReqMsgs;
    u32 invalidConfigurationReqMsgs;
    u32 invalidStatsReqMsgs;
    u32 invalidEnableReqMsgs;
    u32 invalidDisableReqMsgs;
    u32 invalidTcaReqMsgs;
} FwNanDeStats, *pFwNanDeStats;

/*
  Definition of various NanIndication(events)
*/
typedef enum {
    NAN_INDICATION_PUBLISH_REPLIED         =0,
    NAN_INDICATION_PUBLISH_TERMINATED      =1,
    NAN_INDICATION_MATCH                   =2,
    NAN_INDICATION_MATCH_EXPIRED           =3,
    NAN_INDICATION_SUBSCRIBE_TERMINATED    =4,
    NAN_INDICATION_DE_EVENT                =5,
    NAN_INDICATION_FOLLOWUP                =6,
    NAN_INDICATION_DISABLED                =7,
    NAN_INDICATION_TCA                     =8,
    NAN_INDICATION_BEACON_SDF_PAYLOAD      =9,
    NAN_INDICATION_SELF_TRANSMIT_FOLLOWUP  =10,
    NAN_INDICATION_RANGING_REQUEST_RECEIVED =11,
    NAN_INDICATION_RANGING_RESULT           =12,
    NAN_INDICATION_IDENTITY_RESOLUTION      =13,
    NAN_INDICATION_VENDOR_EVENT             =14,
    NAN_INDICATION_UNKNOWN                 =0xFFFF
} NanIndicationType;

/* NAN Capabilities Req */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
} NanCapabilitiesReqMsg, *pNanCapabilitiesReqMsg;

/* NAN Capabilities Rsp */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /* status of the request */
    u32 status;
    u32 value;
    u32 max_concurrent_nan_clusters;
    u32 max_publishes;
    u32 max_subscribes;
    u32 max_service_name_len;
    u32 max_match_filter_len;
    u32 max_total_match_filter_len;
    u32 max_service_specific_info_len;
    u32 max_vsa_data_len;
    u32 max_mesh_data_len;
    u32 max_ndi_interfaces;
    u32 max_ndp_sessions;
    u32 max_app_info_len;
    u32 max_queued_transmit_followup_msgs;
    u32 ndp_supported_bands;
    u32 cipher_suites_supported;
    u32 max_scid_len;
    u32 is_ndp_security_supported:1;
    u32 max_sdea_service_specific_info_len:16;
    u32 max_nan_rtt_initiator_supported:5;
    u32 max_nan_rtt_responder_supported:5;
    u32 ndpe_attr_supported:1;
    u32 nan_pairing_supported:1;
    u32 nan_usd_publisher_supported:1;
    u32 nan_usd_subscriber_supported:1;
    u32 nan_followup_rx_forward_supported:1;
    u32 max_subscribe_address;
    u32 max_nan_pairing_sessions;
    u32 nan_group_mfp_cap;

} NanCapabilitiesRspMsg, *pNanCapabilitiesRspMsg;

/* NAN Self Transmit Followup */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    u32 reason;
} NanSelfTransmitFollowupIndMsg, *pNanSelfTransmitFollowupIndMsg;

/* NAN Group MFP support */
#define NAN_GTKSA_IGTKSA_BIGTKSA_NOT_SUPPORTED             0x00
#define NAN_GTKSA_IGTKSA_SUPPORTED_BIGTKSA_NOT_SUPPORTED   0x01
#define NAN_GTKSA_IGTKSA_BIGTKSA_SUPPORTED                 0x02

/* Mask to check extended csid type is set */
#define NAN_EXT_CSID_TYPE_MASK  ~0x3

/* NAN Cipher Suite Shared Key */
typedef struct PACKED
{
    u32 csid_type;
} NanCsidType;

/* Service Discovery Extended Attribute params */
typedef struct PACKED
{
    u32 fsd_required:1;
    u32 fsd_with_gas:1;
    u32 data_path_required:1;
    u32 data_path_type:1;
    u32 multicast_type:1;
    u32 qos_required:1;
    u32 security_required:1;
    u32 ranging_required:1;
    u32 range_limit_present:1;
    u32 service_update_ind_present:1;
    u32 gtk_protection:1;
    u32 reserved1:5;
    u32 range_report:1;
    u32 reserved2:15;
} NanFWSdeaCtrlParams;

/* NAN Ranging Configuration params */
typedef struct PACKED
{
    u32  inner_threshold;
    u32  outer_threshold;
} NanFWGeoFenceDescriptor;

typedef struct PACKED
{
    u32 range_resolution;
    u32 range_interval;
    u32 ranging_indication_event;
    NanFWGeoFenceDescriptor geo_fence_threshold;
} NanFWRangeConfigParams;

typedef struct PACKED
{
    u32 pairing_setup_required:1;
    u32 npk_nik_caching_required:1;
    u32 bootstrapping_method_bitmap:16;
    u32 reserved:14;
} NanFWPairingConfigParams;

typedef struct PACKED
{
    u32 pairing_setup_required:1;
    u32 npk_nik_caching_required:1;
    u32 bootstrapping_method_bitmap:16;
    u32 reserved:14;
} NanFWPairingParamsMatch;

typedef struct
{
    u8 instance_id;
    u16 sdea_control;
    u16 range_limit_ingress;
    u16 range_limit_egress;
    u8 service_update_indicator;
    u16 ssi_len;
    u8 ssi[NAN_FOLLOWUP_MAX_EXT_SERVICE_SPECIFIC_INFO_LEN];
} nan_sdea;

typedef enum {
    NAN_BS_TYPE_ADVERTISE = 0,
    NAN_BS_TYPE_REQUEST,
    NAN_BS_TYPE_RESPONSE,
} NanBootstrappingType;

typedef enum {
    NAN_BS_STATUS_ACCEPT = 0,
    NAN_BS_STATUS_REJECT,
    NAN_BS_STATUS_COMEBACK,
} NanBootstrappingStatus;

typedef struct PACKED
{
    u8 type;
    u8 status;
    u8 dialog_token;
    u8 reason_code;
    u16 bootstrapping_method_bitmap;
    u16 comeback_after;
} NanFWBootstrappingParams;

/* NAN Identity Resolution Params */
typedef struct PACKED
{
    u32 cipher_version:8;
    u32 reserved:24;
} NanFWIdentityResolutionParams;


typedef struct PACKED
{
    NanMsgHeader fwHeader;
    NanFWIdentityResolutionParams identityresolutionParams;
    /*
     * Excludes TLVs
     *
     * Required: Nounce, Tag
     */
    u8 ptlv[];
} NanFWIdentityResolutionReqMsg, *pNanFWIdentityResolutionReqMsg;

/* NAN Pairing Request Params */
typedef struct PACKED
{
    u32 pairing_handle;
    u32 pairing_verification;
    u32 cipher_suite;
} NanFWPairingIndParams;

typedef struct PACKED
{
    NanMsgHeader               fwHeader;
    NanFWPairingIndParams      pairingIndParams;
    /* TLVs Required:
       MANDATORY
       1. MAC_ADDRESS (Peer NMI)
       2. NM_TK (The TK derived from pairing)
    */
    u8 ptlv[];
} NanFWPairingIndMsg, *pNanFWPairingIndMsg;

typedef struct PACKED
{
    NanMsgHeader               fwHeader;
    u32                        pairing_handle;
} NanFWUnPairingIndMsg, *pFWNanUnPairingIndMsg;

/* NAN Group Key params */
typedef struct PACKED
{
    u32 key_cipher:16;
    u32 key_idx:8;
    u32 key_len:8;
    u8  key_data[NAN_MAX_GROUP_KEY_LEN];
    u8  key_rsc[NAN_MAX_GROUP_KEY_RSC_LEN];
} NanGroupKeyParamsTlv;

/* NAN Group Key Install Req Msg */
typedef struct PACKED
{
    NanMsgHeader fwHeader;

    /*
     * TLVs
     *
     * Required: MAC address, one or more tNanGroupKeyParamsTlv.
     */
    u8 ptlv[];
    /* NOTE:
     * This struct cannot be expanded, due to the above variable-length array.
     */
} NanGroupKeyInstallReqMsg, *pNanGroupKeyInstallReqMsg;

/* NAN Group Key Install Rsp Msg */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    u16 status;
    u16 value;
} NanGroupKeyInstallRspMsg, *pNanGroupKeyInstallRspMsg;

/* NAN Group Key TX PN fetch Req Msg */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    u32 key_idx;
} NanTxPnReqMsg, *pNanTxPnReqMsg;

/* NAN Group Key TX PN fetch Rsp Msg */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    u16 status;
    u16 value;
    u8  key_rsc[NAN_MAX_GROUP_KEY_RSC_LEN];
} NanTxPnRspMsg, *pNanTxPnRspMsg;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /*
      Excludes TLVs
      Optional: Nan Availability
    */
    u8 ptlv[];
} NanTestModeReqMsg, *pNanTestModeReqMsg;

/*
  NAN Status codes exchanged between firmware
  and WifiHal.
*/
typedef enum {
    /* NAN Protocol Response Codes */
    NAN_I_STATUS_SUCCESS = 0,
    NAN_I_STATUS_TIMEOUT = 1,
    NAN_I_STATUS_DE_FAILURE = 2,
    NAN_I_STATUS_INVALID_MSG_VERSION = 3,
    NAN_I_STATUS_INVALID_MSG_LEN = 4,
    NAN_I_STATUS_INVALID_MSG_ID = 5,
    NAN_I_STATUS_INVALID_HANDLE = 6,
    NAN_I_STATUS_NO_SPACE_AVAILABLE = 7,
    NAN_I_STATUS_INVALID_PUBLISH_TYPE = 8,
    NAN_I_STATUS_INVALID_TX_TYPE = 9,
    NAN_I_STATUS_INVALID_MATCH_ALGORITHM = 10,
    NAN_I_STATUS_DISABLE_IN_PROGRESS = 11,
    NAN_I_STATUS_INVALID_TLV_LEN = 12,
    NAN_I_STATUS_INVALID_TLV_TYPE = 13,
    NAN_I_STATUS_MISSING_TLV_TYPE = 14,
    NAN_I_STATUS_INVALID_TOTAL_TLVS_LEN = 15,
    NAN_I_STATUS_INVALID_REQUESTER_INSTANCE_ID= 16,
    NAN_I_STATUS_INVALID_TLV_VALUE = 17,
    NAN_I_STATUS_INVALID_TX_PRIORITY = 18,
    NAN_I_STATUS_INVALID_CONNECTION_MAP = 19,
    NAN_I_STATUS_INVALID_THRESHOLD_CROSSING_ALERT_ID = 20,
    NAN_I_STATUS_INVALID_STATS_ID = 21,
    NAN_I_STATUS_NAN_NOT_ALLOWED = 22,
    NAN_I_STATUS_NO_OTA_ACK = 23,
    NAN_I_STATUS_TX_FAIL = 24,
    NAN_I_STATUS_NAN_ALREADY_ENABLED = 25,
    NAN_I_STATUS_FOLLOWUP_QUEUE_FULL = 26,
    /* 27-4095 Reserved */
    /* NAN Configuration Response codes */
    NAN_I_STATUS_INVALID_RSSI_CLOSE_VALUE = 4096,
    NAN_I_STATUS_INVALID_RSSI_MIDDLE_VALUE = 4097,
    NAN_I_STATUS_INVALID_HOP_COUNT_LIMIT = 4098,
    NAN_I_STATUS_INVALID_MASTER_PREFERENCE_VALUE = 4099,
    NAN_I_STATUS_INVALID_LOW_CLUSTER_ID_VALUE = 4100,
    NAN_I_STATUS_INVALID_HIGH_CLUSTER_ID_VALUE = 4101,
    NAN_I_STATUS_INVALID_BACKGROUND_SCAN_PERIOD = 4102,
    NAN_I_STATUS_INVALID_RSSI_PROXIMITY_VALUE = 4103,
    NAN_I_STATUS_INVALID_SCAN_CHANNEL = 4104,
    NAN_I_STATUS_INVALID_POST_NAN_CONNECTIVITY_CAPABILITIES_BITMAP = 4105,
    NAN_I_STATUS_INVALID_FURTHER_AVAILABILITY_MAP_NUMCHAN_VALUE = 4106,
    NAN_I_STATUS_INVALID_FURTHER_AVAILABILITY_MAP_DURATION_VALUE = 4107,
    NAN_I_STATUS_INVALID_FURTHER_AVAILABILITY_MAP_CLASS_VALUE = 4108,
    NAN_I_STATUS_INVALID_FURTHER_AVAILABILITY_MAP_CHANNEL_VALUE = 4109,
    NAN_I_STATUS_INVALID_FURTHER_AVAILABILITY_MAP_AVAILABILITY_INTERVAL_BITMAP_VALUE = 4110,
    NAN_I_STATUS_INVALID_FURTHER_AVAILABILITY_MAP_MAP_ID = 4111,
    NAN_I_STATUS_INVALID_POST_NAN_DISCOVERY_CONN_TYPE_VALUE = 4112,
    NAN_I_STATUS_INVALID_POST_NAN_DISCOVERY_DEVICE_ROLE_VALUE = 4113,
    NAN_I_STATUS_INVALID_POST_NAN_DISCOVERY_DURATION_VALUE = 4114,
    NAN_I_STATUS_INVALID_POST_NAN_DISCOVERY_BITMAP_VALUE = 4115,
    NAN_I_STATUS_MISSING_FUTHER_AVAILABILITY_MAP = 4116,
    NAN_I_STATUS_INVALID_BAND_CONFIG_FLAGS = 4117,
    NAN_I_STATUS_INVALID_RANDOM_FACTOR_UPDATE_TIME_VALUE = 4118,
    NAN_I_STATUS_INVALID_ONGOING_SCAN_PERIOD = 4119,
    NAN_I_STATUS_INVALID_DW_INTERVAL_VALUE = 4120,
    NAN_I_STATUS_INVALID_DB_INTERVAL_VALUE = 4121,
    /* 4122-8191 RESERVED */
    NAN_I_PUBLISH_SUBSCRIBE_TERMINATED_REASON_INVALID = 8192,
    NAN_I_PUBLISH_SUBSCRIBE_TERMINATED_REASON_TIMEOUT = 8193,
    NAN_I_PUBLISH_SUBSCRIBE_TERMINATED_REASON_USER_REQUEST = 8194,
    NAN_I_PUBLISH_SUBSCRIBE_TERMINATED_REASON_FAILURE = 8195,
    NAN_I_PUBLISH_SUBSCRIBE_TERMINATED_REASON_COUNT_REACHED = 8196,
    NAN_I_PUBLISH_SUBSCRIBE_TERMINATED_REASON_DE_SHUTDOWN = 8197,
    NAN_I_PUBLISH_SUBSCRIBE_TERMINATED_REASON_DISABLE_IN_PROGRESS = 8198,
    NAN_I_PUBLISH_SUBSCRIBE_TERMINATED_REASON_POST_DISC_ATTR_EXPIRED = 8199,
    NAN_I_PUBLISH_SUBSCRIBE_TERMINATED_REASON_POST_DISC_LEN_EXCEEDED = 8200,
    NAN_I_PUBLISH_SUBSCRIBE_TERMINATED_REASON_FURTHER_AVAIL_MAP_EMPTY = 8201,

    /* Status related to Key Install and PN */
    NAN_I_STATUS_INVALID_IGTK_PARAMS = 8501,
    NAN_I_STATUS_INVALID_BIGTK_PARAMS = 8502,
    NAN_I_STATUS_INVALID_TX_KEY_NOT_PRESENT = 8503,
    NAN_I_STATUS_INVALID_NO_PEER_ENTRY = 8504,

    /* 9000-9500 NDP Status type */
    NDP_I_UNSUPPORTED_CONCURRENCY = 9000,
    NDP_I_NAN_DATA_IFACE_CREATE_FAILED = 9001,
    NDP_I_NAN_DATA_IFACE_DELETE_FAILED = 9002,
    NDP_I_DATA_INITIATOR_REQUEST_FAILED = 9003,
    NDP_I_DATA_RESPONDER_REQUEST_FAILED = 9004,
    NDP_I_INVALID_SERVICE_INSTANCE_ID = 9005,
    NDP_I_INVALID_NDP_INSTANCE_ID = 9006,
    NDP_I_INVALID_RESPONSE_CODE = 9007,
    NDP_I_INVALID_APP_INFO_LEN = 9008,
    /* OTA failures and timeouts during negotiation */
    NDP_I_MGMT_FRAME_REQUEST_FAILED = 9009,
    NDP_I_MGMT_FRAME_RESPONSE_FAILED = 9010,
    NDP_I_MGMT_FRAME_CONFIRM_FAILED = 9011,
    NDP_I_END_FAILED = 9012,
    NDP_I_MGMT_FRAME_END_REQUEST_FAILED = 9013,
    NDP_I_MGMT_FRAME_SECURITY_INSTALL_FAILED = 9014,

    /* 9500 onwards vendor specific error codes */
    NDP_I_VENDOR_SPECIFIC_ERROR = 9500
} NanInternalStatusType;

/* This is the TLV used for range report */
typedef struct PACKED
{
    u32 publish_id;
    u32 event_type;
    u32 range_measurement;
} NanFWRangeReportParams;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /*TLV Required:
        MANDATORY
            1. MAC_ADDRESS
            2. NanFWRangeReportParams
        OPTIONAL:
            1. A_UINT32 event type
    */
    u8 ptlv[1];
} NanFWRangeReportInd, *pNanFWRangeReportInd;

/** 2 word representation of MAC addr */
typedef struct {
    /** upper 4 bytes of  MAC address */
    u32 mac_addr31to0;
    /** lower 2 bytes of  MAC address */
    u32 mac_addr47to32;
} fw_mac_addr;

/* This is the TLV used to trigger ranging requests*/
typedef struct PACKED
{
    fw_mac_addr  range_mac_addr;
    u32 range_id; //Match handle in match_ind, publish_id in result ind
    u32 ranging_accept:1;
    u32 ranging_reject:1;
    u32 ranging_cancel:1;
    u32 reserved:29;
} NanFWRangeReqMsg, *pNanFWRangeReqMsg;

typedef struct PACKED
{
    fw_mac_addr  range_mac_addr;
    u32 range_id;//This will publish_id in case of receiving publish.
} NanFWRangeReqRecvdMsg, *pNanFWRangeReqRecvdMsg;

typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /*TLV Required
       1. t_nan_range_req_recvd_msg
    */
    u8 ptlv[1];
} NanFWRangeReqRecvdInd, *pNanFWRangeReqRecvdInd;

#define NIR_STR_LEN 3
#define NAN_MAX_HASH_LEN 32

static inline int is_zero_nan_identity_key(const u8 *buf)
{
    u8 zero[NAN_IDENTITY_KEY_LEN] = { 0 };

    return !memcmp(zero, buf, NAN_IDENTITY_KEY_LEN);
}

typedef struct PACKED {
    u32 cipher_version;
    u32 nonce_len;
    u32 tag_len;
    u8 nonce[NAN_IDENTITY_NONCE_LEN];
    u8 tag[NAN_IDENTITY_TAG_LEN];
} NanNIRARequest;

struct nanGrpKey {
    int cipher;
    u8 gtk[NAN_GTK_MAX_LEN];
    size_t gtk_len;
    u32 gtk_life_time;
    u8  gtk_rsc[NAN_MAX_GROUP_KEY_RSC_LEN];
    u8 igtk[NAN_IGTK_MAX_LEN];
    size_t igtk_len;
    u32 igtk_life_time;
    u8  igtk_rsc[NAN_MAX_GROUP_KEY_RSC_LEN];
    u8 bigtk[NAN_BIGTK_MAX_LEN];
    size_t bigtk_len;
    u32 bigtk_life_time;
    u8  bigtk_rsc[NAN_MAX_GROUP_KEY_RSC_LEN];
};

struct PACKED sharedKeyDesc {
    u8 attrID;
    u16 length;
    u8 publishID;
    u8 keyDescriptor[0];
};

struct PACKED keyDescriptor {
    u8 descriptorType;
    u16 keyInfo;
    u16 keyLength;
    u8 keyReplayCounter[8];
    u8 keyNonce[32];
    u8 eapolKeyIV[16];
    u8 keyRsc[8];
    u8 reserved[8];
    u8 keyMic[16];
    u16 keyDataLen;
    u8 keyData[0];
};

struct PACKED nanKDE {
    u8 type;
    u8 length;
    u8 oui[3];
    u8 dataType;
    u8 data[0];
};

struct PACKED nikKDE {
    u8 cipher;
    u8 nik_data[0];
};

struct PACKED nikLifetime {
    u32 lifetime;
};

struct PACKED igtkKDE {
        u8 keyid[2];
        u8 pn[6];
        u8 igtk[0];
};

#define NAN_BIGTK_KDE_PREFIX_LEN (2 + 6)
struct PACKED bigtkKDE {
        u8 keyid[2];
        u8 pn[6];
        u8 bigtk[0];
};

struct PACKED igtkLifetime {
       u32 lifetime;
};

struct PACKED bigtkLifetime {
       u32 lifetime;
};

typedef struct {
    /* Advertise shared key descriptor holding Group keys */
    u16 pub_sub_id;
    u32 requestor_instance_id;
    u8 peer_disc_mac_addr[NAN_MAC_ADDR_LEN];
    u16 shared_key_attr_len;
    u8 shared_key_attr[NAN_MAX_SHARED_KEY_ATTR_LEN];
} NanSharedKeyRequest, *pNanSharedKeyRequest;

/* Enumeration for NAN device current role */
enum secure_nan_role {
    SECURE_NAN_IDLE = 0,
    SECURE_NAN_BOOTSTRAPPING_INITIATOR,
    SECURE_NAN_BOOTSTRAPPING_RESPONDER,
    SECURE_NAN_PAIRING_INITIATOR,
    SECURE_NAN_PAIRING_RESPONDER,
};

/* This is nan identity key params of the device */
struct nanIDkey {
    /* AKMP used for NIK derviation */
    int akmp;
    /* cipher suite type */
    int cipher;
    /* NIK expiration time in seconds */
    int expiration;
    /* buffer to hold the NIK */
    u8 nik_data[NAN_IDENTITY_KEY_LEN];
    /* length of NIK */
    size_t nik_len;
    /* nonce used in NIRA attribute */
    u8 nira_nonce[NAN_IDENTITY_NONCE_LEN];
    /* length of nonce */
    size_t nira_nonce_len;
    /* tag computed for nonce using NIK */
    u8 nira_tag[NAN_IDENTITY_TAG_LEN];
    /* length of tag */
    size_t nira_tag_len;
};

/* This is data structure to hold PASN M1 frame.
 * It will be freed, when pairing indication response is received.
 */
struct pasn_auth_frame {
    /* buf to store PASN auth frame */
    u8 data[MAX_FRAME_LEN_80211_MGMT];
    /* length of frame */
    u32 len;
};

struct nan_grpkey_params {
    u16 key_cipher;
    u8 key_idx;
    u8 key_len;
    u8 key_data[NAN_MAX_GROUP_KEY_LEN];
    u8 rsc[NAN_MAX_GROUP_KEY_RSC_LEN];
};

struct nan_groupkey_info {
    u8 addr[NAN_MAC_ADDR_LEN];
    bool igtk_valid;
    bool bigtk_valid;
    struct nan_grpkey_params igtk;
    struct nan_grpkey_params bigtk;
};

/* This is nan pairing peer information.
 * This is an entry in the list of all pairing peers.
 */
struct nan_pairing_peer_info {
    /* list of pairing peers */
    struct list_head list;
#ifdef WPA_PASN_LIB
    /* pasn data required for authentication */
    struct pasn_data pasn;
#endif
    /* is trans_id valid */
    bool trans_id_valid;
    /* current transaction ID */
    transaction_id trans_id;
    /* publisg/subscribe ID received in auth frames */
    u16 pub_sub_id;
    /* requestor instance ID */
    u32 requestor_instance_id;
    /* bootstrapping instance ID for the peer */
    u32 bootstrapping_instance_id;
    /* pairing instance ID local to the device */
    u32 pairing_instance_id;
    /* ndp ID of latest instance */
    u32 ndp_instance_id;
    /* bssid of pairing peer */
    u8 bssid[NAN_MAC_ADDR_LEN];
    /* current role of the peer based on the handshake frame received */
    enum secure_nan_role peer_role;
    /* bootstrapping methods advertised by peer */
    u16 peer_supported_bootstrap;
    /* peer nan identity key. Valid for a successfully paired peer */
    u8 peer_nik[NAN_IDENTITY_KEY_LEN];
    /* life time of peer nik in seconds */
    u32 peer_nik_lifetime;
    /* passphrase length */
    size_t passphrase_len;
    /* passphrase */
    char *passphrase;
    /* sae password id to derive pt */
    char *sae_password_id;
    /* flag to check if pairing in progress with same peer */
    bool is_pairing_in_progress;
    /* flag to check if peer is paired */
    bool is_paired;
    /* capability info in DCEA attribute */
    u16 dcea_cap_info;
    /* publisher ID in CSIA attribute */
    u8 csia_pub_id;
    struct pasn_auth_frame *frame;
};

struct wpa_secure_nan {
    /* NAN device own address */
    u8 own_addr[NAN_MAC_ADDR_LEN];
    /* NAN cluster address */
    u8 cluster_addr[NAN_MAC_ADDR_LEN];
    /* pub sub ID of latest instance */
    u16 pub_sub_id;
    /* bootstrapping ID of latest instance */
    u32 bootstrapping_id;
    /* pairing ID of latest instance */
    u32 pairing_id;
    /* device capability to enable pairing setup */
    u32 enable_pairing_setup;
    /* device capability to enable pairing cache */
    u32 enable_pairing_cache;
    /* device supported bootstrapping */
    u16 supported_bootstrap;
    /* nan pairing ptksa cache list */
    struct ptksa_cache *ptksa;
    /* nan pairing initiator pmksa cache list */
    struct rsn_pmksa_cache *initiator_pmksa;
    /* nan pairing responder pmksa cache list */
    struct rsn_pmksa_cache *responder_pmksa;
    /* device nan identity key info */
    struct nanIDkey *dev_nik;
    /* device nan group key info */
    struct nanGrpKey *dev_grp_keys;
    /* nan pairing callback ctx, holds wifi_handle */
    void *cb_ctx;
    /* nan pairing callback iface name, holds interface name */
    char iface_name[IFNAMSIZ+1];
    /* list of pairing peers */
    struct list_head peers;
    /* pointer to rsne buffer */
    struct wpabuf *rsne;
    /* pointer to rsnxe buffer */
    struct wpabuf *rsnxe;
    /* group keys pn req bitmap */
    u8 pn_bitmap;
    /* peer for which skda send is pending */
    struct nan_pairing_peer_info* pending_peer;
};

/***************************************************
 * Wi-Fi HAL and Firmware interface for oem data
 ***************************************************/

#define NAN_OEM1_DATA_MAX_LEN  1024

/* NAN Command request */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    /* TLVs Required:
       MANDATORY
       1. command in byte format
    */
    u8 ptlv[];
} NanFWOemReqMsg, *pNanFWOemReqMsg;

/* NAN Command Rsp */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    u16 status;
    u16 value;
    u8 ptlv[];
} NanFWOemRspMsg, *pNanFWOemRspMsg;

/* NAN Event Ind */
typedef struct PACKED
{
    NanMsgHeader fwHeader;
    u16 reserved[2];
    u8 ptlv[];
} NanFWOemIndMsg, *pNanFWOemIndMsg;

/* Function for NAN error translation
   For NanResponse, NanPublishTerminatedInd, NanSubscribeTerminatedInd,
   NanDisabledInd, NanTransmitFollowupInd:
   function to translate firmware specific errors
   to generic freamework error along with the error string
*/
void NanErrorTranslation(NanInternalStatusType firmwareErrorRecvd,
                         u32 valueRcvd,
                         void *pRsp,
                         bool is_ndp_rsp);

/* nan pairing internal function prototypes */
int secure_nan_init(wifi_interface_handle iface);
int secure_nan_cache_flush(hal_info *info);
int secure_nan_deinit(hal_info *info);
void nan_pairing_set_nira(struct wpa_secure_nan *secure_nan);
unsigned int nan_pairing_get_nik_lifetime(struct nanIDkey *nik);
struct rsn_pmksa_cache *nan_pairing_initiator_pmksa_cache_init(void);
void nan_pairing_initiator_pmksa_cache_deinit(struct rsn_pmksa_cache *pmksa);
struct rsn_pmksa_cache *nan_pairing_responder_pmksa_cache_init(void);
void nan_pairing_responder_pmksa_cache_deinit(struct rsn_pmksa_cache *pmksa);
struct nan_pairing_peer_info*
nan_pairing_add_peer_to_list(struct wpa_secure_nan *secure_nan, u8 *mac);
struct nan_pairing_peer_info*
nan_pairing_get_peer_from_list(struct wpa_secure_nan *secure_nan, u8 *mac);
struct nan_pairing_peer_info*
nan_pairing_get_peer_from_id(struct wpa_secure_nan *secure_nan, u32 pairing_id);
struct nan_pairing_peer_info*
nan_pairing_get_peer_from_bootstrapping_id(struct wpa_secure_nan *secure_nan,
                                           u32 bootstrapping_id);
struct nan_pairing_peer_info*
nan_pairing_get_peer_from_ndp_id(struct wpa_secure_nan *secure_nan,
                                 u32 ndp_instance_id);
void nan_pairing_remove_peers_with_nik(hal_info *info, u8 *nik, u8 *skip_mac);
void nan_pairing_delete_list(struct wpa_secure_nan *secure_nan);
void nan_pairing_delete_peer_from_list(struct wpa_secure_nan *secure_nan,
                                       u8 *mac);
int nan_send_tx_mgmt(void *ctx, const u8 *frame_buf, size_t frame_len,
                     int noack, unsigned int freq, unsigned int wait_dur);
struct wpabuf *nan_pairing_generate_rsn_ie(int akmp, int cipher, u8 *pmkid);
struct wpabuf *nan_pairing_generate_rsnxe(int akmp);
const u8 *nan_attr_from_nan_ie(const u8 *nan_ie, enum nan_attr_id attr);
const u8 *nan_get_attr_from_ies(const u8 *ies, size_t ies_len,
                                enum nan_attr_id attr);
void nan_pairing_add_setup_ies(struct wpa_secure_nan *secure_nan,
                               struct pasn_data *pasn, int peer_role);
void nan_pairing_add_verification_ies(struct wpa_secure_nan *secure_nan,
                                      struct pasn_data *pasn, int peer_role);
int nan_pasn_kdk_to_ndp_pmk(const u8 *kdk, size_t kdk_len, const u8 *spa,
                            const u8 *bssid, u8 *ndp_pmk, u32 *ndp_pmk_len);
int nan_pasn_kdk_to_opportunistic_npk(const u8 *kdk, size_t kdk_len,
                                      const u8 *spa, const u8 *bssid,
                                      int akmp, int cipher, u8 *opp_npk,
                                      size_t *opp_npk_len);
int nan_pasn_kdk_to_nan_kek(const u8 *kdk, size_t kdk_len, const u8 *spa,
                            const u8 *bssid, int akmp, int cipher, u8 *nan_kek,
                            size_t *nan_kek_len);
int nan_pairing_validate_custom_pmkid(void *ctx, const u8 *bssid,
                                      const u8 *pmkid);
void nan_pairing_set_password(struct nan_pairing_peer_info *peer, u8 *passphrase,
                              u32 len);
void nan_pairing_notify_initiator_response(wifi_handle handle, u8 *bssid);
void nan_pairing_notify_responder_response(wifi_handle handle, u8 *bssid);
int nan_pairing_handle_pasn_auth(wifi_handle handle, const u8 *data, size_t len);
int nan_pairing_set_keys_from_cache(wifi_handle handle, u8 *src_addr, u8 *bssid,
                                    int cipher, int akmp, int role);
wifi_error nan_set_nira_request(transaction_id id, wifi_interface_handle iface,
                                const u8 *nan_identity_key);
wifi_error nan_sharedkey_followup_request(transaction_id id,
                                     wifi_interface_handle iface,
                                     NanSharedKeyRequest *msg);
wifi_error nan_validate_shared_key_desc(wifi_interface_handle iface,
                                        const u8 *addr, u8 *buf, u16 len);
wifi_error nan_get_shared_key_descriptor(hal_info *info, const u8 *addr,
                                         NanSharedKeyRequest *key);
int nan_pairing_initiator_pmksa_cache_add(struct rsn_pmksa_cache *pmksa,
                                          u8 *bssid, u8 *pmk, u32 pmk_len);
int nan_pairing_initiator_pmksa_cache_get(struct rsn_pmksa_cache *pmksa,
                                          u8 *bssid, u8 *pmkid);
void nan_pairing_initiator_pmksa_cache_flush(struct rsn_pmksa_cache *pmksa);
int nan_pairing_responder_pmksa_cache_add(struct rsn_pmksa_cache *pmksa,
                                          u8 *own_addr, u8 *bssid, u8 *pmk,
                                          u32 pmk_len);
int nan_pairing_responder_pmksa_cache_get(struct rsn_pmksa_cache *pmksa,
                                          u8 *bssid, u8 *pmkid);
void nan_pairing_responder_pmksa_cache_flush(struct rsn_pmksa_cache *pmksa);
void nan_pairing_derive_grp_keys(hal_info *info, u8* addr, u32 cipher_caps);
bool is_nira_present(struct wpa_secure_nan *secure_nan, const u8 *frame,
                     size_t len);
struct nan_pairing_peer_info*
nan_pairing_initialize_peer_for_verification(struct wpa_secure_nan *secure_nan,
                                             u8 *mac);
void nan_rx_mgmt_auth(wifi_handle handle, const u8 *frame, size_t len);
int nan_register_action_frames(wifi_interface_handle iface);
int nan_register_action_dual_protected_frames(wifi_interface_handle iface);
void nan_rx_mgmt_auth(wifi_handle handle, const u8 *frame, size_t len);
void nan_rx_mgmt_action(wifi_handle handle, const u8 *frame, size_t len);
int nan_handle_pn_response(wifi_handle handle, transaction_id id, u8 *key_rsc);
int nan_pairing_send_skda_data(wifi_interface_handle iface);
int nan_pairing_prepare_skda_data(wifi_interface_handle iface);
wifi_error nan_group_key_pn_request(transaction_id id,
                                    wifi_interface_handle iface,
                                    u32 key_index);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __NAN_I_H__ */

