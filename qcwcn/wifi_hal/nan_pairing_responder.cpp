/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc.All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided ?with the distribution.
 *
 * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <hardware_legacy/wifi_hal.h>
#include "nan_i.h"
#include "nancommand.h"

#ifdef WPA_PASN_LIB
#ifdef __cplusplus
extern "C" {
#endif
#include "ap/pmksa_cache_auth.h"
#ifdef __cplusplus
}
#endif

/*
 * Note: Wi-Fi Aware device can act as PASN initiator with one peer and as
 * PASN responder with other peer, so they maintain separate PMKSA cache
 * for each role. This wrapper functions helps to initialise struct
 * rsn_pmksa_cache which is different for initiator and responder.
 */
struct rsn_pmksa_cache * nan_pairing_responder_pmksa_cache_init(void)
{
   return pmksa_cache_auth_init(NULL, NULL);
}


void nan_pairing_responder_pmksa_cache_deinit(struct rsn_pmksa_cache *pmksa)
{
   return pmksa_cache_auth_deinit(pmksa);
}

int nan_pairing_responder_pmksa_cache_add(struct rsn_pmksa_cache *pmksa,
                                          u8 *own_addr, u8 *bssid, u8 *pmk,
                                          u32 pmk_len)
{
   if (pmksa_cache_auth_add(pmksa, pmk, pmk_len, NULL, NULL, 0, own_addr,
                            bssid, 0, NULL, WPA_KEY_MGMT_SAE))
          return 0;
    return -1;
}

int nan_pairing_responder_pmksa_cache_get(struct rsn_pmksa_cache *pmksa,
                                          u8 *bssid, u8 *pmkid)
{
    struct rsn_pmksa_cache_entry *entry;

    entry = pmksa_cache_auth_get(pmksa, bssid, NULL);
    if (entry) {
          os_memcpy(pmkid, entry->pmkid, PMKID_LEN);
          return 0;
    }
    return -1;
}

void nan_pairing_responder_pmksa_cache_flush(struct rsn_pmksa_cache *pmksa)
{
    return pmksa_cache_auth_flush(pmksa);
}

void NanCommand::notifyPairingResponderResponse(transaction_id id, u32 pairing_id)
{
    NanResponseMsg rsp_data;

    if (mHandler.NotifyResponse) {
        memset(&rsp_data, 0, sizeof(rsp_data));
        rsp_data.status = NAN_STATUS_SUCCESS;
        rsp_data.response_type = NAN_PAIRING_RESPONDER_RESPONSE;
        rsp_data.body.pairing_request_response.paring_instance_id = pairing_id;
        (*mHandler.NotifyResponse)(id, &rsp_data);
    }
}

void nan_pairing_notify_responder_response(wifi_handle handle, u8 *bssid)
{
    hal_info *info = getHalInfo(handle);
    struct nan_pairing_peer_info *peer;
    NanCommand *nanCommand = NULL;

    nanCommand = NanCommand::instance(handle);
    if (nanCommand == NULL) {
        ALOGE("%s: Error NanCommand NULL", __FUNCTION__);
        return;
    }

    peer = nan_pairing_get_peer_from_list(info->secure_nan, bssid);
    if (!peer) {
        ALOGE("nl80211: Peer not found in the pairing list");
        return;
    }

    if (peer->trans_id_valid) {
        nanCommand->notifyPairingResponderResponse(peer->trans_id,
                                                   peer->pairing_instance_id);
        peer->trans_id_valid = false;
   }
}

