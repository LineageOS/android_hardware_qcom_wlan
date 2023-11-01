/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc.All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "wifihal_list.h"
#include "wifi_hal.h"
#include "nan_i.h"
#include "nancommand.h"
#include "common.h"
#include "cpp_bindings.h"
#include <utils/Log.h>
#include <errno.h>

#ifdef WPA_PASN_LIB
#define NAN_PAIRING_SSID "516F9A010000"

/* NAN Identity key lifetime in seconds */
static const int NIKLifetime = 43200;
/* NAN group key lifetime in seconds */
static const int GrpKeyLifetime = 43200;
static int nan_pairing_set_key(hal_info *info, int alg, const u8 *addr,
                               int key_idx, int set_tx, const u8 *seq,
                               size_t seq_len, const u8 *key, size_t key_len,
                               int key_flag);

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
           wpa_pasn_reset(&entry->pasn);
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

    mentry->pasn.cb_ctx = secure_nan->cb_ctx;
    mentry->pasn.send_mgmt = nan_send_tx_mgmt;
    mentry->pasn.validate_custom_pmkid = nan_pairing_validate_custom_pmkid;
    wpa_pasn_reset(&mentry->pasn);
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

void nan_pairing_delete_list(struct wpa_secure_nan *secure_nan)
{
    struct nan_pairing_peer_info *entry, *tmp;

    list_for_each_entry_safe(entry, tmp, &secure_nan->peers, list) {
         del_from_list(&entry->list);

         if (entry->passphrase)
             free(entry->passphrase);

         if (entry->pasn.extra_ies)
             free((u8 *)entry->pasn.extra_ies);

         wpa_pasn_reset(&entry->pasn);

         if (entry->frame)
             free(entry->frame);

         free(entry);
    }
}

