/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc.All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "wifihal_list.h"
#include <hardware_legacy/wifi_hal.h>
#include "nan_i.h"
#include "nancommand.h"
#include "common.h"
#include "cpp_bindings.h"
#include <utils/Log.h>
#include <errno.h>

static const int nanPMKLifetime = 43200;
#define NAN_PAIRING_SSID "516F9A010000"

/* NAN Identity key lifetime in seconds */
static const int NIKLifetime = 43200;
/* NAN group key lifetime in seconds */
static const int GrpKeyLifetime = 43200;
#ifdef WPA_PASN_LIB
static int nan_pairing_set_key(hal_info *info, int alg, const u8 *addr,
                               int key_idx, int set_tx, const u8 *seq,
                               size_t seq_len, const u8 *key, size_t key_len,
                               int key_flag);
wifi_error nan_pairing_set_group_key(transaction_id id,
                                     wifi_interface_handle iface,
                                     struct nan_groupkey_info *info);
#endif

static u16 sda_get_service_info_offset(const u8 *buf, size_t buf_len, u8 window)
{
    u8 attr_id;
    u16 attr_len, len;
    u8 service_ctrl;
    u16 offset = 0;

    if (!buf || buf_len < 3) {
        ALOGI("Invalid attribute buffer");
        return offset;
    }

    attr_id = *buf++;
    buf_len--;
    if (NAN_ATTR_ID_SERVICE_DESCRIPTOR != attr_id) {
        ALOGE("Invalid attribute ID %u", attr_id);
        return offset;
    }

    attr_len = WPA_GET_LE16(buf);
    buf_len -= 2;
    buf += 2;

    if (window == NAN_WINDOW_DW) {
        /* -3 because id and length bytes are not included in attr_len. */
        if ((attr_len < (NAN_SD_ATTR_MIN_LEN-3)) ||
           (attr_len > NAN_SD_ATTR_MAX_LEN)) {
            ALOGE("Invalid attribute length %u", attr_len);
            return offset;
        }
    }

    if (buf_len < (NAN_SD_ATTR_SERVICE_ID_LEN + 3)) {
        ALOGE("SDA buffer too short %d", buf_len);
        return offset;
    }
    buf += NAN_SD_ATTR_SERVICE_ID_LEN + 2;
    buf_len -= NAN_SD_ATTR_SERVICE_ID_LEN + 2;

    service_ctrl = *buf++;
    buf_len--;

    if (!(service_ctrl & NAN_SVC_CTRL_FLAG_SERVICE_INFO))
        return offset;

    offset = NAN_SD_ATTR_MIN_LEN;

    if ((service_ctrl & NAN_SVC_CTRL_FLAG_BINDING_BITMAP) &&
        buf_len >= 2) {
        offset += 2;
        buf += 2;
        buf_len -= 2;
    }

    if ((service_ctrl & NAN_SVC_CTRL_FLAG_MATCH_FILTER) &&
        buf_len > 0) {
        len = *buf++;
        buf_len--;
        if (buf_len < len)
            return offset;
        offset += 1 + len;
        buf_len -= len;
        buf += len;
    }

    if ((service_ctrl & NAN_SVC_CTRL_FLAG_SERVICE_RSP) &&
        buf_len > 0) {
        len = *buf;
        buf_len--;
        if (buf_len < len)
            return offset;
        offset += 1 + *buf;
    }

    ALOGI("Service Info offset is %d", offset);
    return offset;
}

static bool is_sda_valid(const u8 *buf, size_t buf_len)
{
    u8 serviceCtrlFlags;
    u16 attr_len, len, i;
    u8 lenoffset = 1;

    if (!buf || buf_len < 3) {
        ALOGE("%s: Invalid attribute buffer", __FUNCTION__);
        return false;
    }

    attr_len = WPA_GET_LE16(buf + 1);
    buf_len -= 3;
    buf += 3; // Skip attribute ID and length bytes

    ALOGI("%s: Validate SD attribute length %d", __FUNCTION__, attr_len);

    if (attr_len < NAN_SD_ATTR_MIN_LEN - 3 || buf_len < attr_len) {
        ALOGE("%s: Invalid attribute length attr_len %d, buf_len %d",
              __FUNCTION__, attr_len, buf_len);
        return false;
    }

    /* Skip the service, instance and requestor IDs. */
    buf += NAN_SD_ATTR_SERVICE_ID_LEN + 2;
    attr_len -= NAN_SD_ATTR_SERVICE_ID_LEN + 2;

    serviceCtrlFlags = *buf++;
    attr_len--;

    ALOGI("%s: serviceCtrlFlags %x", __FUNCTION__, serviceCtrlFlags);

    /* Parse the binding bitmap if necessary. */
    if (serviceCtrlFlags & NAN_SVC_CTRL_FLAG_BINDING_BITMAP) {
        if (attr_len < 2)
            return false;
        buf += 2;
        attr_len -= 2;
    }

    /* Parse the match filter if necessary. */
    if (serviceCtrlFlags & NAN_SVC_CTRL_FLAG_MATCH_FILTER) {
        if (attr_len < 1)
            return false;
        len = *buf++;
        if (attr_len < len + 1)
            return false;
        buf += len;
        attr_len -= (len + 1);
    }

    /* Parse the service resopnse filter if necessary. */
    if (serviceCtrlFlags & NAN_SVC_CTRL_FLAG_SERVICE_RSP) {
        if (attr_len < 1)
            return false;
        len = *buf++;
        if (attr_len < len + 1)
            return false;
        buf += len;
        attr_len -= (len + 1);
    }

    /* Parse the service-specific info if necessary. */
    if (serviceCtrlFlags & NAN_SVC_CTRL_FLAG_SERVICE_INFO)
    {
        if (attr_len < 1)
            return false;
        len = *buf++;

        if (attr_len < len + lenoffset)
            return false;
        buf += len;
        /* +1 for len field - length field is 1 byte for SSI in NAN spec */
        attr_len -= (len + lenoffset);
    }

    if (attr_len != 0) {
        ALOGE("Attribute data exceeds by %d bytes", attr_len);
        return false;
    }
    return true;
}

static bool is_sdea_valid(const u8 *buf, size_t buf_len)
{
    u16 attr_len;
    u16 service_info_len;
    u16 sdea_control;

    if (!buf || buf_len < 3) {
        ALOGE("%s: Invalid attribute", __FUNCTION__);
        return false;
    }

    attr_len = WPA_GET_LE16(buf + 1);
    buf_len -= 3;
    buf += 3; // Skip attribute ID and length bytes

    ALOGI("Validate SDE attribute, length %d", attr_len);

    if (attr_len < NAN_SDE_ATTR_MIN_LEN || buf_len < attr_len) {
        ALOGE("%s: Invalid attribute length in SDEA attr_len %d, buf_len %d",
              __FUNCTION__, attr_len, buf_len);
        return false;
    }

    sdea_control = WPA_GET_LE16(buf + 1);
    buf += 3;
    attr_len -= 3;

    if (sdea_control & BIT(NAN_SDE_ATTR_CTRL_RANGE_LIMIT_OFFSET)) {
        if (attr_len < 4) {
            ALOGE("Invalid attribute length in SDEA range_limit");
            return false;
        }
        buf += 4;
        attr_len -= 4;
    }

    if (sdea_control & BIT(NAN_SDE_ATTR_CTRL_SERVICE_UPDATE_INDI_PRESENT)) {
        if (attr_len < 1) {
            ALOGE("Invalid attribute length in SDEA service_update_indicator");
            return false;
        }
        buf++;
        attr_len--;
    }

    /* If attr_len is still > 0, it implies Service Info Length and Service Info field are present */
    if (attr_len > 0) {
        if (attr_len < 2) {
            ALOGE("Invalid attribute length in SDEA Service Info Length");
            return false;
        }
        service_info_len = WPA_GET_LE16(buf);
        buf += 2;
        attr_len -= 2;

        if (attr_len < service_info_len) {
            ALOGE("Invalid attribute length in SDEA Service Info field");
            return false;
        }
    }
    return true;
}

static int nan_get_npba_attr(const u8* buf, size_t buf_len,
                             NanFWBootstrappingParams* npba, u8 *cookie,
                             u16 *cookie_len)
{
    u16 attr_len;
    u8 type_status = 0;

    if (!buf || buf_len < 3) {
        ALOGE("%s: Invalid attribute", __FUNCTION__);
        return -1;
    }

    attr_len = WPA_GET_LE16(buf + 1);
    buf_len -= 3;
    buf += 3; // Skip attribute ID and length bytes

    if (attr_len < NAN_NPBA_ATTR_MIN_LEN - 3 || buf_len < attr_len) {
        ALOGE("Invalid attribute length in NPBA");
        return -1;
    }

    npba->dialog_token = *buf++;
    attr_len--;

    type_status = *buf++;
    attr_len--;
    npba->type = type_status & 0x0F;
    npba->status = (type_status & 0xF0) >> 4;

    npba->reason_code = *buf++;
    attr_len--;

    /* Check if Comeback field is present */
    if ((npba->type == NAN_BS_TYPE_REQUEST ||
         npba->type == NAN_BS_TYPE_RESPONSE) &&
        npba->status == NAN_BS_STATUS_COMEBACK) {
        if (npba->type == NAN_BS_TYPE_RESPONSE) {
            if (attr_len < 2) {
                ALOGE("Invalid attribute length in Comeback field #1");
                return -1;
            }
            npba->comeback_after = WPA_GET_LE16(buf);
            attr_len -= 2;
        }

        /* Cookie length should be present */
        if (attr_len < 1) {
            ALOGE("Invalid attribute length in Comeback field #2");
            return -1;
        }
        *cookie_len = *buf++;
        attr_len--;

        if (attr_len < *cookie_len) {
            ALOGE("Invalid attribute length in Comeback field #3");
            return -1;
        }
        memcpy(cookie, buf, *cookie_len);
        buf += *cookie_len;
        attr_len -= *cookie_len;
    }

    if (attr_len < 2) {
        ALOGE("Invalid attribute length in NPBA #4");
        return -1;
    }

    /* Get the Pairing Bootstrapping Method */
    npba->bootstrapping_method_bitmap = WPA_GET_LE16(buf);

    ALOGI("NPBA: Parse dialog: %d type: %d, status: %d reason: %d comeaback: %d "
          " boostrapping: %d", npba->dialog_token, npba->type, npba->status,
          npba->reason_code, npba->comeback_after,
          npba->bootstrapping_method_bitmap);

    return 0;
}

static bool nan_get_sde_attr(u8 *frame, u16 frame_len, nan_sdea *sdea)
{
    u16 service_info_len;

    if (!sdea || !frame || (frame_len < NAN_SDE_ATTR_MIN_LEN)) {
        ALOGE("%s: Incorrect arguments 0x%x 0x%x or length %d",
              __FUNCTION__, sdea, frame, frame_len);
        return false;
    }

    sdea->instance_id = *frame++;
    frame_len--;

    sdea->sdea_control = WPA_GET_LE16(frame);
    frame += 2;
    frame_len -= 2;

    if (sdea->sdea_control & BIT(NAN_SDE_ATTR_CTRL_RANGE_LIMIT_OFFSET)) {
        if (frame_len < 4)
            return false;
        sdea->range_limit_ingress = WPA_GET_LE16(frame);
        sdea->range_limit_egress = WPA_GET_LE16(frame + 2);
        frame += 4;
        frame_len -= 4;
    }

    if (sdea->sdea_control & BIT(NAN_SDE_ATTR_CTRL_SERVICE_UPDATE_INDI_PRESENT)) {
        if (frame_len < 1)
            return false;
        sdea->service_update_indicator = *frame++;
        frame_len--;
    }

    /* Check if service info length and service info field is present. */
    if (frame_len >= 2) {
        service_info_len = WPA_GET_LE16(frame);
        frame_len -= 2;
        frame += 2;

        if (frame_len >= service_info_len &&
            service_info_len >= NAN_SDE_ATTR_SERVICE_INFO_HEADER_LEN) {
            frame += NAN_SDE_ATTR_SERVICE_INFO_HEADER_LEN;
            sdea->ssi_len = service_info_len - NAN_SDE_ATTR_SERVICE_INFO_HEADER_LEN;

            if (sdea->ssi_len <= NAN_FOLLOWUP_MAX_EXT_SERVICE_SPECIFIC_INFO_LEN) {
                memcpy(sdea->ssi, frame, sdea->ssi_len);
                return true;
            } else {
                ALOGE("SDEA data size exceeds max extended SSI");
                return false;
            }
        }
    }
    return true;
}