wifi_error nan_pairing_indication_response(transaction_id id,
                                           wifi_interface_handle iface,
                                           NanPairingIndicationResponse* msg)
{
    struct pasn_data *pasn;
    struct nan_pairing_peer_info *peer;
    NanCommand *nanCommand = NULL;
    wifi_handle wifiHandle = getWifiHandle(iface);
    hal_info *info = getHalInfo(wifiHandle);
    const struct ieee80211_mgmt *mgmt = NULL;
    struct wpa_secure_nan *secure_nan;
    int ret;

    if (!info) {
        ALOGE("%s: Error hal_info NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    secure_nan = info->secure_nan;
    if (!secure_nan) {
        ALOGE("%s: Error secure nan NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    if (!msg) {
        ALOGE("%s: msg req invalid", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    nanCommand = NanCommand::instance(wifiHandle);
    if (nanCommand == NULL) {
        ALOGE("%s: Error NanCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    if (is_zero_ether_addr(nanCommand->getClusterAddr())) {
        ALOGE("%s: Invalid Cluster Address", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    peer = nan_pairing_get_peer_from_id(secure_nan, msg->pairing_instance_id);
    if (!peer) {
        ALOGE("%s: peer not found, pairing id: %d", __FUNCTION__,
              msg->pairing_instance_id);
        return WIFI_ERROR_INVALID_ARGS;
    }

    if (!peer->frame) {
        ALOGE("%s: no auth frame to process", __FUNCTION__);
        peer->is_pairing_in_progress = false;
        return WIFI_ERROR_UNKNOWN;
    }

    pasn = &peer->pasn;
    pasn->derive_kdk = true;
    pasn->kdk_len = WPA_KDK_MAX_LEN;
    peer->peer_role = SECURE_NAN_PAIRING_INITIATOR;

    if (msg->rsp_code == NAN_PAIRING_REQUEST_REJECT) {
        ALOGE("%s: received reject rsp", __FUNCTION__);
        peer->is_pairing_in_progress = false;
        goto fail;
    }

    mgmt = (struct ieee80211_mgmt *)peer->frame->data;
    memcpy(pasn->own_addr, nanCommand->getNmi(), NAN_MAC_ADDR_LEN);
    memcpy(pasn->bssid, nanCommand->getClusterAddr(), NAN_MAC_ADDR_LEN);
    os_memcpy(pasn->peer_addr, (u8 *)mgmt->sa, NAN_MAC_ADDR_LEN);

    if (msg->cipher_type == NAN_CIPHER_SUITE_PUBLIC_KEY_PASN_256_MASK) {
        pasn->cipher = WPA_CIPHER_CCMP_256;
        pasn->rsn_pairwise = WPA_CIPHER_CCMP_256;
    } else {
        pasn->cipher = WPA_CIPHER_CCMP;
        pasn->rsn_pairwise = WPA_CIPHER_CCMP;
    }

    if (msg->nan_pairing_request_type == NAN_PAIRING_VERIFICATION) {
        if (msg->akm == SAE) {
            pasn->akmp = WPA_KEY_MGMT_SAE;
            pasn->wpa_key_mgmt = WPA_KEY_MGMT_SAE;
            pasn->rsnxe_capab |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);
        } else {
            pasn->akmp = WPA_KEY_MGMT_PASN;
            pasn->wpa_key_mgmt = WPA_KEY_MGMT_PASN;
        }

        // Configure NIK from the user.
        memcpy(secure_nan->dev_nik->nik_data, msg->nan_identity_key,
               NAN_IDENTITY_KEY_LEN);
        secure_nan->dev_nik->nik_len = NAN_IDENTITY_KEY_LEN;
        nan_pairing_set_nira(info->secure_nan);

        if ((secure_nan->dev_nik->nira_nonce_len +
             secure_nan->dev_nik->nira_tag_len) > PMKID_LEN) {
            ALOGE("%s: Invalid nonce/tag len, nonce_len = %d, tag len = %d",
                  __FUNCTION__, secure_nan->dev_nik->nira_nonce_len,
                  secure_nan->dev_nik->nira_tag_len);
            goto fail;
        } else {
            os_memcpy(pasn->custom_pmkid, secure_nan->dev_nik->nira_nonce,
                      secure_nan->dev_nik->nira_nonce_len);
            os_memcpy(&pasn->custom_pmkid[secure_nan->dev_nik->nira_nonce_len],
                      secure_nan->dev_nik->nira_tag,
                      secure_nan->dev_nik->nira_tag_len);
            pasn->custom_pmkid_valid = true;
        }
        // construct wrapped data for csia, nira
        nan_pairing_add_verification_ies(secure_nan, pasn, peer->peer_role);

        if (msg->key_info.key_type == NAN_SECURITY_KEY_INPUT_PMK &&
            msg->akm == SAE) {
            if (!msg->key_info.body.pmk_info.pmk_len ||
                nan_pairing_responder_pmksa_cache_add(secure_nan->responder_pmksa,
                                                      pasn->own_addr,
                                                      pasn->peer_addr,
                                                      msg->key_info.body.pmk_info.pmk,
                                                      msg->key_info.body.pmk_info.pmk_len)) {
                ALOGE("pmksa cache add failed for peer=" MACSTR " and pmk len=%d ",
                      MAC2STR(pasn->peer_addr),
                      msg->key_info.body.pmk_info.pmk_len);
                goto fail;
            }
        }
    } else {
        if (!msg->is_opportunistic) {
            pasn->akmp = WPA_KEY_MGMT_SAE;
            pasn->wpa_key_mgmt = WPA_KEY_MGMT_SAE;
            pasn->rsnxe_capab |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);
        } else {
            pasn->akmp = WPA_KEY_MGMT_PASN;
            pasn->wpa_key_mgmt = WPA_KEY_MGMT_PASN;
        }
        if (msg->key_info.key_type == NAN_SECURITY_KEY_INPUT_PASSPHRASE) {
            nan_pairing_set_password(peer,
                             msg->key_info.body.passphrase_info.passphrase,
                             msg->key_info.body.passphrase_info.passphrase_len);

        } else if (msg->key_info.key_type == NAN_SECURITY_KEY_INPUT_PMK) {
            ALOGE("%s: Error key type PMK is invalid ", __FUNCTION__);
            goto fail;
        }
        // construct wrapped data for dcea, csia, npba
        nan_pairing_add_setup_ies(secure_nan, pasn, peer->peer_role);
    }

    if (secure_nan->rsnxe)
        wpabuf_free(secure_nan->rsnxe);

    secure_nan->rsnxe = nan_pairing_generate_rsnxe(pasn->akmp);
    if (secure_nan->rsnxe)
        pasn->rsnxe_ie = wpabuf_head_u8(secure_nan->rsnxe);

    pasn->pmksa = secure_nan->responder_pmksa;
    peer->trans_id = id;
    peer->trans_id_valid = true;
    ret = handle_auth_pasn_1(pasn, pasn->own_addr, (u8 *)mgmt->sa, mgmt,
                             peer->frame->len);
    if (ret == -1) {
        ALOGE("%s: Handle auth pasn 1 failed", __FUNCTION__);
        wpa_pasn_reset(pasn);
        peer->peer_role = SECURE_NAN_IDLE;
        goto fail;
    }
    free(peer->frame);
    peer->frame = NULL;
    wifi_get_iface_name(iface, secure_nan->iface_name,
                        sizeof(secure_nan->iface_name));

    return WIFI_SUCCESS;

fail:
    free(peer->frame);
    peer->frame = NULL;
    peer->is_pairing_in_progress = false;
    return WIFI_ERROR_UNKNOWN;
}

int nan_pairing_handle_pasn_auth(wifi_handle handle, const u8 *data, size_t len)
{
    int ret = 0;
    struct pasn_data *pasn;
    const u8 *nan_attr_ie;
    bool nira_present = false;
    NanCommand *nanCommand = NULL;
    hal_info *info = getHalInfo(handle);
    u16 auth_alg, auth_transaction, status_code;
    struct nan_pairing_peer_info *entry;
    u8 nira_nonce[NAN_IDENTITY_NONCE_LEN];
    u8 nira_tag[NAN_IDENTITY_TAG_LEN];
    const struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *) data;

    nanCommand = NanCommand::instance(handle);
    if (nanCommand == NULL) {
        ALOGE("%s: Error NanCommand NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    if (!mgmt) {
        ALOGE("%s: PASN mgmt frame NULL", __FUNCTION__);
        return WIFI_ERROR_UNKNOWN;
    }

    if (os_memcmp(mgmt->da, nanCommand->getNmi(), NAN_MAC_ADDR_LEN) != 0) {
        ALOGE("PASN Responder: Not our frame");
        return WIFI_ERROR_UNKNOWN;
    }

    auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
    status_code = le_to_host16(mgmt->u.auth.status_code);

    auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);

    if(auth_alg != WLAN_AUTH_PASN || auth_transaction == 2) {
       ALOGE("PASN Responder: Not PASN frame/Unexpected auth frame, auth_alg = %d",
              auth_alg);
       return WIFI_ERROR_UNKNOWN;
    }

    if (memcmp(info->secure_nan->own_addr, nanCommand->getNmi(), NAN_MAC_ADDR_LEN) != 0) {
        memcpy(info->secure_nan->own_addr, nanCommand->getNmi(), NAN_MAC_ADDR_LEN);
    }

    /* PASN authentication M1 frame processing */
    if (auth_transaction == 1) {

        nan_attr_ie = nan_get_attr_from_ies(mgmt->u.auth.variable,
                         len - offsetof(struct ieee80211_mgmt, u.auth.variable),
                         NAN_ATTR_ID_NIRA);

        entry = nan_pairing_get_peer_from_list(info->secure_nan,
                                               (u8 *)mgmt->sa);
        if (nan_attr_ie) {
            if (!entry || !entry->is_paired) {
                ALOGI("PASN Responder: NIRA present, but peer entry not found");
                return WIFI_ERROR_UNKNOWN;
            }

            nan_nira *nira = (nan_nira *)nan_attr_ie;
            nira_present = true;
            memcpy(nira_nonce, nira->nonce_tag, NAN_IDENTITY_NONCE_LEN);
            memcpy(nira_tag, &nira->nonce_tag[NAN_IDENTITY_NONCE_LEN],
                   NAN_IDENTITY_TAG_LEN);
        }

        if (entry && entry->is_pairing_in_progress) {
            ALOGV("PASN Responder: Drop PASN M1 frame as Pairing in progress");
            return WIFI_ERROR_UNKNOWN;
        }

        entry = nan_pairing_add_peer_to_list(info->secure_nan, (u8 *)mgmt->sa);
        if (!entry) {
            ALOGE("PASN Responder: Unable to add peer");
            return WIFI_ERROR_UNKNOWN;
        }

        if (data && (len < MAX_FRAME_LEN_80211_MGMT)) {
            entry->frame = (struct pasn_auth_frame *)malloc(sizeof(struct pasn_auth_frame));
            if (entry->frame) {
                memcpy(entry->frame->data, data, len);
                entry->frame->len = len;
            }
        }
        entry->is_pairing_in_progress = true;

        nan_attr_ie = nan_get_attr_from_ies(mgmt->u.auth.variable,
                         len - offsetof(struct ieee80211_mgmt, u.auth.variable),
                         NAN_ATTR_ID_DCEA);
        if (nan_attr_ie) {
           nan_dcea *dcea = (nan_dcea *)nan_attr_ie;
           entry->dcea_cap_info = dcea->cap_info;
        }

        nan_attr_ie = nan_get_attr_from_ies(mgmt->u.auth.variable,
                         len - offsetof(struct ieee80211_mgmt, u.auth.variable),
                         NAN_ATTR_ID_NPBA);
        if (nan_attr_ie) {
           nan_npba *npba = (nan_npba *)nan_attr_ie;
           entry->peer_supported_bootstrap = npba->bootstrapping_method;
        }

        entry->pairing_instance_id = info->secure_nan->pairing_id++;
        entry->peer_role = SECURE_NAN_PAIRING_INITIATOR;

        NanPairingRequestInd pairingReqInd;

        /* pub_sub_id and requestor_instance_id populated during bootstrapping */
        pairingReqInd.publish_subscribe_id = entry->pub_sub_id;
        pairingReqInd.requestor_instance_id = entry->requestor_instance_id;
        pairingReqInd.pairing_instance_id = entry->pairing_instance_id;
        memcpy(pairingReqInd.peer_disc_mac_addr, (u8 *)mgmt->sa, NAN_MAC_ADDR_LEN);
        if (nira_present) {
            pairingReqInd.nan_pairing_request_type = NAN_PAIRING_VERIFICATION;
            /* The NIRA from peer for Nan pairing verification */
            memcpy(pairingReqInd.nira.nonce, nira_nonce, NAN_IDENTITY_NONCE_LEN);
            memcpy(pairingReqInd.nira.tag, nira_tag, NAN_IDENTITY_TAG_LEN);
            entry->is_paired = true;
        } else {
            pairingReqInd.nan_pairing_request_type = NAN_PAIRING_SETUP;
        }

        if (entry->dcea_cap_info & DCEA_NPK_CACHING_ENABLED)
            pairingReqInd.enable_pairing_cache = 1;

        nanCommand->handleNanPairingReqInd(&pairingReqInd);

    /* PASN authentication M3 frame processing */
    } else if (auth_transaction == 3) {
        entry = nan_pairing_get_peer_from_list(info->secure_nan, (u8 *)mgmt->sa);
        if (!entry) {
            ALOGE("PASN Responder: M3 from different peer");
            return WIFI_SUCCESS;
        }
        pasn = &entry->pasn;
        ret = handle_auth_pasn_3(pasn, pasn->own_addr,
                                 (u8 *)mgmt->sa, mgmt, len);
        if (ret != 0) {
            ALOGE("PASN Responder: Handle PASN Auth3 failed ");
            return WIFI_ERROR_UNKNOWN;
        }
    }
    return WIFI_SUCCESS;
}
#else /* WPA_PASN_LIB */
wifi_error nan_pairing_indication_response(transaction_id id,
                                           wifi_interface_handle iface,
                                           NanPairingIndicationResponse* msg)
{
    ALOGE(" nan pairing not supported");
    return WIFI_ERROR_NOT_SUPPORTED;
}
#endif /* WPA_PASN_LIB */