void nan_pairing_delete_peer_from_list(struct wpa_secure_nan *secure_nan,
                                       u8 *mac)
{
    struct nan_pairing_peer_info *entry, *tmp;

    list_for_each_entry_safe(entry, tmp, &secure_nan->peers, list) {
       if (memcmp(entry->bssid, mac, ETH_ALEN) == 0) {
                 del_from_list(&entry->list);

                 if (entry->passphrase)
                     free(entry->passphrase);

                 if (entry->pasn.extra_ies)
                     free((u8 *)entry->pasn.extra_ies);

                 wpa_pasn_reset(&entry->pasn);

                 if (entry->frame)
                     free(entry->frame);

                 free(entry);
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
    return res;
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

int nan_send_tx_mgmt(void *ctx, const u8 *frame_buf, size_t frame_len,
                     int noack, unsigned int freq, unsigned int wait_dur)
{
    wifi_handle handle = (wifi_handle)ctx;
    hal_info *info = getHalInfo(handle);
    struct nl_msg * msg;
    int err = 0, l = 0;
    u32 i, idx;

    msg = nlmsg_alloc();

    if (!msg) {
        ALOGE("%s: Memory allocation failed \n", __FUNCTION__);
        return -1;
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

wifi_error nan_validate_shared_key_desc(hal_info *info, const u8 *addr, u8 *buf,
                                        u16 len)
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
            goto fail;

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
                 nan_pairing_set_key(info, WPA_CIPHER_GCMP, addr,
                                     NAN_IGTK_KEY_IDX, 1, NULL, 0,
                                     igtk_kde->igtk, key_len,
                                     KEY_FLAG_GROUP_RX);
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
                 nan_pairing_set_key(info, WPA_CIPHER_GCMP, addr,
                                     NAN_BIGTK_KEY_IDX, 1, NULL, 0,
                                     bigtk_kde->bigtk, key_len,
                                     KEY_FLAG_GROUP_RX);
             } else {
                 ALOGE("%s: unsupported BIGTK len", __FUNCTION__);
             }
             break;
        case NAN_KDE_TYPE_BIGTK_LIFETIME:
             bigtk_lifetime_kde = (struct bigtkLifetime *)nan_kde->data;
             ALOGV("%s: received BIGTK Lifetime: %d", __FUNCTION__,
                   bigtk_lifetime_kde->lifetime);
        default:
           ALOGE("NAN: Invalid Shared key KDE, DataType=%d", nan_kde->dataType);
           break;
        }

skip_kde:
        pos += 2 + nan_kde->length;
        remainingLen -= 2 + nan_kde->length;
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

    if (!info || !info->secure_nan || !info->secure_nan->dev_nik) {
        ALOGE("%s: secure nan NULL", __FUNCTION__);
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

/* IGTK */
    if (grp_keys && grp_keys->igtk_len) {
        nan_kde = (struct nanKDE *)pos;
        nan_kde->type = NAN_VENDOR_ATTR_TYPE;
        nan_kde->length = oui_offset + sizeof(struct igtkKDE) +
                          grp_keys->igtk_len;
        WPA_PUT_BE24(nan_kde->oui, OUI_WFA);
        nan_kde->dataType = NAN_KDE_TYPE_IGTK;
        pos += sizeof(struct nanKDE);

        igtk_kde = (struct igtkKDE *)pos;
        igtk_kde->keyid[0] = NAN_IGTK_KEY_IDX;
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
    if (grp_keys && grp_keys->bigtk_len) {
        nan_kde = (struct nanKDE *)pos;
        nan_kde->type = NAN_VENDOR_ATTR_TYPE;
        nan_kde->length = oui_offset + sizeof(struct bigtkKDE) +
                          grp_keys->bigtk_len;
        WPA_PUT_BE24(nan_kde->oui, OUI_WFA);
        nan_kde->dataType = NAN_KDE_TYPE_BIGTK;
        pos += sizeof(struct nanKDE);

        bigtk_kde = (struct bigtkKDE *)pos;
        bigtk_kde->keyid[0] = NAN_BIGTK_KEY_IDX;
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

static int nan_pairing_set_key(hal_info *info, int alg, const u8 *addr,
                               int key_idx, int set_tx, const u8 *seq,
                               size_t seq_len, const u8 *key, size_t key_len,
                               int key_flag)
{
    int idx;
    u32 suite;
    struct nl_msg *msg;
    struct nl_msg *key_msg;
    int ret = 0;

    idx = if_nametoindex(DEFAULT_NAN_IFACE);

    if (check_key_flag((enum key_flag) key_flag)) {
        ALOGE("%s: invalid key_flag", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    msg = nlmsg_alloc();
    key_msg = nlmsg_alloc();
    if (!msg || !key_msg) {
        ALOGE("%s: Memory allocation failed\n", __FUNCTION__);
        return WIFI_ERROR_OUT_OF_MEMORY;
    }

    if ((key_flag & KEY_FLAG_PAIRWISE_MASK) ==
        KEY_FLAG_PAIRWISE_RX_TX_MODIFY) {
        ALOGV("%s: nl80211: SET_KEY (pairwise RX/TX modify)", __FUNCTION__);
        genlmsg_put(msg, 0, 0, info->nl80211_family_id, 0, 0,
                    NL80211_CMD_SET_KEY, 0);
        nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx);
        if(!msg)
           goto fail2;
    } else if (alg == WPA_ALG_NONE && (key_flag & KEY_FLAG_RX_TX)) {
        ALOGE("%s: invalid key_flag to delete key", __FUNCTION__);
        ret = WIFI_ERROR_INVALID_ARGS;
        goto fail2;
    } else if (alg == WPA_ALG_NONE) {
        ALOGV("%s: nl80211: DEL_KEY", __FUNCTION__);
        genlmsg_put(msg, 0, 0, info->nl80211_family_id, 0, 0,
                    NL80211_CMD_DEL_KEY, 0);
        nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx);
        if(!msg)
           goto fail2;
    } else {
        suite = wpa_alg_to_cipher_suite((enum wpa_alg) alg, key_len);
        if (!suite) {
            ret = WIFI_ERROR_INVALID_ARGS;
            goto fail2;
        }
        ALOGV("%s: nl80211: NEW_KEY", __FUNCTION__);
        genlmsg_put(msg, 0, 0, info->nl80211_family_id, 0, 0,
                    NL80211_CMD_NEW_KEY, 0);
        nla_put_u32(msg, NL80211_ATTR_IFINDEX, idx);
        if (!msg)
            goto fail2;
        if (nla_put(key_msg, NL80211_KEY_DATA, key_len, key) ||
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
            if (nla_put_u32(key_msg, NL80211_KEY_TYPE,
                            NL80211_KEYTYPE_GROUP))
                goto fail;
        } else if (!(key_flag & KEY_FLAG_PAIRWISE)) {
            ALOGV("%s:  key_flag missing PAIRWISE when setting a pairwise key", __FUNCTION__);
            ret = WIFI_ERROR_INVALID_ARGS;
            goto fail;
        } else {
            ALOGV("%s: pairwise key", __FUNCTION__);
        }
    } else if ((key_flag & KEY_FLAG_PAIRWISE) ||
               !(key_flag & KEY_FLAG_GROUP)) {
        ALOGE("%s: invalid key_flag for a broadcast key", __FUNCTION__);
        ret = WIFI_ERROR_INVALID_ARGS;
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
fail2:
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
    pasn = &peer->pasn;
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
    if (!(peer->dcea_cap_info & DCEA_NPK_CACHING_ENABLED)) {
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

        if (pasn->akmp == WPA_KEY_MGMT_PASN)
            evt.npk_security_association.akm = PASN;
        else
            evt.npk_security_association.akm = SAE;

        if (info->secure_nan->dev_nik)
            memcpy(evt.npk_security_association.local_nan_identity_key,
                   info->secure_nan->dev_nik->nik_data,
                   NAN_IDENTITY_KEY_LEN);

        evt.npk_security_association.npk.pmk_len = pasn->pmk_len;
        memcpy(evt.npk_security_association.npk.pmk, pasn->pmk,
               pasn->pmk_len);

        wpa_pasn_reset(pasn);
        nanCommand->handleNanPairingConfirm(&evt);
        peer->is_paired = true;
        peer->is_pairing_in_progress = false;
    } else if (peer_role == SECURE_NAN_PAIRING_RESPONDER) {
      NanSharedKeyRequest msg;
      if (nan_get_shared_key_descriptor(info, peer->bssid, &msg)) {
          ALOGE("NAN: Unable to get shared key descriptor");
          return -1;
      }
      memcpy(msg.peer_disc_mac_addr, peer->bssid, NAN_MAC_ADDR_LEN);
      msg.requestor_instance_id = peer->requestor_instance_id;
      msg.pub_sub_id = peer->pub_sub_id;
      nan_sharedkey_followup_request(0, ifaceHandle, &msg);
    }
    return WIFI_SUCCESS;
}

static int nan_pairing_register_pasn_auth_frames(wifi_interface_handle iface)
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

    /* wlan type:mgmt, wlan subtype: auth */
    u16 type = (WLAN_FC_TYPE_MGMT << 2) | (WLAN_FC_STYPE_AUTH << 4);
    /* register for PASN Authentication frames */
    const u8 pasn_auth_match[2] = {7,0};
    nla_put_u16(msg, NL80211_ATTR_FRAME_TYPE, type);
    nla_put(msg, NL80211_ATTR_FRAME_MATCH, 2, pasn_auth_match);

    nan_send_nl_msg_event_sock(info, msg);

    if (msg)
        nlmsg_free(msg);

    return 0;
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

    if (!secure_nan || !pasn) {
        ALOGE("%s: Secure NAN/PASN Null ", __FUNCTION__);
        return;
    }

    pasn->extra_ies_len = NAN_IE_HEADER + sizeof(nan_dcea) +
                          sizeof(nan_csia) + sizeof(nan_csa) +
                          sizeof(nan_npba);

    if (peer_role == SECURE_NAN_PAIRING_INITIATOR)
        pasn->extra_ies_len += sizeof(nan_csa);

    if (pasn->extra_ies)
        os_free((u8 *)pasn->extra_ies);

    extra_ies = (u8 *)os_zalloc(pasn->extra_ies_len);

    if (!extra_ies) {
        ALOGE("%s: Memory allocation failed", __FUNCTION__);
        pasn->extra_ies_len = 0;
        return;
    }

    pos = extra_ies;

    // NAN IE header
    *pos++ = WLAN_EID_VENDOR_SPECIFIC;
    *pos++ = pasn->extra_ies_len - 2;
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
    csia->caps = 0;
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
    pasn->extra_ies = extra_ies;
}

void nan_pairing_add_verification_ies(struct wpa_secure_nan *secure_nan,
                                      struct pasn_data *pasn, int peer_role)
{
    u8 *pos;
    nan_dcea *dcea;
    nan_csia *csia;
    nan_nira *nira;
    u8 *extra_ies;

    if (!secure_nan || !pasn || !secure_nan->dev_nik) {
        ALOGE("NAN: NIK not initialized");
        return;
    }

    pasn->extra_ies_len = NAN_IE_HEADER + sizeof(nan_dcea) +
                          sizeof(nan_csia) + sizeof(nan_csa) +
                          offsetof(nan_nira, nonce_tag) +
                          secure_nan->dev_nik->nira_nonce_len +
                          secure_nan->dev_nik->nira_tag_len;

    if (peer_role == SECURE_NAN_PAIRING_INITIATOR)
        pasn->extra_ies_len += sizeof(nan_csa);

    if (pasn->extra_ies)
        os_free((u8 *)pasn->extra_ies);

    extra_ies = (u8 *) os_zalloc(pasn->extra_ies_len);

    if (!extra_ies) {
        ALOGE("%s: Memory allocation failed", __FUNCTION__);
        pasn->extra_ies_len = 0;
        return;
    }

    pos = extra_ies;

    // NAN IE header
    *pos++ = WLAN_EID_VENDOR_SPECIFIC;
    *pos++ = pasn->extra_ies_len - 2;
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
    csia->caps = 0;
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
    pasn->extra_ies = extra_ies;
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
#ifdef CONFIG_SAE
    case WPA_KEY_MGMT_SAE:
        RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_SAE);
        break;
#endif /* CONFIG_SAE */
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
    strlcpy(peer->passphrase, reinterpret_cast<const char *> (passphrase),
            len + 1);
    peer->pasn.pt = sae_derive_pt(NULL, pairing_ssid, pairing_ssid_len,
                                  (const u8 *)passphrase, len,
                                  peer->sae_password_id);
    /* Set passpharse for Pairing Responder to validate PASN auth1 frame*/
    peer->pasn.password = peer->passphrase;
}

void nan_pairing_derive_grp_keys(hal_info *info, u8* addr, u32 cipher_caps)
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
        if (random_get_bytes(grp_key->igtk, grp_key->igtk_len) < 0) {
            ALOGE("%s: Get random IGTK Failed", __FUNCTION__);
            goto fail;
        }
        if (nan_pairing_set_key(info, WPA_CIPHER_GCMP, addr, NAN_IGTK_KEY_IDX, 1,
                                NULL, 0, grp_key->igtk, grp_key->igtk_len,
                                KEY_FLAG_GROUP_RX)) {
             ALOGE("%s: set pairing key IGTK failed", __FUNCTION__);
             goto fail;
        }
    } else {
        ALOGE("%s: unsupported IGTK len %d", __FUNCTION__, grp_key->igtk_len);
        goto fail;
    }

    if (grp_key->bigtk_len == NAN_CSIA_GRPKEY_LEN_16) {
        if (random_get_bytes(grp_key->bigtk, grp_key->bigtk_len) < 0) {
            ALOGE("%s: Get random BIGTK Failed", __FUNCTION__);
            goto fail;
        }
        if (nan_pairing_set_key(info, WPA_CIPHER_GCMP, addr, NAN_BIGTK_KEY_IDX, 1,
                                NULL, 0, grp_key->bigtk, grp_key->bigtk_len,
                                KEY_FLAG_GROUP_RX)) {
             ALOGE("%s: set pairing key BIGTK failed", __FUNCTION__);
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

    ret = random_get_bytes(nik->nira_nonce, NAN_IDENTITY_NONCE_LEN);
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

    secure_nan = (struct wpa_secure_nan *)os_zalloc(sizeof(*secure_nan));

    if (!secure_nan) {
        ALOGE("%s: Memory allocation failed \n", __FUNCTION__);
        return -1;
    }

    if (eloop_init()) {
        ALOGE("Secure NAN eloop init failed");
        return -1;
    }

    secure_nan->cb_ctx = wifiHandle;
    wifi_get_iface_name(iface, secure_nan->iface_name,
                        sizeof(secure_nan->iface_name));
    secure_nan->ptksa = ptksa_cache_init();
    if (!secure_nan->ptksa) {
        ALOGE("Secure NAN PTKSA init failed");
        return -1;
    }

    secure_nan->dev_nik = nan_pairing_nik_init();
    if (!secure_nan->dev_nik)
        return -1;

    secure_nan->initiator_pmksa = nan_pairing_initiator_pmksa_cache_init();
    if (!secure_nan->initiator_pmksa) {
        ALOGE("Secure NAN Initiator PMKSA cache init failed");
        return -1;
    }

    secure_nan->responder_pmksa = nan_pairing_responder_pmksa_cache_init();
    if (!secure_nan->responder_pmksa) {
        ALOGE("Secure NAN Responder PMKSA cache init failed");
        return -1;
    }

    info->secure_nan = secure_nan;
    //! Initailise peers list
    INITIALISE_LIST(&secure_nan->peers);

    if (nan_pairing_register_pasn_auth_frames(iface)) {
        ALOGE("Secure NAN Register PASN auth failed");
        return -1;
    }

    eloop_run();

    return 0;
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

#endif /* WPA_PASN_LIB */