void nan_process_followup_frame(wifi_handle handle, const u8 *buf,
                                size_t len, const u8 *mac)
{
    NanCommand *nanCommand;
    nan_sda *sd_attr;
    nan_sdea sde_attr;
    u8 npba_valid = 0;
    u32 match_handle = 0;
    u16 service_info_offset;
    u16 followup_ind_size;
    u8 *pos, *ptlv, *temptlv;
    u8 *sdea_attr_temp;
    u16 sdea_attr_len;
    u8 i, j, sda_count = 0, sdea_count = 0;
    u8 *sda[NAN_MAX_SD_ATTRS_PER_FRAME];
    size_t sda_len[NAN_MAX_SD_ATTRS_PER_FRAME];
    u8 *sdea[NAN_MAX_SD_ATTRS_PER_FRAME];
    u16 skd_len = 0, cookie_len = 0, msg_len;
    u8 skd_data[NAN_MAX_SHARED_KEY_DESC_ATTR_LEN];
    u8 cookie[NAN_MAX_BOOTSTRAPPING_COOKIE_LEN];
    NanFollowupIndMsg *followInd;
    NanFWBootstrappingParams npba;

    nanCommand = NanCommand::instance(handle);
    if (nanCommand == NULL) {
        ALOGE("%s: Error NanCommand NULL", __FUNCTION__);
        return;
    }

    if (!nanCommand->isNanEnabled()) {
        ALOGE("%s: Dropping followup rx frame from " MACSTR,
              __FUNCTION__, MAC2STR(mac));
        return;
    }

    memset(&npba, 0, sizeof(NanFWBootstrappingParams));

    if (len < 5) {
        ALOGE("%s: Frame length too short %d", __FUNCTION__, len);
        return;
    }
    pos = (u8 *)buf + 4;
    len -= 4;
    memset(&sda[0], 0, sizeof(u8*) * NAN_MAX_SD_ATTRS_PER_FRAME);
    memset(&sdea[0], 0, sizeof(u8*) * NAN_MAX_SD_ATTRS_PER_FRAME);

    while (len > 3) {
        u8 *attr = pos;
        u8 attrId = *pos;
        u16 attrLen = WPA_GET_LE16(pos + 1);

        if (!attrLen || len < (attrLen + 3)) {
            ALOGE("%s: SDF Invalid Frame: framelen = %d attrId = 0x%x attrlen = %d",
                  __FUNCTION__, len, attrId, attrLen);
            return;
        }
        pos += (attrLen + 3);
        len -= (attrLen + 3);

        switch (attrId)
        {
            case NAN_ATTR_ID_SERVICE_DESCRIPTOR:
                if (!is_sda_valid(attr, attrLen + 3)) {
                    ALOGE("Invalid SD attribute: attr_len = %d", attrLen);
                    return;
                }
                if (sda_count < NAN_MAX_SD_ATTRS_PER_FRAME) {
                    sda[sda_count] = attr;
                    sda_len[sda_count++] = attrLen + 3;
                } else {
                    ALOGE("SDA count exceeds max SD attribute: %d", sda_count);
                    return;
                }
                break;

            case NAN_ATTR_ID_SDE:
                if (!is_sdea_valid(attr, attrLen + 3)) {
                    ALOGE("Invalid SDE attribute: attr_len = %d", attrLen);
                    return;
                }
                if (sdea_count < NAN_MAX_SD_ATTRS_PER_FRAME)
                    sdea[sdea_count++] = attr;
                else {
                    ALOGE("SDEA count exceeds max SD attribute: %d", sdea_count);
                    return;
                }
                break;

            case NAN_ATTR_ID_NPBA:
                if (!nan_get_npba_attr(attr, attrLen + 3, &npba, cookie,
                    &cookie_len))
                    npba_valid = 1;
                break;

            case NAN_ATTR_ID_SHARED_KEY_DESC:
                if (((attrLen + 3) >= NAN_MAX_SHARED_KEY_DESC_ATTR_LEN) ||
                    (attrLen < 1))
                {
                    ALOGE("Invalid Shared Key descriptor: attr_len = %d", attrLen);
                    return;
                }
                memcpy(skd_data, attr, attrLen + 3);
                skd_len = attrLen + 3;
                break;

            default:
                break;
        }
    }

    for (i = 0; i < sda_count; ++i) {
        sd_attr = (nan_sda *)sda[i];
        memset(&sde_attr, 0, sizeof(nan_sdea));
        for (j = 0; j < sdea_count; ++j) {
            sdea_attr_temp = sdea[j];
            sdea_attr_temp += NAN_SDE_ATTR_LEN_OFFSET;
            sdea_attr_len = WPA_GET_LE16(sdea_attr_temp);
            /* NAN_SDE_ATTR_OFFSET_INSTANCE_ID */
            if (sd_attr->instance_id == *(sdea_attr_temp + 2)) {
                if (!nan_get_sde_attr((sdea_attr_temp + 2), sdea_attr_len,
                                      &sde_attr)) {
                    ALOGE("Incorrect SD extended attribute");
                    return;
                }
            }
        }

        if (sde_attr.ssi_len > NAN_MAX_SERVICE_SPECIFIC_INFO_LEN)
            followup_ind_size = NAN_MAX_FOLLOWUP_IND_SIZE_EXT_SSI;
        else
            followup_ind_size = NAN_MAX_FOLLOWUP_IND_SIZE;

        u8 *eventbuf = (u8 *)malloc(followup_ind_size);
        if (!eventbuf) {
            ALOGE("%s: Memory allocation failed", __FUNCTION__);
            return;
        }

        memset(eventbuf, 0, followup_ind_size);
        followInd = (NanFollowupIndMsg *)(eventbuf);

        followInd->fwHeader.msgVersion = 1;
        followInd->fwHeader.msgId = NAN_MSG_ID_FOLLOWUP_IND;
        followInd->fwHeader.handle = sd_attr->requestor_id;

        if (sd_attr->requestor_id < 1 ||
            (sd_attr->requestor_id > 6 && sd_attr->requestor_id < 128) ||
             (sd_attr->requestor_id > 133)) {
            ALOGE("SDF Followup invalid requestor_id");
            free(eventbuf);
            return;
        }

        match_handle = nanCommand->getNanMatchHandle(sd_attr->requestor_id,
                                                     sd_attr->service_id, mac);

        if (match_handle)
            followInd->followupIndParams.matchHandle = match_handle;
        else
            followInd->followupIndParams.matchHandle =
                                 (sd_attr->instance_id << 24) | 0x0000FFFF;

        ptlv = followInd->ptlv;
        temptlv = ptlv;
        msg_len = 0;

        ptlv = addTlv(NAN_TLV_TYPE_MAC_ADDRESS, NAN_MAC_ADDR_LEN,
                      mac, ptlv);
        msg_len += ptlv - temptlv;
        temptlv = ptlv;

        service_info_offset = sda_get_service_info_offset(sda[i], sda_len[i],
                                                          NAN_WINDOW_DW);
        if (service_info_offset &&
            (service_info_offset + 1 < NAN_SD_ATTR_MAX_LEN)) {
            ptlv = addTlv(NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO,
                          *(sda[i] + service_info_offset),
                          sda[i] + (service_info_offset + 1), ptlv);
            msg_len += ptlv - temptlv;
            temptlv = ptlv;
        }

        if (sde_attr.ssi_len > 0) {
            ptlv = addTlv(NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO,
                          sde_attr.ssi_len, sde_attr.ssi, ptlv);
            msg_len += ptlv - temptlv;
            temptlv = ptlv;
        }

        if (npba_valid) {
            ptlv = addTlv(NAN_TLV_TYPE_BOOTSTRAPPING_PARAMS,
                          sizeof(NanFWBootstrappingParams),
                          (u8 *)&npba, ptlv);
            msg_len += ptlv - temptlv;
            temptlv = ptlv;

            if (cookie_len){
                ptlv = addTlv(NAN_TLV_TYPE_BOOTSTRAPPING_COOKIE, cookie_len,
                              cookie, ptlv);
                msg_len += ptlv - temptlv;
                temptlv = ptlv;
            }
        }

        if (skd_len) {
            ptlv = addTlv(NAN_TLV_TYPE_NAN_SHARED_KEY_DESC_ATTR, skd_len,
                          (u8 *)skd_data, ptlv);
            msg_len += ptlv - temptlv;
            temptlv = ptlv;
        }

        followInd->fwHeader.msgLen = sizeof(NanMsgHeader) +
                                     sizeof(NanFollowupIndParams) + msg_len;
        nanCommand->setNanVendorEventAndDataLen((char *)(eventbuf),
                                                followInd->fwHeader.msgLen);
        nanCommand->handleNanRx();

        free(eventbuf);
    }
    return;
}

void nan_rx_mgmt_action(wifi_handle handle, const u8 *frame, size_t len)
{
    size_t plen;
    u8 category;
    const u8 *payload;
    const struct ieee80211_hdr *hdr = (const struct ieee80211_hdr *)frame;

    if (len < IEEE80211_HDRLEN + 5)
        return;

    plen = len - IEEE80211_HDRLEN - 1;
    payload = frame + IEEE80211_HDRLEN;
    category = *payload++;

    if ((category == WLAN_ACTION_PUBLIC ||
        category == WLAN_ACTION_PROTECTED_DUAL) && plen >= 5 &&
        payload[0] == WLAN_PA_VENDOR_SPECIFIC &&
        WPA_GET_BE24(&payload[1]) == OUI_WFA &&
        payload[4] == NAN_MSG_ID_FOLLOWUP_IND) {
        payload++;
        plen--;
        nan_process_followup_frame(handle, payload, plen, hdr->addr2);
        return;
    }
    return;
}

static int nan_send_nl_msg_event_sock(hal_info *info, struct nl_msg *msg)
{
    int res = 0;
    struct nl_cb * cb = NULL;

    if (!info->event_sock) {
        ALOGE("event socket is null");
        return -1;
    }

    /* send message */
    res = nl_send_auto_complete(info->event_sock, msg);
    if (res < 0) {
           ALOGE("%s: send msg failed. err = %d",__func__, res);
           return res;
    }

    cb = nl_socket_get_cb(info->event_sock);

    /* err is populated as part of finish_handler */
    while (res > 0)
        res = nl_recvmsgs(info->event_sock, cb);

    nl_cb_put(cb);
    return res;
}

static int nan_register_frames(wifi_interface_handle iface, u16 type,
                               const u8 *frame_match, size_t match_len)
{
    u32 idx;
    struct nl_msg * msg;
    wifi_handle wifiHandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifiHandle);

    msg = nlmsg_alloc();
    if (!msg) {
        ALOGE("%s: nlmsg malloc failed", __FUNCTION__);
        return -1;
    }

    genlmsg_put(msg, 0, 0, info->nl80211_family_id, 0, 0,
                NL80211_CMD_REGISTER_FRAME, 0);

    idx = if_nametoindex(DEFAULT_NAN_IFACE);
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx);

    nla_put_u16(msg, NL80211_ATTR_FRAME_TYPE, type);
    nla_put(msg, NL80211_ATTR_FRAME_MATCH, match_len, frame_match);

    nan_send_nl_msg_event_sock(info, msg);

    if (msg)
        nlmsg_free(msg);

    return 0;
}

int nan_register_action_frames(wifi_interface_handle iface)
{
    /* wlan type:mgmt, wlan subtype: action */
    u16 type = (WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_ACTION << 4);
    /* register for Public Action frames */
    const u8 nan_action_match[6] = {0x04, 0x09, 0x50, 0x6f, 0x9a, 0x13};

    return nan_register_frames(iface, type, nan_action_match, 6);
}

int nan_register_action_dual_protected_frames(wifi_interface_handle iface)
{
    /* wlan type:mgmt, wlan subtype: action */
    u16 type = (WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_ACTION << 4);
    /* register for Public Action Dual Protected frames */
    const u8 nan_action_dual_match[6] = {0x09, 0x09, 0x50, 0x6f, 0x9a, 0x13};

    return nan_register_frames(iface, type, nan_action_dual_match, 6);
}

#ifdef WPA_PASN_LIB
void nan_rx_mgmt_auth(wifi_handle handle, const u8 *frame, size_t len)
{
    int ret = 0;
    const u8 *nan_attr_ie;
    struct pasn_data *pasn;
    hal_info *info = getHalInfo(handle);
    struct wpa_pasn_params_data pasn_data;
    struct nan_pairing_peer_info *peer;
    struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *) frame;
    NanCommand *nanCommand = NULL;

    nanCommand = NanCommand::instance(handle);
    if (nanCommand == NULL) {
        ALOGE("%s: Error NanCommand NULL", __FUNCTION__);
        return;
    }

    if (!info || !info->secure_nan) {
        ALOGE("%s: secure nan NULL", __FUNCTION__);
        return;
    }

    if (!mgmt || len < offsetof(struct ieee80211_mgmt, u.auth.variable)) {
        ALOGE("%s: Incorrect frame length", __FUNCTION__);
        return;
    }

    peer = nan_pairing_get_peer_from_list(info->secure_nan, mgmt->sa);
    if (!peer) {
        if (is_nira_present(info->secure_nan, frame, len))
            peer = nan_pairing_initialize_peer_for_verification(info->secure_nan,
                                                                mgmt->sa);
    }

    if (!peer) {
        ALOGE("nl80211: Peer not found in the pairing list");
        return;
    }

    pasn = peer->pasn;

    ALOGI("nl80211: RX AUTH frame da=" MACSTR " sa=" MACSTR " bssid=" MACSTR
          " seq_ctrl=0x%x len=%u",
          MAC2STR(mgmt->da), MAC2STR(mgmt->sa), MAC2STR(mgmt->bssid),
          le_to_host16(mgmt->seq_ctrl), (unsigned int) len);

    if (peer->peer_role == SECURE_NAN_PAIRING_RESPONDER) {
        if (os_memcmp(mgmt->da, info->secure_nan->own_addr, ETH_ALEN) != 0) {
            ALOGE(" %s Pairing Initiator: Not our frame", __FUNCTION__);
            return;
        }

        ret = wpa_pasn_auth_rx(pasn, frame, len, &pasn_data);
        if (ret == 0) {
            nan_attr_ie = nan_get_attr_from_ies(mgmt->u.auth.variable,
                             len - offsetof(struct ieee80211_mgmt, u.auth.variable),
                             NAN_ATTR_ID_DCEA);
            if (nan_attr_ie) {
               nan_dcea *dcea = (nan_dcea *)nan_attr_ie;
               peer->dcea_cap_info = dcea->cap_info;
            }

            nan_attr_ie = nan_get_attr_from_ies(mgmt->u.auth.variable,
                             len - offsetof(struct ieee80211_mgmt, u.auth.variable),
                             NAN_ATTR_ID_CSIA);
            if (nan_attr_ie) {
                nan_csia *csia = (nan_csia *)nan_attr_ie;
                peer->csia_cap_info = csia->caps;
            }
            ptksa_cache_add(info->secure_nan->ptksa, info->secure_nan->own_addr,
                            peer->bssid, pasn_get_cipher(pasn), nanPMKLifetime,
                            pasn_get_ptk(pasn), NULL, NULL, pasn_get_akmp(pasn));
            memset(pasn_get_ptk(pasn), 0, sizeof(struct wpa_ptk));
        } else if (ret == -1 || mgmt->u.auth.status_code) {
            NanPairingConfirmInd evt;

            memset(&evt, 0, sizeof(NanPairingConfirmInd));
            evt.pairing_instance_id = peer->pairing_instance_id;
            evt.rsp_code = NAN_PAIRING_REQUEST_REJECT;
            evt.reason_code = NAN_STATUS_PROTOCOL_FAILURE;
            evt.enable_pairing_cache = 0;
            nanCommand->handleNanPairingConfirm(&evt);

            peer->is_paired = false;
            peer->is_pairing_in_progress = false;
            wpa_pasn_reset(pasn);
            ALOGE(" %s wpa_pasn_auth_rx failed", __FUNCTION__);
            peer->peer_role = SECURE_NAN_IDLE;
        }
    } else {
       nan_pairing_handle_pasn_auth(handle, frame, len);
    }
}

struct nan_pairing_peer_info*
nan_pairing_add_peer_to_list(struct wpa_secure_nan *secure_nan, u8 *mac)
{
    struct nan_pairing_peer_info *entry, *mentry = NULL;

    list_for_each_entry(entry, &secure_nan->peers, list) {

       if (memcmp(entry->bssid, mac, ETH_ALEN) == 0) {
           if (entry->is_paired) {
               ALOGV(" %s :Peer already paired: ADDR=" MACSTR,
                     __FUNCTION__, MAC2STR(mac));
           } else {
               ALOGV(" %s :Add peer req for existing peer: ADDR=" MACSTR,
                     __FUNCTION__, MAC2STR(mac));
           }
           entry->pairing_instance_id = secure_nan->pairing_id++;
           pasn_register_callbacks(entry->pasn, secure_nan->cb_ctx,
                                   nan_send_tx_mgmt,
                                   nan_pairing_validate_custom_pmkid);
           return entry;
       }
    }

    mentry = (struct nan_pairing_peer_info *)malloc(sizeof(*entry));
    if (!mentry) {
        ALOGE("%s: peer entry malloc failed", __FUNCTION__);
        return NULL;
    }

    memset((char *)mentry, 0, sizeof(*entry));
    memcpy(mentry->bssid, mac, ETH_ALEN);
    mentry->pairing_instance_id = secure_nan->pairing_id++;

    mentry->pasn = pasn_data_init();
    if (!mentry->pasn) {
        ALOGE("%s: pasn data init failed", __FUNCTION__);
        free(mentry);
        return NULL;
    }

    pasn_register_callbacks(mentry->pasn, secure_nan->cb_ctx, nan_send_tx_mgmt,
                            nan_pairing_validate_custom_pmkid);
    wpa_pasn_reset(mentry->pasn);
    add_to_list(&mentry->list, &secure_nan->peers);
    return mentry;
}

struct nan_pairing_peer_info*
nan_pairing_get_peer_from_list(struct wpa_secure_nan *secure_nan, u8 *mac)
{
    struct nan_pairing_peer_info *entry;

    list_for_each_entry(entry, &secure_nan->peers, list) {
       if (memcmp(entry->bssid, mac, ETH_ALEN) == 0)
                  return entry;
    }
    return NULL;
}

struct nan_pairing_peer_info*
nan_pairing_get_peer_from_id(struct wpa_secure_nan *secure_nan, u32 pairing_id)
{
    struct nan_pairing_peer_info *entry;

    list_for_each_entry(entry, &secure_nan->peers, list) {
       if (entry->pairing_instance_id == pairing_id)
           return entry;
    }
    return NULL;
}

struct nan_pairing_peer_info*
nan_pairing_get_peer_from_bootstrapping_id(struct wpa_secure_nan *secure_nan,
                                           u32 bootstrapping_id)
{
    struct nan_pairing_peer_info *entry;

    list_for_each_entry(entry, &secure_nan->peers, list) {
       if (entry->bootstrapping_instance_id == bootstrapping_id)
           return entry;
    }
    return NULL;
}

struct nan_pairing_peer_info*
nan_pairing_get_peer_from_ndp_id(struct wpa_secure_nan *secure_nan,
                                 u32 ndp_instance_id)
{
    struct nan_pairing_peer_info *entry;

    list_for_each_entry(entry, &secure_nan->peers, list) {
       if (entry->ndp_instance_id == ndp_instance_id)
           return entry;
    }
    return NULL;
}

static void nan_pairing_delete_peer(struct nan_pairing_peer_info *peer)
{
    del_from_list(&peer->list);

    if (peer->passphrase)
        free(peer->passphrase);
    wpa_pasn_reset(peer->pasn);
    pasn_data_deinit(peer->pasn);

    if (peer->frame)
        free(peer->frame);

    free(peer);
}

void nan_pairing_remove_peers_with_nik(hal_info *info, u8 *nik, u8 *skip_mac)
{
    struct nan_pairing_peer_info *entry, *tmp;

    list_for_each_entry_safe(entry, tmp, &info->secure_nan->peers, list) {

       if (memcmp(entry->peer_nik, nik, NAN_IDENTITY_KEY_LEN) == 0) {

           if (skip_mac && memcmp(entry->bssid, skip_mac, ETH_ALEN) == 0)
               continue;

           nan_pairing_set_key(info, WPA_ALG_NONE, entry->bssid, 0, 0, NULL, 0,
                               NULL, 0, KEY_FLAG_PAIRWISE);
           nan_pairing_delete_peer(entry);
       }
    }
}

void nan_pairing_delete_list(struct wpa_secure_nan *secure_nan)
{
    struct nan_pairing_peer_info *entry, *tmp;

    list_for_each_entry_safe(entry, tmp, &secure_nan->peers, list) {
         nan_pairing_delete_peer(entry);
    }
}

void nan_pairing_delete_peer_from_list(struct wpa_secure_nan *secure_nan,
                                       u8 *mac)
{
    struct nan_pairing_peer_info *entry, *tmp;

    list_for_each_entry_safe(entry, tmp, &secure_nan->peers, list) {
       if (memcmp(entry->bssid, mac, ETH_ALEN) == 0) {
           nan_pairing_delete_peer(entry);
           return;
       }
    }
}

bool is_nira_present(struct wpa_secure_nan *secure_nan, const u8 *frame,
                     size_t len)
{
    u16 auth_alg, auth_transaction;
    const struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *) frame;

    if (!mgmt) {
        ALOGE("%s: PASN mgmt frame NULL", __FUNCTION__);
        return false;
    }

    if (os_memcmp(mgmt->da, secure_nan->own_addr, NAN_MAC_ADDR_LEN) != 0) {
        ALOGE("PASN Responder: Not our frame");
        return false;
    }

    auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
    auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);

    if (auth_alg == WLAN_AUTH_PASN && auth_transaction == 1  &&
        nan_get_attr_from_ies(mgmt->u.auth.variable,
                        len - offsetof(struct ieee80211_mgmt, u.auth.variable),
                        NAN_ATTR_ID_NIRA)) {
        ALOGV("%s: NIRA present", __FUNCTION__);
        return true;
    }
    return false;
}

struct nan_pairing_peer_info*
nan_pairing_initialize_peer_for_verification(struct wpa_secure_nan *secure_nan,
                                             u8 *mac)
{
  struct nan_pairing_peer_info* entry;

  entry = nan_pairing_add_peer_to_list(secure_nan, mac);
  if (entry == NULL) {
      ALOGE("%s: peer not available", __FUNCTION__);
      return NULL;
  }
  entry->peer_role = SECURE_NAN_PAIRING_INITIATOR;
  entry->pub_sub_id = secure_nan->pub_sub_id;
  entry->is_paired = true;
  return entry;
}

/* callback handlers registered for nl message send */
static int error_handler_nan(struct sockaddr_nl *nla, struct nlmsgerr *err,
                         void *arg)
{
    struct sockaddr_nl * tmp;
    int *ret = (int *)arg;
    tmp = nla;
    *ret = err->error;
    ALOGE("%s: Error code:%d (%s)", __func__, *ret, strerror(-(*ret)));
    return NL_STOP;
}

/* callback handlers registered for nl message send */
static int ack_handler_nan(struct nl_msg *msg, void *arg)
{
    int *ret = (int *)arg;
    struct nl_msg * a;

    a = msg;
    *ret = 0;
    return NL_STOP;
}

/* callback handlers registered for nl message send */
static int finish_handler_nan(struct nl_msg *msg, void *arg)
{
  int *ret = (int *)arg;
  struct nl_msg * a;

  a = msg;
  *ret = 0;
  return NL_SKIP;
}

static int nan_send_nl_msg(hal_info *info, struct nl_msg *msg)
{
    int res = 0;
    struct nl_cb * cb = NULL;

    pthread_mutex_lock(&info->cb_lock);

    cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!cb) {
        ALOGE("%s: Callback allocation failed",__func__);
        res = -1;
        goto out;
    }

    if (!info->cmd_sock) {
        ALOGE("%s: Command socket is null",__func__);
        res = -1;
        goto out;
    }

    /* send message */
    res = nl_send_auto_complete(info->cmd_sock, msg);
    if (res < 0) {
        ALOGE("%s: send msg failed. err = %d",__func__, res);
        goto out;
    }

    res = 1;

    nl_cb_err(cb, NL_CB_CUSTOM, error_handler_nan, &res);
    nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler_nan, &res);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler_nan, &res);

    // err is populated as part of finish_handler
    while (res > 0)
        nl_recvmsgs(info->cmd_sock, cb);

out:
    nl_cb_put(cb);
    pthread_mutex_unlock(&info->cb_lock);
    return res;
}

int nan_send_tx_mgmt(void *ctx, const u8 *frame_buf, size_t frame_len,
                     int noack, unsigned int freq, unsigned int wait_dur)
{
    wifi_handle handle = (wifi_handle)ctx;
    hal_info *info = getHalInfo(handle);
    const struct ieee80211_mgmt *mgmt;
    u16 auth_transaction, status_code;
    struct nan_pairing_peer_info *peer;
    struct pasn_data *pasn;
    struct nl_msg * msg;
    int err = 0, l = 0;
    u32 i, idx;

    mgmt = (struct ieee80211_mgmt *)frame_buf;
    if (!mgmt || frame_len < offsetof(struct ieee80211_mgmt, u.auth.variable)) {
        ALOGE("%s: Invalid frame buf: len=%d \n", __FUNCTION__, frame_len);
        return -1;
    }

    msg = nlmsg_alloc();

    if (!msg) {
        ALOGE("%s: Memory allocation failed \n", __FUNCTION__);
        return -1;
    }

    /* After sending M2 frame, responder is expected to receive M3 and an
       encrypted followup frames from the initiator. Some times followup frame
       is received before the session keys are installed resulting in frame
       drop. Hence to avoid the race condition, install session keys immediately
       before sending M2 frame
    */

    status_code = le_to_host16(mgmt->u.auth.status_code);
    auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);

    peer = nan_pairing_get_peer_from_list(info->secure_nan, (u8 *)mgmt->da);
    if (peer && peer->peer_role == SECURE_NAN_PAIRING_INITIATOR &&
        auth_transaction == 2 && status_code == WLAN_STATUS_SUCCESS) {
        pasn = peer->pasn;
        ptksa_cache_add(info->secure_nan->ptksa, info->secure_nan->own_addr,
                        peer->bssid,pasn_get_cipher(pasn), 43200,
                        pasn_get_ptk(pasn), NULL, NULL,
                        pasn_get_akmp(pasn));
        nan_pairing_set_keys_from_cache(handle, info->secure_nan->own_addr,
                                        peer->bssid,pasn_get_cipher(pasn),
                                        pasn_get_akmp(pasn), peer->peer_role);
    }

    genlmsg_put(msg, 0, 0, info->nl80211_family_id, 0, 0, NL80211_CMD_FRAME, 0);

    idx = if_nametoindex(DEFAULT_NAN_IFACE);
    nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx);
    /* Add Frame here */
    nla_put(msg, NL80211_ATTR_FRAME, frame_len, frame_buf);

    err = nan_send_nl_msg(info, msg);

out_free_msg:
    nlmsg_free(msg);
    return err;
}

wifi_error nan_get_pairing_tk(transaction_id id,
                              wifi_interface_handle iface,
                              NanPairingTK *msg)
{
    interface_info *ifaceInfo = getIfaceInfo(iface);
    wifi_handle wifiHandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifiHandle);
    struct ptksa_cache_entry *entry;

    if (!info || !info->secure_nan || !msg) {
        ALOGE("%s: secure nan or msg NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    entry = ptksa_cache_get(info->secure_nan->ptksa,
                            msg->bssid, WPA_CIPHER_NONE);
    if (entry) {
        if (sizeof(msg->tk) < entry->ptk.tk_len) {
            ALOGE("%s: TK length invalid. len = %d", __FUNCTION__,
                  entry->ptk.tk_len);
            return WIFI_ERROR_UNKNOWN;
        }
        msg->tk_len = entry->ptk.tk_len;
        memcpy(msg->tk, entry->ptk.tk, entry->ptk.tk_len);
    }
    return WIFI_SUCCESS;
}

wifi_error nan_get_pairing_pmkid(transaction_id id,
                                 wifi_interface_handle iface,
                                 NanPairingPmkid *msg)
{
    interface_info *ifaceInfo = getIfaceInfo(iface);
    wifi_handle wifiHandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifiHandle);
    struct wpa_secure_nan *secure_nan;
    struct nan_pairing_peer_info *peer;

    if (!info || !info->secure_nan || !msg) {
        ALOGE("%s: secure nan or msg NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    secure_nan = info->secure_nan;
    peer = nan_pairing_get_peer_from_list(secure_nan, (u8 *)msg->bssid);
    if (!peer) {
        ALOGE(" %s :No Peer in pairing list, ADDR=" MACSTR,
              __FUNCTION__, MAC2STR(msg->bssid));
        return WIFI_ERROR_UNKNOWN;
    }

    if (peer->peer_role == SECURE_NAN_PAIRING_INITIATOR) {
        if (!nan_pairing_initiator_pmksa_cache_get(secure_nan->initiator_pmksa,
                                                    msg->bssid, msg->pmkid))
            msg->pmkid_len = PMKID_LEN;
    } else if (peer->peer_role == SECURE_NAN_PAIRING_RESPONDER) {
        if (!nan_pairing_responder_pmksa_cache_get(secure_nan->responder_pmksa,
                                                    msg->bssid, msg->pmkid))
            msg->pmkid_len = PMKID_LEN;
    } else {
        ALOGE(" %s :Peer role invalid, ADDR=" MACSTR,
              __FUNCTION__, MAC2STR(msg->bssid));
        return WIFI_ERROR_UNKNOWN;
    }
    return WIFI_SUCCESS;
}

wifi_error nan_validate_shared_key_desc(wifi_interface_handle iface,
                                        const u8 *addr, u8 *buf, u16 len)
{
    wifi_error ret = WIFI_SUCCESS;
    u16 key_data_len;
    struct wpa_secure_nan *secure_nan;
    struct ptksa_cache_entry *entry;
    struct sharedKeyDesc *shared_key_desc;
    struct keyDescriptor *key_desc;
    struct nanKDE *nan_kde;
    struct nikKDE *nik_kde;
    struct igtkKDE *igtk_kde;
    struct bigtkKDE *bigtk_kde;
    struct nikLifetime *nik_lifetime_kde;
    struct igtkLifetime *igtk_lifetime_kde;
    struct bigtkLifetime *bigtk_lifetime_kde;
    u8 *pos, *key_data, *data;
    struct nan_pairing_peer_info *peer;
    u16 remainingLen, key_len;
    u8 igtk_rcvd = 0, bigtk_rcvd = 0;
    struct nan_groupkey_info grpkey_info;
    wifi_handle wifiHandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifiHandle);

    if (len < sizeof(struct sharedKeyDesc) +
              sizeof(struct keyDescriptor))
    {
        ALOGE("%s: Invalid buf for Shared Key", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    if (!info || !info->secure_nan) {
        ALOGE("%s: secure nan NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }
    secure_nan = info->secure_nan;

    peer = nan_pairing_get_peer_from_list(secure_nan, (u8 *)addr);
    if (!peer) {
        ALOGE(" %s :No Peer in pairing list, ADDR=" MACSTR,
              __FUNCTION__, MAC2STR(addr));
        return WIFI_ERROR_UNKNOWN;
    }

    shared_key_desc = (struct sharedKeyDesc *)buf;
    if (shared_key_desc->attrID != NAN_SHARED_KEY_ATTR_ID) {
        ALOGE("%s: Invalid Attr ID: %d", __FUNCTION__, shared_key_desc->attrID);
        return WIFI_ERROR_UNKNOWN;
    }

    pos = buf;
    pos += sizeof(struct sharedKeyDesc);

    key_desc = (struct keyDescriptor *)pos;
    key_data_len = WPA_GET_BE16((u8 *)&key_desc->keyDataLen);

    if (len < sizeof(struct sharedKeyDesc) +
              sizeof(struct keyDescriptor) + key_data_len)
    {
        ALOGE("%s: Invalid buf for Shared Key", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pos += sizeof(struct keyDescriptor);

    key_data = pos;

    u16 keyInfo = WPA_GET_BE16((u8 *)&key_desc->keyInfo);
    data = (u8 *)malloc(key_data_len);
    if (!data) {
        ALOGE("%s: Memory alloc failed", __FUNCTION__);
        return WIFI_ERROR_OUT_OF_MEMORY;
    }
    /* Data is encrypted with KEK */
    if (keyInfo & NAN_ENCRYPT_KEY_DATA) {
        entry = ptksa_cache_get(secure_nan->ptksa, addr, WPA_CIPHER_NONE);
        if (!entry) {
            ALOGE("%s: PTKSA entry NULL", __FUNCTION__);
            ret = WIFI_ERROR_UNKNOWN;
            goto fail;
        }
        if(aes_unwrap(entry->ptk.kek, entry->ptk.kek_len,
           (key_data_len - 8)/8, key_data, data)) {
           ALOGE("%s: aes unwrap failed", __FUNCTION__);
           ret = WIFI_ERROR_UNKNOWN;
           goto fail;
        }
    } else {
        memcpy(data, key_data, key_data_len);
    }

    pos = data;
    remainingLen = key_data_len;

    while (remainingLen > 0) {

        if (remainingLen < sizeof(struct nanKDE))
            break;

        nan_kde = (struct nanKDE *)pos;
        if (remainingLen < 2 + nan_kde->length)
            break;
        if (nan_kde->length + 2 <= sizeof(struct nanKDE))
            break;

        if ((nan_kde->type != NAN_VENDOR_ATTR_TYPE) ||
            (WPA_GET_BE24(nan_kde->oui) != OUI_WFA)) {
            ALOGE("%s: invalid ATTR type:(%d) or  OUI:(0x%x)", __FUNCTION__,
                  nan_kde->type, WPA_GET_BE24(nan_kde->oui));
            goto skip_kde;
        }

        switch (nan_kde->dataType) {

        case NAN_KDE_TYPE_NIK:
             if (NAN_IDENTITY_KEY_LEN !=
                 2 + nan_kde->length - sizeof(struct nanKDE) - sizeof(struct nikKDE)) {
                 ALOGE("%s: invalid NIK Length", __FUNCTION__);
             } else {
                 nik_kde = (struct nikKDE *)nan_kde->data;
                 ALOGI("%s: copied peer nik", __FUNCTION__);
                 memcpy(peer->peer_nik, nik_kde->nik_data, NAN_IDENTITY_KEY_LEN);
             }
             break;

        case NAN_KDE_TYPE_NIK_LIFETIME:
             nik_lifetime_kde = (struct nikLifetime *)nan_kde->data;
             peer->peer_nik_lifetime = nik_lifetime_kde->lifetime;
             ALOGV("%s: received NIK Lifetime: %d", __FUNCTION__,
                   peer->peer_nik_lifetime);
             break;

        case NAN_KDE_TYPE_IGTK:
             igtk_kde = (struct igtkKDE *)nan_kde->data;
             key_len = 2 + nan_kde->length - sizeof(struct nanKDE) - sizeof(struct igtkKDE);
             // Using GCMP with key size of 16 bytes
             if (key_len == NAN_CSIA_GRPKEY_LEN_16) {
                 igtk_rcvd = 1;
             } else {
                 ALOGE("%s: unsupported IGTK len", __FUNCTION__);
             }
             break;

        case NAN_KDE_TYPE_IGTK_LIFETIME:
             igtk_lifetime_kde = (struct igtkLifetime *)nan_kde->data;
             ALOGV("%s: received IGTK Lifetime: %d", __FUNCTION__,
                   igtk_lifetime_kde->lifetime);
             break;
        case NAN_KDE_TYPE_BIGTK:
             bigtk_kde = (struct bigtkKDE *)nan_kde->data;
             key_len = 2 + nan_kde->length - sizeof(struct nanKDE) - sizeof(struct bigtkKDE);
             // Using GCMP with key size of 16 bytes
             if (key_len == NAN_CSIA_GRPKEY_LEN_16) {
                 bigtk_rcvd = 1;
             } else {
                 ALOGE("%s: unsupported BIGTK len", __FUNCTION__);
             }
             break;
        case NAN_KDE_TYPE_BIGTK_LIFETIME:
             bigtk_lifetime_kde = (struct bigtkLifetime *)nan_kde->data;
             ALOGV("%s: received BIGTK Lifetime: %d", __FUNCTION__,
                   bigtk_lifetime_kde->lifetime);
             break;
        default:
           ALOGE("NAN: Invalid Shared key KDE, DataType=%d", nan_kde->dataType);
           break;
        }

skip_kde:
        pos += 2 + nan_kde->length;
        remainingLen -= 2 + nan_kde->length;
    }

    if (igtk_rcvd || bigtk_rcvd) {
        memset(&grpkey_info, 0, sizeof(struct nan_groupkey_info));
        memcpy(grpkey_info.addr, addr, NAN_MAC_ADDR_LEN);

        if(igtk_rcvd) {
           grpkey_info.igtk_valid = 1;
           grpkey_info.igtk.key_cipher = WPA_CIPHER_GCMP;
           grpkey_info.igtk.key_idx = NAN_IGTK_KEY_IDX;
           grpkey_info.igtk.key_len = NAN_CSIA_GRPKEY_LEN_16;
           memcpy(grpkey_info.igtk.key_data, igtk_kde->igtk, NAN_CSIA_GRPKEY_LEN_16);
           memcpy(grpkey_info.igtk.rsc, igtk_kde->pn, NAN_MAX_GROUP_KEY_RSC_LEN);
        }

        if(bigtk_rcvd) {
           grpkey_info.bigtk_valid = 1;
           grpkey_info.bigtk.key_cipher = WPA_CIPHER_GCMP;
           grpkey_info.bigtk.key_idx = NAN_BIGTK_KEY_IDX;
           grpkey_info.bigtk.key_len = NAN_CSIA_GRPKEY_LEN_16;
           memcpy(grpkey_info.bigtk.key_data, bigtk_kde->bigtk, NAN_CSIA_GRPKEY_LEN_16);
           memcpy(grpkey_info.bigtk.rsc, bigtk_kde->pn, NAN_MAX_GROUP_KEY_RSC_LEN);
        }
        nan_pairing_set_group_key(0, iface, &grpkey_info);
    }
fail:
     free(data);
     return ret;
}

wifi_error nan_get_shared_key_descriptor(hal_info *info, const u8 *addr,
                                         NanSharedKeyRequest *key)
{
    wifi_error ret = WIFI_ERROR_UNKNOWN;

    u8 *buf, *pos, *key_data, *enc_key_data, *enc_buf;
    u16 buf_len, key_data_len, pad_len = 0, oui_offset;
    struct sharedKeyDesc *shared_key_desc;
    struct keyDescriptor *key_desc;
    struct nanKDE *nan_kde;
    struct nikKDE *nik_kde;
    struct igtkKDE *igtk_kde;
    struct bigtkKDE *bigtk_kde;
    struct nikLifetime *nik_lifetime_kde;
    struct igtkLifetime *igtk_lifetime_kde;
    struct bigtkLifetime *bigtk_lifetime_kde;
    struct nanGrpKey *grp_keys;
    struct nanIDkey *nik;
    struct ptksa_cache_entry *entry;
    struct wpa_secure_nan *secure_nan;
    struct nan_pairing_peer_info *peer;
    int groupMfp;

    if (!info || !info->secure_nan || !info->secure_nan->dev_nik) {
        ALOGE("%s: secure nan NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    peer = nan_pairing_get_peer_from_list(info->secure_nan, (u8 *)addr);
    if (!peer) {
        ALOGE("nl80211: Peer not found in the pairing list");
        return WIFI_ERROR_UNKNOWN;
    }

    secure_nan = info->secure_nan;
    nik = secure_nan->dev_nik;
    grp_keys = secure_nan->dev_grp_keys;
    // construct KDE

    key_data_len = sizeof(struct nanKDE) + sizeof(struct nikKDE) + nik->nik_len +
                   sizeof(struct nanKDE) + sizeof(struct nikLifetime);

    if (grp_keys) {
       if (grp_keys->igtk_len)
           key_data_len += (sizeof(struct nanKDE) + sizeof(struct igtkKDE) +
                            grp_keys->igtk_len) +
                           (sizeof(struct nanKDE) + sizeof(struct igtkLifetime));
       if (grp_keys->bigtk_len)
           key_data_len += (sizeof(struct nanKDE) + sizeof(struct bigtkKDE) +
                            grp_keys->bigtk_len) +
                           (sizeof(struct nanKDE) + sizeof(struct bigtkLifetime));
    }

    pad_len = key_data_len % 8;
    if (pad_len)
        pad_len = 8 - pad_len;
    key_data_len += pad_len + 8;

    buf_len = sizeof(struct sharedKeyDesc) + sizeof(struct keyDescriptor) +
              key_data_len;

    buf = (u8 *)malloc(buf_len);

    if (!buf) {
            ALOGE("%s: Memory allocation Fail", __FUNCTION__);
            return WIFI_ERROR_OUT_OF_MEMORY;
    }
    memset(buf, 0, buf_len);
    pos = buf;

    oui_offset = sizeof(struct nanKDE) - offsetof(struct nanKDE, oui);
    shared_key_desc = (struct sharedKeyDesc *)buf;
    shared_key_desc->attrID = NAN_SHARED_KEY_ATTR_ID;
    shared_key_desc->length = buf_len - offsetof(struct sharedKeyDesc,
                                                 publishID);
    shared_key_desc->publishID = secure_nan->pub_sub_id;
    pos += sizeof(struct sharedKeyDesc);

    key_desc = (struct keyDescriptor *)pos;
    WPA_PUT_BE16((u8 *)&key_desc->keyInfo, NAN_ENCRYPT_KEY_DATA);
    WPA_PUT_BE16((u8 *)&key_desc->keyDataLen, key_data_len);
    pos += sizeof(struct keyDescriptor);

    key_data = pos;
    nan_kde = (struct nanKDE *)pos;
    nan_kde->type = NAN_VENDOR_ATTR_TYPE;
    nan_kde->length = oui_offset + sizeof(struct nikKDE) + nik->nik_len;
    WPA_PUT_BE24(nan_kde->oui, OUI_WFA);
    nan_kde->dataType = NAN_KDE_TYPE_NIK;
    pos += sizeof(struct nanKDE);

    nik_kde = (struct nikKDE *)pos;
    nik_kde->cipher = NCS_SK_128;
    memcpy(nik_kde->nik_data, nik->nik_data, nik->nik_len);
    pos += sizeof(struct nikKDE) + nik->nik_len;

    nan_kde = (struct nanKDE *)pos;
    nan_kde->type = NAN_VENDOR_ATTR_TYPE;
    nan_kde->length = oui_offset + sizeof(struct nikLifetime);
    WPA_PUT_BE24(nan_kde->oui, OUI_WFA);
    nan_kde->dataType = NAN_KDE_TYPE_NIK_LIFETIME;
    pos += sizeof(struct nanKDE);

    nik_lifetime_kde = (struct nikLifetime *)pos;
    nik_lifetime_kde->lifetime = nan_pairing_get_nik_lifetime(nik);
    pos += sizeof(struct nikLifetime);


    groupMfp = NAN_CSIA_GRPKEY_SUPPORT_GET(peer->csia_cap_info);
    ALOGI("%s: CSIA capabilities %d", __FUNCTION__, peer->csia_cap_info);
/* IGTK */
    if (grp_keys && grp_keys->igtk_len &&
        (groupMfp == NAN_GTKSA_IGTKSA_BIGTKSA_SUPPORTED ||
         groupMfp == NAN_GTKSA_IGTKSA_SUPPORTED_BIGTKSA_NOT_SUPPORTED)) {
        nan_kde = (struct nanKDE *)pos;
        nan_kde->type = NAN_VENDOR_ATTR_TYPE;
        nan_kde->length = oui_offset + sizeof(struct igtkKDE) +
                          grp_keys->igtk_len;
        WPA_PUT_BE24(nan_kde->oui, OUI_WFA);
        nan_kde->dataType = NAN_KDE_TYPE_IGTK;
        pos += sizeof(struct nanKDE);

        igtk_kde = (struct igtkKDE *)pos;
        igtk_kde->keyid[0] = NAN_IGTK_KEY_IDX;
        memcpy(igtk_kde->pn, grp_keys->igtk_rsc, NAN_MAX_GROUP_KEY_RSC_LEN);
        memcpy(igtk_kde->igtk, grp_keys->igtk, grp_keys->igtk_len);
        pos += sizeof(struct igtkKDE) + grp_keys->igtk_len;

        nan_kde = (struct nanKDE *)pos;
        nan_kde->type = NAN_VENDOR_ATTR_TYPE;
        nan_kde->length = oui_offset + sizeof(struct igtkLifetime);
        WPA_PUT_BE24(nan_kde->oui, OUI_WFA);
        nan_kde->dataType = NAN_KDE_TYPE_IGTK_LIFETIME;
        pos += sizeof(struct nanKDE);

        igtk_lifetime_kde = (struct igtkLifetime *)pos;
        igtk_lifetime_kde->lifetime = grp_keys->igtk_life_time;
        pos += sizeof(struct igtkLifetime);
    }
/* IGTK end */

/* BIGTK */
    if (grp_keys && grp_keys->bigtk_len &&
        groupMfp == NAN_GTKSA_IGTKSA_BIGTKSA_SUPPORTED) {
        nan_kde = (struct nanKDE *)pos;
        nan_kde->type = NAN_VENDOR_ATTR_TYPE;
        nan_kde->length = oui_offset + sizeof(struct bigtkKDE) +
                          grp_keys->bigtk_len;
        WPA_PUT_BE24(nan_kde->oui, OUI_WFA);
        nan_kde->dataType = NAN_KDE_TYPE_BIGTK;
        pos += sizeof(struct nanKDE);

        bigtk_kde = (struct bigtkKDE *)pos;
        bigtk_kde->keyid[0] = NAN_BIGTK_KEY_IDX;
        memcpy(bigtk_kde->pn, grp_keys->bigtk_rsc, NAN_MAX_GROUP_KEY_RSC_LEN);
        memcpy(bigtk_kde->bigtk, grp_keys->bigtk, grp_keys->bigtk_len);
        pos += sizeof(struct bigtkKDE) + grp_keys->bigtk_len;

        nan_kde = (struct nanKDE *)pos;
        nan_kde->type = NAN_VENDOR_ATTR_TYPE;
        nan_kde->length = oui_offset + sizeof(struct bigtkLifetime);
        WPA_PUT_BE24(nan_kde->oui, OUI_WFA);
        nan_kde->dataType = NAN_KDE_TYPE_BIGTK_LIFETIME;
        pos += sizeof(struct nanKDE);

        bigtk_lifetime_kde = (struct bigtkLifetime *)pos;
        bigtk_lifetime_kde->lifetime = grp_keys->bigtk_life_time;
        pos += sizeof(struct bigtkLifetime);
    }
/* BIGTK end */

    if(pad_len)
       *pos++ = 0xdd;

    //Encrypt keydata
    enc_buf = (u8 *)malloc(buf_len);
    if (!enc_buf) {
        ALOGE("%s: enc buf alloc Failed", __FUNCTION__);
        ret = WIFI_ERROR_OUT_OF_MEMORY;
        goto fail;
    }

    memcpy (enc_buf, buf, buf_len - key_data_len);
    enc_key_data = enc_buf + (buf_len - key_data_len);

    entry = ptksa_cache_get(secure_nan->ptksa, addr, WPA_CIPHER_NONE);
    if (entry && aes_wrap(entry->ptk.kek, entry->ptk.kek_len,
                       (key_data_len - 8)/8, key_data, enc_key_data))
    {
            ALOGE("%s: aes wrap Failed", __FUNCTION__);
            ret = WIFI_ERROR_UNKNOWN;
            goto fail;
    }
    key->shared_key_attr_len = buf_len;
    memcpy(key->shared_key_attr, enc_buf, buf_len);
    ret = WIFI_SUCCESS;

fail:
    free(buf);
    free(enc_buf);
    return ret;
}

static u32 wpa_alg_to_cipher_suite(enum wpa_alg alg, size_t key_len)
{
    switch (alg) {
    case WPA_ALG_WEP:
        if (key_len == 5)
            return RSN_CIPHER_SUITE_WEP40;
        return RSN_CIPHER_SUITE_WEP104;
    case WPA_ALG_TKIP:
        return RSN_CIPHER_SUITE_TKIP;
    case WPA_ALG_CCMP:
        return RSN_CIPHER_SUITE_CCMP;
    case WPA_ALG_GCMP:
        return RSN_CIPHER_SUITE_GCMP;
    case WPA_ALG_CCMP_256:
        return RSN_CIPHER_SUITE_CCMP_256;
    case WPA_ALG_GCMP_256:
        return RSN_CIPHER_SUITE_GCMP_256;
    case WPA_ALG_BIP_CMAC_128:
        return RSN_CIPHER_SUITE_AES_128_CMAC;
    case WPA_ALG_BIP_GMAC_128:
        return RSN_CIPHER_SUITE_BIP_GMAC_128;
    case WPA_ALG_BIP_GMAC_256:
        return RSN_CIPHER_SUITE_BIP_GMAC_256;
    case WPA_ALG_BIP_CMAC_256:
        return RSN_CIPHER_SUITE_BIP_CMAC_256;
    case WPA_ALG_SMS4:
        return RSN_CIPHER_SUITE_SMS4;
    case WPA_ALG_KRK:
        return RSN_CIPHER_SUITE_KRK;
    default:
        ALOGI("NAN: Unexpected encryption algorithm %d", alg);
        return 0;
    }
}

wifi_error nan_pairing_set_group_key(transaction_id id,
                                     wifi_interface_handle iface,
                                     struct nan_groupkey_info *grpkey_info)
{
    wifi_error ret;
    NanCommand *nanCommand;
    interface_info *ifaceInfo = getIfaceInfo(iface);
    wifi_handle wifiHandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifiHandle);

    if (info == NULL) {
        ALOGE("%s: Error hal_info NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    nanCommand = new NanCommand(wifiHandle,
                                0,
                                OUI_QCA,
                                info->support_nan_ext_cmd?
                                QCA_NL80211_VENDOR_SUBCMD_NAN_EXT :
                                QCA_NL80211_VENDOR_SUBCMD_NAN);
    if (nanCommand == NULL) {
        ALOGE("%s: Error NanCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    ret = nanCommand->create();
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = nanCommand->set_iface_id(ifaceInfo->name);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    ret = nanCommand->putNanGroupKeyInstallReq(id, grpkey_info);
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: putNanGroupKeyInstallReq Error:%d", __FUNCTION__, ret);
        goto cleanup;
    }

    ret = nanCommand->requestEvent();
    if (ret != WIFI_SUCCESS)
        ALOGE("%s: requestEvent Error:%d", __FUNCTION__, ret);

cleanup:
    delete nanCommand;
    return ret;
}


static void nl80211_nlmsg_clear(struct nl_msg *msg)
{
   if (msg) {
       struct nlmsghdr *hdr = nlmsg_hdr(msg);
       void *data = nlmsg_data(hdr);
       int len = hdr->nlmsg_len - NLMSG_HDRLEN;

       os_memset(data, 0, len);
   }
}


static int nan_pairing_set_key(hal_info *info, int alg, const u8 *addr,
                               int key_idx, int set_tx, const u8 *seq,
                               size_t seq_len, const u8 *key, size_t key_len,
                               int key_flag)
{
    int idx;
    u32 suite;
    struct nl_msg *msg;
    struct nl_msg *key_msg;
    int ret = -1;

    idx = if_nametoindex(DEFAULT_NAN_IFACE);

    if (check_key_flag((enum key_flag) key_flag)) {
        ALOGE("%s: invalid key_flag", __FUNCTION__);
        return ret;
    }

    msg = nlmsg_alloc();
    if (!msg) {
        ALOGE("%s: msg allocation failed\n", __FUNCTION__);
        return ret;
    }
    key_msg = nlmsg_alloc();
    if (!key_msg) {
        nlmsg_free(msg);
        ALOGE("%s: Key msg allocation failed\n", __FUNCTION__);
        return ret;
    }

    if ((key_flag & KEY_FLAG_PAIRWISE_MASK) ==
        KEY_FLAG_PAIRWISE_RX_TX_MODIFY) {
        ALOGV("%s: nl80211: SET_KEY (pairwise RX/TX modify)", __FUNCTION__);
        if (!genlmsg_put(msg, 0, 0, info->nl80211_family_id, 0, 0,
                         NL80211_CMD_SET_KEY, 0) ||
            nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx))
            goto fail;
    } else if (alg == WPA_ALG_NONE && (key_flag & KEY_FLAG_RX_TX)) {
        ALOGE("%s: invalid key_flag to delete key", __FUNCTION__);
        goto fail;
    } else if (alg == WPA_ALG_NONE) {
        ALOGV("%s: nl80211: DEL_KEY", __FUNCTION__);
        if (!genlmsg_put(msg, 0, 0, info->nl80211_family_id, 0, 0,
                         NL80211_CMD_DEL_KEY, 0) ||
            nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx))
            goto fail;
    } else {
        suite = wpa_alg_to_cipher_suite((enum wpa_alg) alg, key_len);
        if (!suite) {
            goto fail;
        }
        ALOGV("%s: nl80211: NEW_KEY", __FUNCTION__);
        if (!genlmsg_put(msg, 0, 0, info->nl80211_family_id, 0, 0,
                         NL80211_CMD_NEW_KEY, 0) ||
            nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx) ||
            nla_put(key_msg, NL80211_KEY_DATA, key_len, key) ||
            nla_put_u32(key_msg, NL80211_KEY_CIPHER, suite))
            goto fail;
        if (seq && seq_len) {
            if (nla_put(key_msg, NL80211_KEY_SEQ, seq_len, seq))
                goto fail;
            ALOGV("%s: NL80211_KEY_SEQ seq=%p, sed_len=%lu", __FUNCTION__, seq, (unsigned long) seq_len);
        }
    }
    if (addr && !is_broadcast_ether_addr(addr)) {
        ALOGV("%s: addr=" MACSTR, __FUNCTION__, MAC2STR(addr));
        if (nla_put(msg, NL80211_ATTR_MAC, ETH_ALEN, addr) ||
            nla_put(msg, NL80211_ATTR_BSS, ETH_ALEN, addr))
            goto fail;
        if ((key_flag & KEY_FLAG_PAIRWISE_MASK) ==
            KEY_FLAG_PAIRWISE_RX ||
            (key_flag & KEY_FLAG_PAIRWISE_MASK) ==
            KEY_FLAG_PAIRWISE_RX_TX_MODIFY) {
            if (nla_put_u8(key_msg, NL80211_KEY_MODE,
                           key_flag == KEY_FLAG_PAIRWISE_RX ? NL80211_KEY_NO_TX : NL80211_KEY_SET_TX))
                goto fail;
        } else if ((key_flag & KEY_FLAG_GROUP_MASK) ==
                    KEY_FLAG_GROUP_RX) {
            ALOGV("%s:    RSN IBSS RX GTK", __FUNCTION__);
            if (nla_put_u32(key_msg, NL80211_KEY_TYPE, NL80211_KEYTYPE_GROUP))
                goto fail;
        } else if (!(key_flag & KEY_FLAG_PAIRWISE)) {
            ALOGV("%s:  key_flag missing PAIRWISE when setting a pairwise key", __FUNCTION__);
            goto fail;
        } else {
            ALOGV("%s: pairwise key", __FUNCTION__);
        }
    } else if ((key_flag & KEY_FLAG_PAIRWISE) ||
               !(key_flag & KEY_FLAG_GROUP)) {
        ALOGE("%s: invalid key_flag for a broadcast key", __FUNCTION__);
        goto fail;
    } else {
        ALOGV("%s: broadcast key", __FUNCTION__);
    }
    if (nla_put_u8(key_msg, NL80211_KEY_IDX, key_idx) ||
        nla_put_nested(msg, NL80211_ATTR_KEY, key_msg))
        goto fail;

    ret = nan_send_nl_msg(info, msg);
fail:
    nlmsg_free(msg);
    nl80211_nlmsg_clear(key_msg);
    nlmsg_free(key_msg);
    return ret;
}

int nan_pairing_set_keys_from_cache(wifi_handle handle, u8 *src_addr, u8 *bssid,
                                    int cipher, int akmp, int peer_role)
{

    const u8 *tk;
    size_t tk_len;
    enum wpa_alg alg;
    u32 size = 0;
    NanDebugParams cfg_debug;
    struct ptksa_cache_entry *entry;
    struct nan_pairing_peer_info *peer;
    hal_info *info = getHalInfo(handle);
    struct pasn_data *pasn;
    NanCommand *nanCommand = NULL;
    wifi_interface_handle ifaceHandle;

    nanCommand = NanCommand::instance(handle);
    if (nanCommand == NULL) {
        ALOGE("%s: Error NanCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    peer = nan_pairing_get_peer_from_list(info->secure_nan, bssid);
    if (!peer) {
        ALOGE("nl80211: Peer not found in the pairing list");
        return WIFI_ERROR_UNKNOWN;
    }
    pasn = peer->pasn;
    entry = ptksa_cache_get(info->secure_nan->ptksa, bssid, cipher);
    if (!entry) {
        ALOGE("NAN Pairing: peer " MACSTR "not present in PTKSA cache",
              MAC2STR(bssid));
        return WIFI_ERROR_UNKNOWN;
    }

    if (os_memcmp(entry->own_addr, src_addr, ETH_ALEN) != 0) {
        ALOGE("NAN Pairing: src addr " MACSTR " and PTKSA entry src addr " MACSTR " differ",
              MAC2STR(src_addr), MAC2STR(entry->own_addr));
        return WIFI_ERROR_UNKNOWN;
    }

    tk = entry->ptk.tk;
    tk_len = entry->ptk.tk_len;
    alg = wpa_cipher_to_alg(entry->cipher);

    ALOGD("PASN:" MACSTR "present in PTKSA cache",
          MAC2STR(bssid));

    nan_pairing_set_key(info, alg, bssid, 0, 1, NULL, 0, tk, tk_len,
                        KEY_FLAG_PAIRWISE_RX_TX);

    ifaceHandle = wifi_get_iface_handle(handle, info->secure_nan->iface_name);
    if (!ifaceHandle) {
        ALOGE("%s: ifaceHandle NULL for %s", __FUNCTION__,
              info->secure_nan->iface_name);
        return WIFI_ERROR_INVALID_ARGS;
    }

    if (peer_role == SECURE_NAN_PAIRING_INITIATOR) {
        memset(&cfg_debug, 0, sizeof(NanDebugParams));
        cfg_debug.cmd = NAN_TEST_MODE_CMD_PMK;
        nan_pasn_kdk_to_ndp_pmk(entry->ptk.kdk, entry->ptk.kdk_len,
                                entry->addr, entry->own_addr,
                                cfg_debug.debug_cmd_data, &size);
        if (!size) {
            ALOGE("%s: Invalid NDP PMK len", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        nan_debug_command_config(0, ifaceHandle, cfg_debug, size + 4);
        nan_pasn_kdk_to_nan_kek(entry->ptk.kdk, entry->ptk.kdk_len, entry->addr,
                                entry->own_addr, akmp, cipher, entry->ptk.kek,
                                &entry->ptk.kek_len);
    } else {
        nan_pasn_kdk_to_nan_kek(entry->ptk.kdk, entry->ptk.kdk_len, entry->own_addr,
                                entry->addr, akmp, cipher, entry->ptk.kek,
                                &entry->ptk.kek_len);
    }
    nan_set_nira_request(0, ifaceHandle, info->secure_nan->dev_nik->nik_data);

    if (peer_role == SECURE_NAN_PAIRING_INITIATOR)
        return WIFI_SUCCESS;

    if (peer->dcea_cap_info & DCEA_NPK_CACHING_ENABLED) {
        info->secure_nan->pending_peer = peer;
        nan_pairing_prepare_skda_data(ifaceHandle);
    } else {
        // Send Pairing Confirmation as Followup with Peer NIK is not mandatory
        NanPairingConfirmInd evt;
        evt.pairing_instance_id = peer->pairing_instance_id;
        evt.rsp_code = NAN_PAIRING_REQUEST_ACCEPT;
        evt.reason_code = NAN_STATUS_SUCCESS;
        evt.enable_pairing_cache = 0;

        if (peer->is_paired)
            evt.nan_pairing_request_type = NAN_PAIRING_VERIFICATION;
        else
            evt.nan_pairing_request_type = NAN_PAIRING_SETUP;

        if (pasn_get_akmp(pasn) == WPA_KEY_MGMT_PASN)
            evt.npk_security_association.akm = PASN;
        else
            evt.npk_security_association.akm = SAE;

        if (pasn_get_cipher(pasn) == WPA_CIPHER_CCMP_256)
            evt.npk_security_association.cipher_type = NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_256_MASK;
        else
            evt.npk_security_association.cipher_type = NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_128_MASK;

        if (info->secure_nan->dev_nik)
            memcpy(evt.npk_security_association.local_nan_identity_key,
                   info->secure_nan->dev_nik->nik_data,
                   NAN_IDENTITY_KEY_LEN);

        if (pasn_get_pmk_len(pasn) <= sizeof(evt.npk_security_association.npk.pmk)) {
            memcpy(evt.npk_security_association.npk.pmk, pasn_get_pmk(pasn),
                   pasn_get_pmk_len(pasn));
            evt.npk_security_association.npk.pmk_len = pasn_get_pmk_len(pasn);
        } else {
            ALOGE("%s: Invalid pmk len: %d", __FUNCTION__, pasn_get_pmk_len(pasn));
        }
        wpa_pasn_reset(pasn);
        nanCommand->handleNanPairingConfirm(&evt);
        peer->is_paired = true;
        peer->is_pairing_in_progress = false;
    }
    return WIFI_SUCCESS;
}

static u32 nan_fetch_key_idx(struct wpa_secure_nan *secure_nan)
{
    if (secure_nan->pn_bitmap & NAN_PN_REQ_BITMAP_IGTK) {
        secure_nan->pn_bitmap &= ~NAN_PN_REQ_BITMAP_IGTK;
        return NAN_IGTK_KEY_IDX;
    }

    if (secure_nan->pn_bitmap & NAN_PN_REQ_BITMAP_BIGTK) {
        secure_nan->pn_bitmap &= ~NAN_PN_REQ_BITMAP_BIGTK;
        return NAN_BIGTK_KEY_IDX;
    }
    return 0;
}


int nan_handle_pn_response(wifi_handle handle, transaction_id id, u8 *key_rsc)
{
    u32 key_index;
    wifi_interface_handle ifaceHandle;
    hal_info *info = getHalInfo(handle);

    ifaceHandle = wifi_get_iface_handle(handle, info->secure_nan->iface_name);
    if (!ifaceHandle) {
        ALOGE("%s: ifaceHandle NULL for %s", __FUNCTION__,
              info->secure_nan->iface_name);
        return WIFI_ERROR_INVALID_ARGS;
    }

    // For PN request, transaction id and key index are same
    if (id == NAN_IGTK_KEY_IDX) {
        memcpy(info->secure_nan->dev_grp_keys->igtk_rsc, key_rsc,
               NAN_MAX_GROUP_KEY_RSC_LEN);
    } else if (id == NAN_BIGTK_KEY_IDX) {
        memcpy(info->secure_nan->dev_grp_keys->bigtk_rsc, key_rsc,
               NAN_MAX_GROUP_KEY_RSC_LEN);
    }

    key_index = nan_fetch_key_idx(info->secure_nan);
    if (!key_index) {
        nan_pairing_send_skda_data(ifaceHandle);
        return 0;
    }

    // Use transaction ID same as Key index for group key PN requests
    nan_group_key_pn_request(key_index, ifaceHandle, key_index);
    return 0;
}


int nan_pairing_send_skda_data(wifi_interface_handle iface)
{
      int ret = 0;
      NanSharedKeyRequest msg;
      struct nan_pairing_peer_info *peer;
      wifi_handle wifiHandle = getWifiHandle(iface);
      hal_info *info = getHalInfo(wifiHandle);
      struct wpa_secure_nan *secure_nan = info->secure_nan;

      if (!secure_nan->pending_peer) {
          ALOGE("NAN: Pending Peer info NULL");
          return -1;
      }

      peer = secure_nan->pending_peer;

      memset((u8 *)&msg, 0, sizeof(NanSharedKeyRequest));
      if (nan_get_shared_key_descriptor(info, peer->bssid, &msg)) {
          ALOGE("NAN: Unable to get shared key descriptor");
          ret = -1;
          goto out;
      }
      memcpy(msg.peer_disc_mac_addr, peer->bssid, NAN_MAC_ADDR_LEN);
      msg.requestor_instance_id = peer->requestor_instance_id;
      msg.pub_sub_id = peer->pub_sub_id;
      nan_sharedkey_followup_request(0, iface, &msg);
out:
      secure_nan->pending_peer = NULL;
      return ret;
}

int nan_pairing_prepare_skda_data(wifi_interface_handle iface)
{
    u32 key_index;
    wifi_handle wifiHandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifiHandle);
    struct wpa_secure_nan *secure_nan = info->secure_nan;

    secure_nan->pn_bitmap = 0;

    if (secure_nan->dev_grp_keys) {
        if (secure_nan->dev_grp_keys->igtk_len)
            secure_nan->pn_bitmap |= NAN_PN_REQ_BITMAP_IGTK;

        if (secure_nan->dev_grp_keys->bigtk_len)
            secure_nan->pn_bitmap |= NAN_PN_REQ_BITMAP_BIGTK;
    }

    key_index = nan_fetch_key_idx(secure_nan);
    if (!key_index) {
        ALOGV("NAN: SKDA data with out Group keys");
        nan_pairing_send_skda_data(iface);
        return 0;
    }

    // Use transaction ID same as Key index for group key PN requests
    nan_group_key_pn_request(key_index, iface, key_index);
    return 0;
}

static int nan_pairing_register_pasn_auth_frames(wifi_interface_handle iface)
{
    /* wlan type:mgmt, wlan subtype: auth */
    u16 type = (WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_AUTH << 4);
    /* register for PASN Authentication frames */
    const u8 pasn_auth_match[2] = {7,0};

    return nan_register_frames(iface, type, pasn_auth_match, 2);
}

int nan_pasn_kdk_to_ndp_pmk(const u8 *kdk, size_t kdk_len, const u8 *spa,
                            const u8 *bssid, u8 *ndp_pmk, u32 *ndp_pmk_len)
{
    u8 tmp[WPA_NDP_PMK_MAX_LEN];
    u8 *data;
    size_t data_len;
    int ret = -1;
    const char *label = "NDP PMK Derivation";

    *ndp_pmk_len = 0;

    if (!kdk || !kdk_len) {
        ALOGE("PASN: No KDK set for NDP PMK derivation");
        return -1;
    }

    if (!bssid || !spa || !ndp_pmk) {
        ALOGE("PASN: Invalid arguments");
        return -1;
    }

    /*
     * NDP-PMK = KDF-256(KDK, “NDP PMK Derivation”, Initiator NMI || Responder NMI)
     */
    data_len = 2 * ETH_ALEN;
    data = (u8 *)os_zalloc(data_len);
    if (!data) {
        ALOGE("%s: Memory allocation failed", __FUNCTION__);
        return -1;
    }

    os_memcpy(data, spa, ETH_ALEN);
    os_memcpy(data + ETH_ALEN, bssid, ETH_ALEN);

    ALOGD("PASN: NDP PMK derivation: SPA=" MACSTR " BSSID=" MACSTR,
          MAC2STR(spa), MAC2STR(bssid));

    ret = sha256_prf(kdk, kdk_len, label, data, data_len, tmp, WPA_NDP_PMK_MAX_LEN);
    if (ret < 0) {
        ALOGE("%s: PMK derivation failed, err = %d", __FUNCTION__, ret);
        goto err;
    }

    os_memcpy(ndp_pmk, tmp, WPA_NDP_PMK_MAX_LEN);
    *ndp_pmk_len = WPA_NDP_PMK_MAX_LEN;

    forced_memzero(tmp, sizeof(tmp));
    ret = 0;
err:
    bin_clear_free(data, data_len);
    return ret;
}

int nan_pasn_kdk_to_opportunistic_npk(const u8 *kdk, size_t kdk_len,
                                      const u8 *spa, const u8 *bssid,
                                      int akmp, int cipher, u8 *opp_npk,
                                      size_t *opp_npk_len)
{
    u8 tmp[WPA_OPP_NPK_MAX_LEN];
    u8 *data;
    size_t data_len, key_len;
    int ret = -1;
    const char *label = "NAN Opportunistic NPK Derivation";

    *opp_npk_len = 0;

    if (!kdk || !kdk_len) {
        ALOGE("PASN: No KDK set for NAN Opportunistic NPK derivation");
        return -1;
    }

    if (!bssid || !spa || !opp_npk) {
        ALOGE("PASN: Invalid arguments");
        return -1;
    }

    /*
     * PASN-Opportunistic-NPK = KDF-256(KDK, “NAN Opportunistic NPK Derivation”, Initiator NMI || Responder NMI)
     */
    data_len = 2 * ETH_ALEN;
    data = (u8 *)os_zalloc(data_len);
    if (!data) {
        ALOGE("%s: Memory allocation failed", __FUNCTION__);
        return -1;
    }

    os_memcpy(data, spa, ETH_ALEN);
    os_memcpy(data + ETH_ALEN, bssid, ETH_ALEN);

    key_len = wpa_cipher_key_len(cipher);

    if (key_len == 0) {
        ALOGE("PASN: Unsupported cipher (0x%x) used in NAN Opportunistic NPK derivation",
              cipher);
        goto err;
    }

    ALOGD("PASN: NAN Opportunistic NPK derivation: SPA=" MACSTR " BSSID=" MACSTR " akmp=0x%x, cipher=0x%x",
          MAC2STR(spa), MAC2STR(bssid), akmp, cipher);

    if (pasn_use_sha384(akmp, cipher))
        ret = sha384_prf(kdk, kdk_len, label, data, data_len, tmp, key_len);
    else
        ret = sha256_prf(kdk, kdk_len, label, data, data_len, tmp, key_len);

    if (ret < 0) {
        ALOGE("%s: sha prf failed, err = %d", __FUNCTION__, ret);
        goto err;
    }

    os_memcpy(opp_npk, tmp, key_len);
    *opp_npk_len = key_len;

    forced_memzero(tmp, sizeof(tmp));
    ret = 0;
err:
    bin_clear_free(data, data_len);
    return ret;
}

int nan_pasn_kdk_to_nan_kek(const u8 *kdk, size_t kdk_len, const u8 *spa,
                            const u8 *bssid, int akmp, int cipher, u8 *nan_kek,
                            size_t *nan_kek_len)
{
    u8 tmp[WPA_KEK_MAX_LEN];
    u8 *data;
    size_t data_len, key_len;
    int ret = -1;
    const char *label = "NAN Management KEK Derivation";

    *nan_kek_len = 0;

    if (!kdk || !kdk_len) {
        ALOGE("PASN: No KDK set for NAN KEK derivation");
        return -1;
    }

    if (!bssid || !spa || !nan_kek) {
        ALOGE("PASN: Invalid arguments");
        return -1;
    }

    /*
     * PASN-KEK = KDF(KDK, “NAN Management KEK Derivation”, Initiator NMI || Responder NMI)
     */
    data_len = 2 * ETH_ALEN;
    data = (u8 *)os_zalloc(data_len);
    if (!data) {
        ALOGE("%s: Memory allocation failed", __FUNCTION__);
        return -1;
    }

    os_memcpy(data, spa, ETH_ALEN);
    os_memcpy(data + ETH_ALEN, bssid, ETH_ALEN);

    key_len = wpa_cipher_key_len(cipher);

    if (key_len == 0) {
        ALOGE("PASN: Unsupported cipher (0x%x) used in NAN KEK derivation",
              cipher);
        goto err;
    }

    ALOGD("PASN: NAN KEK derivation: SPA=" MACSTR " BSSID=" MACSTR " akmp=0x%x, cipher=0x%x",
          MAC2STR(spa), MAC2STR(bssid), akmp, cipher);

    if (pasn_use_sha384(akmp, cipher))
        ret = sha384_prf(kdk, kdk_len, label, data, data_len, tmp, key_len);
    else
        ret = sha256_prf(kdk, kdk_len, label, data, data_len, tmp, key_len);

    if (ret < 0) {
        ALOGE("%s: sha prf failed, err = %d", __FUNCTION__, ret);
        goto err;
    }

    os_memcpy(nan_kek, tmp, key_len);
    *nan_kek_len = key_len;

    forced_memzero(tmp, sizeof(tmp));
    ret = 0;
err:
    bin_clear_free(data, data_len);
    return ret;
}

int nan_pairing_validate_custom_pmkid(void *ctx, const u8 *bssid,
                                      const u8 *pmkid)
{
    int ret;
    struct nan_pairing_peer_info *entry;
    wifi_handle handle = (wifi_handle)ctx;
    hal_info *info = getHalInfo(handle);
    u8 tag[NAN_MAX_HASH_LEN];
    u8 data[NIR_STR_LEN + NAN_IDENTITY_NONCE_LEN + ETH_ALEN];

    if (!info || !info->secure_nan) {
        ALOGE(" %s: HAL info or Secure NAN is NULL", __FUNCTION__);
        return -1;
    }

    entry = nan_pairing_get_peer_from_list(info->secure_nan, (u8 *)bssid);
    if (!entry) {
        ALOGE(" %s: No Peer in pairing list, ADDR=" MACSTR,
              __FUNCTION__, MAC2STR(bssid));
        return -1;
    }

    if (is_zero_nan_identity_key(entry->peer_nik)) {
        ALOGV("Peer NIK not available, Ignore NIRA validation");
        return 0;
    }

    os_memset(tag, 0, sizeof(tag));
    os_memset(data, 0, sizeof(data));
    os_memcpy(data, "NIR", NIR_STR_LEN);
    os_memcpy(&data[NIR_STR_LEN], bssid, ETH_ALEN);
    os_memcpy(&data[NIR_STR_LEN + ETH_ALEN], pmkid, NAN_IDENTITY_NONCE_LEN);

    ret = hmac_sha256(entry->peer_nik, NAN_IDENTITY_KEY_LEN, data,
                      sizeof(data), tag);
    if (ret < 0) {
        ALOGE("NAN PASN: Could not derive NIRA Tag, retval = %d", ret);
        return -1;
    }
    if (os_memcmp(tag, &pmkid[NAN_IDENTITY_NONCE_LEN], NAN_IDENTITY_TAG_LEN) != 0) {
        ALOGE("NAN PASN: NIRA TAG mismatch");
        return -1;
    }
    return 0;
}

const u8 * get_nan_subattr(const u8 *ies, size_t len, u8 id)
{
  const nan_subattr *subattr;

  if (!ies)
      return NULL;

  for_each_nan_subattr_id(subattr, id, ies, len)
      return &subattr->id;

  return NULL;
}

const u8 *nan_attr_from_nan_ie(const u8 *nan_ie, enum nan_attr_id attr)
{
  const u8 *nan;
  u8 ie_len = nan_ie[1];

  if (ie_len < NAN_IE_HEADER - 2) {
      ALOGV("%s: NAN IE does not contain attr", __FUNCTION__);
      return NULL;
  }
  nan = nan_ie + NAN_IE_HEADER;

  return get_nan_subattr(nan, 2 + ie_len - NAN_IE_HEADER, attr);
}

const u8 *nan_get_attr_from_ies(const u8 *ies, size_t ies_len,
                                 enum nan_attr_id attr)
{
  const u8 *nan_ie;

  nan_ie = get_vendor_ie(ies, ies_len, NAN_IE_VENDOR_TYPE);
  if (!nan_ie) {
      ALOGV("%s: NAN IE NULL", __FUNCTION__);
      return NULL;
  }

  return nan_attr_from_nan_ie(nan_ie, attr);
}

void nan_pairing_add_setup_ies(struct wpa_secure_nan *secure_nan,
                               struct pasn_data *pasn, int peer_role)
{
    u8 *pos;
    nan_dcea *dcea;
    nan_csia *csia;
    nan_npba *npba;
    u8 *extra_ies;
    size_t extra_ies_len;

    if (!secure_nan || !pasn) {
        ALOGE("%s: Secure NAN/PASN Null ", __FUNCTION__);
        return;
    }

    extra_ies_len = NAN_IE_HEADER + sizeof(nan_dcea) +
                    sizeof(nan_csia) + sizeof(nan_csa) +
                    sizeof(nan_npba);

    if (peer_role == SECURE_NAN_PAIRING_INITIATOR)
        extra_ies_len += sizeof(nan_csa);

    extra_ies = (u8 *)os_zalloc(extra_ies_len);

    if (!extra_ies) {
        ALOGE("%s: Memory allocation failed", __FUNCTION__);
        return;
    }

    pos = extra_ies;

    // NAN IE header
    *pos++ = WLAN_EID_VENDOR_SPECIFIC;
    *pos++ = extra_ies_len - 2;
    WPA_PUT_BE32(pos, NAN_IE_VENDOR_TYPE);
    pos += 4;

    dcea = (nan_dcea *)pos;
    dcea->attr_id = NAN_ATTR_ID_DCEA;
    dcea->len = sizeof(nan_dcea) - offsetof(nan_dcea, cap_info);
    if (secure_nan->enable_pairing_setup)
        dcea->cap_info |= DCEA_PARING_SETUP_ENABLED;
    if (secure_nan->enable_pairing_cache)
        dcea->cap_info |= DCEA_NPK_CACHING_ENABLED;
    pos += sizeof(nan_dcea);

    csia = (nan_csia *)pos;
    csia->attr_id = NAN_ATTR_ID_CSIA;
    csia->len = sizeof(nan_csia) - offsetof(nan_csia, caps);
    csia->len += sizeof(nan_csa);
    csia->caps = secure_nan->csia_cap_info;
    csia->csa[0].cipher = NCS_PK_PASN_128;
    csia->csa[0].pub_id = secure_nan->pub_sub_id;
    if (peer_role == SECURE_NAN_PAIRING_INITIATOR) {
        csia->csa[1].cipher = NCS_SK_128;
        csia->csa[1].pub_id = secure_nan->pub_sub_id;
        csia->len += sizeof(nan_csa);
        pos += sizeof(nan_csa);
    }
    pos += sizeof(nan_csia) + sizeof(nan_csa);

    npba = (nan_npba *)pos;
    npba->attr_id = NAN_ATTR_ID_NPBA;
    npba->len = sizeof(nan_npba) - offsetof(nan_npba, dialog_token);
    npba->dialog_token = 0;
    npba->type_status = 0;
    npba->reason_code = 0;
    npba->bootstrapping_method = secure_nan->supported_bootstrap;

    ALOGV("NAN Pairing Setup IEs: dcea cap_info = %d "
          "npba bootstrapping method = %d", dcea->cap_info,
           npba->bootstrapping_method);
    pasn_set_extra_ies(pasn, extra_ies, extra_ies_len);
    os_free(extra_ies);
}

void nan_pairing_add_verification_ies(struct wpa_secure_nan *secure_nan,
                                      struct pasn_data *pasn, int peer_role)
{
    u8 *pos;
    nan_dcea *dcea;
    nan_csia *csia;
    nan_nira *nira;
    u8 *extra_ies;
    size_t extra_ies_len;

    if (!secure_nan || !pasn || !secure_nan->dev_nik) {
        ALOGE("NAN: NIK not initialized");
        return;
    }

    extra_ies_len = NAN_IE_HEADER + sizeof(nan_dcea) +
                    sizeof(nan_csia) + sizeof(nan_csa) +
                    offsetof(nan_nira, nonce_tag) +
                    secure_nan->dev_nik->nira_nonce_len +
                    secure_nan->dev_nik->nira_tag_len;

    if (peer_role == SECURE_NAN_PAIRING_INITIATOR)
        extra_ies_len += sizeof(nan_csa);

    extra_ies = (u8 *) os_zalloc(extra_ies_len);

    if (!extra_ies) {
        ALOGE("%s: Memory allocation failed", __FUNCTION__);
        return;
    }

    pos = extra_ies;

    // NAN IE header
    *pos++ = WLAN_EID_VENDOR_SPECIFIC;
    *pos++ = extra_ies_len - 2;
    WPA_PUT_BE32(pos, NAN_IE_VENDOR_TYPE);
    pos += 4;

    dcea = (nan_dcea *)pos;
    dcea->attr_id = NAN_ATTR_ID_DCEA;
    dcea->len = sizeof(nan_dcea) - offsetof(nan_dcea, cap_info);
    if (secure_nan->enable_pairing_setup)
        dcea->cap_info |= DCEA_PARING_SETUP_ENABLED;
    if (secure_nan->enable_pairing_cache)
        dcea->cap_info |= DCEA_NPK_CACHING_ENABLED;
     pos += sizeof(nan_dcea);

    csia = (nan_csia *)pos;
    csia->attr_id = NAN_ATTR_ID_CSIA;
    csia->len = sizeof(nan_csia) - offsetof(nan_csia, caps);
    csia->len += sizeof(nan_csa);
    csia->caps = secure_nan->csia_cap_info;
    csia->csa[0].cipher = NCS_PK_PASN_128;
    csia->csa[0].pub_id = secure_nan->pub_sub_id;
    if (peer_role == SECURE_NAN_PAIRING_INITIATOR) {
        csia->csa[1].cipher = NCS_SK_128;
        csia->csa[1].pub_id = secure_nan->pub_sub_id;
        csia->len += sizeof(nan_csa);
        pos += sizeof(nan_csa);
    }
    pos += sizeof(nan_csia) + sizeof(nan_csa);

    nira = (nan_nira *)pos;
    nira->attr_id = NAN_ATTR_ID_NIRA;
    nira->len = 1 + secure_nan->dev_nik->nira_nonce_len +
                 secure_nan->dev_nik->nira_tag_len;
    nira->cipher_ver = 0;
    os_memcpy(nira->nonce_tag, secure_nan->dev_nik->nira_nonce,
              secure_nan->dev_nik->nira_nonce_len);
    os_memcpy(&nira->nonce_tag[secure_nan->dev_nik->nira_nonce_len],
              secure_nan->dev_nik->nira_tag, secure_nan->dev_nik->nira_tag_len);

    ALOGV("NAN Pairing Verification IEs: dcea cap_info = %d", dcea->cap_info);
    pasn_set_extra_ies(pasn, extra_ies, extra_ies_len);
    os_free(extra_ies);
}

struct wpabuf *nan_pairing_generate_rsn_ie(int akmp, int cipher, u8 *pmkid)
{
    u8 *pos;
    u16 capab;
    u32 suite;
    size_t rsne_len;
    struct rsn_ie_hdr *hdr;
    struct wpabuf *buf = NULL;

    ALOGD("NAN: Generate RSNE");

    rsne_len = sizeof(*hdr) + RSN_SELECTOR_LEN +
               2 + RSN_SELECTOR_LEN + 2 + RSN_SELECTOR_LEN +
               2 + RSN_SELECTOR_LEN + 2 + (pmkid ? PMKID_LEN : 0);

    buf = wpabuf_alloc(rsne_len);
    if (!buf) {
        ALOGE("%s: Memory allocation failed", __FUNCTION__);
        return NULL;
    }

    if (wpabuf_tailroom(buf) < rsne_len) {
        ALOGE("%s: wpabuf tail room small", __FUNCTION__);
        wpabuf_free(buf);
        return NULL;
    }

    hdr = (struct rsn_ie_hdr *)wpabuf_put(buf, rsne_len);
    hdr->elem_id = WLAN_EID_RSN;
    hdr->len = rsne_len - 2;
    WPA_PUT_LE16(hdr->version, RSN_VERSION);
    pos = (u8 *) (hdr + 1);

    /* Group addressed data is not allowed */
    RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_NO_GROUP_ADDRESSED);
    pos += RSN_SELECTOR_LEN;

    /* Add the pairwise cipher */
    WPA_PUT_LE16(pos, 1);
    pos += 2;
    suite = wpa_cipher_to_suite(WPA_PROTO_RSN, cipher);
    RSN_SELECTOR_PUT(pos, suite);
    pos += RSN_SELECTOR_LEN;

    /* Add the AKM suite */
    WPA_PUT_LE16(pos, 1);
    pos += 2;

    switch (akmp) {
    case WPA_KEY_MGMT_PASN:
        RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_PASN);
        break;
    case WPA_KEY_MGMT_SAE:
        RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_SAE);
        break;
    default:
        ALOGE("NAN: Invalid AKMP=0x%x", akmp);
        wpabuf_free(buf);
        return NULL;
    }
    pos += RSN_SELECTOR_LEN;

    /* RSN Capabilities: PASN mandates both MFP capable and required */
    capab = WPA_CAPABILITY_MFPC | WPA_CAPABILITY_MFPR;
    WPA_PUT_LE16(pos, capab);
    pos += 2;

    if (pmkid) {
        ALOGD("NAN: Adding PMKID");

        WPA_PUT_LE16(pos, 1);
        pos += 2;
        os_memcpy(pos, pmkid, PMKID_LEN);
        pos += PMKID_LEN;
    } else {
        WPA_PUT_LE16(pos, 0);
        pos += 2;
    }

    /* Group addressed management is not allowed */
    RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_NO_GROUP_ADDRESSED);

    return buf;
}

struct wpabuf *nan_pairing_generate_rsnxe(int akmp)
{
    struct wpabuf *buf = NULL;
    size_t flen;
    u16 capab = 0;

    if (akmp == WPA_KEY_MGMT_SAE)
        capab |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);

    if (!capab) {
        ALOGE("%s: no supported caps", __FUNCTION__);
        return NULL; /* no supported extended RSN capabilities */
    }

    flen = (capab & 0xff00) ? 2 : 1;
    buf = wpabuf_alloc(2 + flen);
    if (!buf) {
        ALOGE("%s: Memory allocation failed", __FUNCTION__);
        return NULL;
    }

    if (wpabuf_tailroom(buf) < 2 + flen) {
        ALOGE("%s: wpabuf tail room small", __FUNCTION__);
        wpabuf_free(buf);
        return NULL;
    }
    capab |= flen - 1; /* bit 0-3 = Field length (n - 1) */

    wpabuf_put_u8(buf, WLAN_EID_RSNX);
    wpabuf_put_u8(buf, flen);
    wpabuf_put_u8(buf, capab & 0x00ff);
    capab >>= 8;
    if (capab)
        wpabuf_put_u8(buf, capab);

    return buf;
}

void nan_pairing_set_password(struct nan_pairing_peer_info *peer, u8 *passphrase,
                              u32 len)
{
    struct sae_pt *pt;
    const u8 *pairing_ssid;
    size_t pairing_ssid_len;

    if (!peer || !passphrase) {
        ALOGE("%s: peer/passphrase NULL", __FUNCTION__);
        return;
    }

    if (peer->passphrase)
        os_free(peer->passphrase);

    pairing_ssid = reinterpret_cast<const u8 *> (NAN_PAIRING_SSID);
    pairing_ssid_len = strlen(NAN_PAIRING_SSID);
    peer->passphrase = (char *)os_zalloc(len + 1);
    if (!peer->passphrase) {
        ALOGE("%s: Mem Alloc for passphrase failed", __FUNCTION__);
        return;
    }
    strlcpy(peer->passphrase, reinterpret_cast<const char *> (passphrase),
            len + 1);
    pt = sae_derive_pt(NULL, pairing_ssid, pairing_ssid_len,
                       (const u8 *)passphrase, len,
                       peer->sae_password_id);
    pasn_set_pt(peer->pasn, pt);
    /* Set passpharse for Pairing Responder to validate PASN auth1 frame*/
    pasn_set_password(peer->pasn, peer->passphrase);
}

void nan_pairing_derive_grp_keys(hal_info *info, u8* addr, u8 cipher_caps)
{
    int groupMfp;
    int len = 0;
    struct nanGrpKey *grp_key;
    struct wpa_secure_nan *secure_nan = info->secure_nan;

    if (!secure_nan) {
        ALOGE("%s: Secure NAN Null ", __FUNCTION__);
        return;
    }

    if (!addr || is_zero_ether_addr(addr)) {
        ALOGE("%s: Invalid NMI Address", __FUNCTION__);
        return;
    }

    grp_key = (struct nanGrpKey *)os_zalloc(sizeof(struct nanGrpKey));
    if (!grp_key) {
        ALOGE("%s: malloc failed", __FUNCTION__);
        return;
    }

    if (NAN_CSIA_GRPKEY_LEN_GET(cipher_caps))
        len = NAN_CSIA_GRPKEY_LEN_32;
    else
        len = NAN_CSIA_GRPKEY_LEN_16;

    secure_nan->csia_cap_info = cipher_caps;
    groupMfp = NAN_CSIA_GRPKEY_SUPPORT_GET(cipher_caps);

    switch (groupMfp) {
    case NAN_GTKSA_IGTKSA_SUPPORTED_BIGTKSA_NOT_SUPPORTED:
        grp_key->igtk_len = len;
        break;
    case NAN_GTKSA_IGTKSA_BIGTKSA_SUPPORTED:
        grp_key->igtk_len = len;
        grp_key->bigtk_len = len;
        break;
    default:
        goto fail;
    }

    if (grp_key->igtk_len == NAN_CSIA_GRPKEY_LEN_16) {
        if (os_get_random(grp_key->igtk, grp_key->igtk_len) < 0) {
            ALOGE("%s: Get random IGTK Failed", __FUNCTION__);
            goto fail;
        }
    } else {
        ALOGE("%s: unsupported IGTK len %d", __FUNCTION__, grp_key->igtk_len);
        goto fail;
    }

    if (grp_key->bigtk_len == NAN_CSIA_GRPKEY_LEN_16) {
        if (os_get_random(grp_key->bigtk, grp_key->bigtk_len) < 0) {
            ALOGE("%s: Get random BIGTK Failed", __FUNCTION__);
            goto fail;
        }
    } else {
        ALOGE("%s: unsupported BIGTK len %d", __FUNCTION__, grp_key->bigtk_len);
        goto fail;
    }

    grp_key->igtk_life_time = GrpKeyLifetime;
    grp_key->bigtk_life_time = GrpKeyLifetime;

    if (secure_nan->dev_grp_keys)
        os_free(secure_nan->dev_grp_keys);

    ALOGD("NAN Group Key: Initializing");
    secure_nan->dev_grp_keys = grp_key;
    return;

fail:
    os_free(grp_key);
    return;
}

void nan_pairing_set_nira(struct wpa_secure_nan *secure_nan)
{
    int ret;
    struct nanIDkey *nik;
    u8 data[NIR_STR_LEN + NAN_IDENTITY_NONCE_LEN + ETH_ALEN];
    u8 tag[NAN_MAX_HASH_LEN];

    if (!secure_nan || !secure_nan->dev_nik) {
        ALOGE("%s: Secure NAN device NIK Null ", __FUNCTION__);
        return;
    }

    nik = secure_nan->dev_nik;

    ret = os_get_random(nik->nira_nonce, NAN_IDENTITY_NONCE_LEN);
    if (ret < 0) {
        ALOGE("%s: Get random NIRA nonce Failed, err = %d", __FUNCTION__, ret);
        return;
    }

    os_memset(data, 0, sizeof(data));
    os_memset(tag, 0, sizeof(tag));
    os_memset(nik->nira_tag, 0, NAN_IDENTITY_TAG_LEN);

    os_memcpy(data, "NIR", NIR_STR_LEN);
    os_memcpy(&data[NIR_STR_LEN], secure_nan->own_addr, ETH_ALEN);
    os_memcpy(&data[NIR_STR_LEN + ETH_ALEN], nik->nira_nonce,
              NAN_IDENTITY_NONCE_LEN);

    ALOGD("NAN PASN:" MACSTR, MAC2STR(secure_nan->own_addr));
    ret = hmac_sha256(nik->nik_data, NAN_IDENTITY_KEY_LEN, data, sizeof(data), tag);
    if (ret < 0) {
        ALOGE("%s: Could not derive NIRA tag, err = %d", __FUNCTION__, ret);
        return;
    }
    os_memcpy(nik->nira_tag, tag, NAN_IDENTITY_TAG_LEN);

    nik->nira_nonce_len = NAN_IDENTITY_NONCE_LEN;
    nik->nira_tag_len = NAN_IDENTITY_TAG_LEN;
}

unsigned int nan_pairing_get_nik_lifetime(struct nanIDkey *nik)
{
    struct os_reltime now;
    os_get_reltime(&now);

    if (nik && nik->expiration > now.sec)
        return (nik->expiration - now.sec);

    return 0;
}

static struct nanIDkey * nan_pairing_nik_init(void)
{
    struct nanIDkey *nik = (struct nanIDkey *)os_zalloc(sizeof(struct nanIDkey));
    if (!nik) {
        ALOGE("NAN NIK: Init failed");
        return NULL;
    }
    return nik;
}

static void nan_pairing_nik_deinit(struct nanIDkey *nik)
{
    os_free(nik);
    ALOGD("NAN NIK: De-Initialize");
    return;
}

int secure_nan_init(wifi_interface_handle iface)
{
    struct wpa_secure_nan *secure_nan = NULL;
    wifi_handle wifiHandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifiHandle);

    if (info->secure_nan) {
        ALOGE("Secure NAN Already initialized");
        return 0;
    }

    if (eloop_init()) {
        ALOGE("Secure NAN eloop init failed");
        return -1;
    }

    secure_nan = (struct wpa_secure_nan *)os_zalloc(sizeof(*secure_nan));

    if (!secure_nan) {
        ALOGE("%s: Memory allocation failed \n", __FUNCTION__);
        eloop_destroy();
        return -1;
    }

    info->secure_nan = secure_nan;
    secure_nan->cb_ctx = wifiHandle;

    //! Initailise peers list
    INITIALISE_LIST(&secure_nan->peers);

    wifi_get_iface_name(iface, secure_nan->iface_name,
                        sizeof(secure_nan->iface_name));
    secure_nan->ptksa = ptksa_cache_init();
    if (!secure_nan->ptksa) {
        ALOGE("Secure NAN PTKSA init failed");
        goto fail;
    }

    secure_nan->dev_nik = nan_pairing_nik_init();
    if (!secure_nan->dev_nik)
        goto fail;

    secure_nan->initiator_pmksa = nan_pairing_initiator_pmksa_cache_init();
    if (!secure_nan->initiator_pmksa) {
        ALOGE("Secure NAN Initiator PMKSA cache init failed");
        goto fail;
    }

    secure_nan->responder_pmksa = nan_pairing_responder_pmksa_cache_init();
    if (!secure_nan->responder_pmksa) {
        ALOGE("Secure NAN Responder PMKSA cache init failed");
        goto fail;
    }

    if (nan_pairing_register_pasn_auth_frames(iface)) {
        ALOGE("Secure NAN Register PASN auth failed");
        goto fail;
    }

    eloop_run();

    return 0;

fail:
    secure_nan_deinit(info);
    return -1;
}

int secure_nan_cache_flush(hal_info *info)
{
    if(!info->secure_nan) {
       ALOGE("Secure NAN == NULL");
       return -1;
    }
    if (info->secure_nan->ptksa)
        ptksa_cache_flush(info->secure_nan->ptksa, NULL, WPA_CIPHER_NONE);

    nan_pairing_initiator_pmksa_cache_flush(info->secure_nan->initiator_pmksa);
    nan_pairing_responder_pmksa_cache_flush(info->secure_nan->responder_pmksa);
    nan_pairing_delete_list(info->secure_nan);
    return 0;
}

int secure_nan_deinit(hal_info *info)
{
    if(!info->secure_nan) {
       ALOGE("Secure NAN == NULL");
       return -1;
    }

    if (info->secure_nan->ptksa)
        ptksa_cache_deinit(info->secure_nan->ptksa);

    if (info->secure_nan->dev_nik)
        nan_pairing_nik_deinit(info->secure_nan->dev_nik);

    nan_pairing_initiator_pmksa_cache_deinit(info->secure_nan->initiator_pmksa);
    nan_pairing_responder_pmksa_cache_deinit(info->secure_nan->responder_pmksa);
    nan_pairing_delete_list(info->secure_nan);
    eloop_destroy();

    os_free(info->secure_nan);
    info->secure_nan = NULL;
    return 0;
}

/*  Function to end pairing */
wifi_error nan_pairing_end(transaction_id id,
                           wifi_interface_handle iface,
                           NanPairingEndRequest *msg)
{
    wifi_handle wifiHandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifiHandle);
    struct nan_pairing_peer_info *peer;

    if (!info || !info->secure_nan) {
        ALOGE("%s: Error hal_info NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    peer = nan_pairing_get_peer_from_id(info->secure_nan, msg->pairing_instance_id);
    if (peer) {
        nan_pairing_set_key(info, WPA_ALG_NONE, peer->bssid, 0, 0, NULL, 0,
                            NULL, 0, KEY_FLAG_PAIRWISE);
        nan_pairing_delete_peer_from_list(info->secure_nan, peer->bssid);
    }
    return WIFI_SUCCESS;
}

#else  /* WPA_PASN_LIB */

struct nan_pairing_peer_info*
nan_pairing_add_peer_to_list(struct wpa_secure_nan *secure_nan, u8 *mac)
{
   return NULL;
}

struct nan_pairing_peer_info*
nan_pairing_get_peer_from_list(struct wpa_secure_nan *secure_nan, u8 *mac)
{
    return NULL;
}

struct nan_pairing_peer_info*
nan_pairing_get_peer_from_id(struct wpa_secure_nan *secure_nan, u32 pairing_id)
{
  return NULL;
}

void nan_pairing_delete_list(struct wpa_secure_nan *secure_nan)
{
   return;
}

void nan_pairing_delete_peer_from_list(struct wpa_secure_nan *secure_nan,
                                       u8 *mac)
{
   return;
}

int secure_nan_init(wifi_interface_handle iface)
{
    ALOGE("Secure NAN init not supported");
    return -1;
}

int secure_nan_cache_flush(hal_info *info)
{
    ALOGE("Secure NAN cache flush not supported");
    return -1;
}

int secure_nan_deinit(hal_info *info)
{
    ALOGE("Secure NAN deinit not supported");
    return -1;
}

wifi_error nan_get_pairing_tk(transaction_id id,
                              wifi_interface_handle iface,
                              NanPairingTK *msg)
{
    ALOGE("NAN Pairing get TK not supported");
    return WIFI_ERROR_NOT_SUPPORTED;
}

wifi_error nan_get_pairing_pmkid(transaction_id id,
                                 wifi_interface_handle iface,
                                 NanPairingPmkid *msg)
{
    ALOGE("NAN Pairing get PMKID not supported");
    return WIFI_ERROR_NOT_SUPPORTED;
}

void nan_pairing_set_nira(struct wpa_secure_nan *secure_nan)
{
    ALOGE("NAN Pairing set NIRA not supported");
}

wifi_error nan_pairing_end(transaction_id id,
                           wifi_interface_handle iface,
                           NanPairingEndRequest *msg)
{
    ALOGE("NAN Pairing end not supported");
    return WIFI_ERROR_NOT_SUPPORTED;
}

int nan_handle_pn_response(wifi_handle handle, transaction_id id, u8 *key_rsc)
{
    ALOGE("Secure NAN Group keys not supported");
    return -1;
}

#endif /* WPA_PASN_LIB */
