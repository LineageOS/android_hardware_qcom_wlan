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
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "sync.h"
#include <utils/Log.h>
#include <errno.h>
#include <hardware_legacy/wifi_hal.h>
#include "nan_i.h"
#include "nancommand.h"
#include <inttypes.h>

#define NOISE_FLOOR (-100)

//Function which calls the necessaryIndication callback
//based on the indication type
int NanCommand::handleNanIndication()
{
    //Based on the message_id in the header determine the Indication type
    //and call the necessary callback handler
    u16 msg_id;
    int res = 0;

    msg_id = getIndicationType();

    ALOGV("handleNanIndication msg_id:%u", msg_id);
    switch (msg_id) {
    case NAN_INDICATION_PUBLISH_REPLIED:
        NanPublishRepliedInd publishRepliedInd;
        memset(&publishRepliedInd, 0, sizeof(publishRepliedInd));
        res = getNanPublishReplied(&publishRepliedInd);
        if (!res && mHandler.EventPublishReplied) {
            (*mHandler.EventPublishReplied)(&publishRepliedInd);
        }
        break;

    case NAN_INDICATION_PUBLISH_TERMINATED:
        NanPublishTerminatedInd publishTerminatedInd;
        memset(&publishTerminatedInd, 0, sizeof(publishTerminatedInd));
        res = getNanPublishTerminated(&publishTerminatedInd);
        if (!res && mHandler.EventPublishTerminated) {
            (*mHandler.EventPublishTerminated)(&publishTerminatedInd);
        }
        break;

    case NAN_INDICATION_MATCH:
        NanMatchInd matchInd;
        memset(&matchInd, 0, sizeof(matchInd));
        res = getNanMatch(&matchInd);
        if (!res && mHandler.EventMatch) {
            (*mHandler.EventMatch)(&matchInd);
        }
        break;

    case NAN_INDICATION_MATCH_EXPIRED:
        NanMatchExpiredInd matchExpiredInd;
        memset(&matchExpiredInd, 0, sizeof(matchExpiredInd));
        res = getNanMatchExpired(&matchExpiredInd);
        if (!res && mHandler.EventMatchExpired) {
            (*mHandler.EventMatchExpired)(&matchExpiredInd);
        }
        break;

    case NAN_INDICATION_SUBSCRIBE_TERMINATED:
        NanSubscribeTerminatedInd subscribeTerminatedInd;
        memset(&subscribeTerminatedInd, 0, sizeof(subscribeTerminatedInd));
        res = getNanSubscribeTerminated(&subscribeTerminatedInd);
        if (!res && mHandler.EventSubscribeTerminated) {
            (*mHandler.EventSubscribeTerminated)(&subscribeTerminatedInd);
        }
        break;

    case NAN_INDICATION_DE_EVENT:
        NanDiscEngEventInd discEngEventInd;
        memset(&discEngEventInd, 0, sizeof(discEngEventInd));
        res = getNanDiscEngEvent(&discEngEventInd);
        /* Save the self MAC address received in DE indication event to use it
         * in Passphrase to PMK calculation. And do not call the handler if the
         * framework has disabled the self MAC address indication.
         */
        if (!res &&
            ((discEngEventInd.event_type == NAN_EVENT_ID_STARTED_CLUSTER) ||
            (discEngEventInd.event_type == NAN_EVENT_ID_JOINED_CLUSTER))) {
            mNanCommandInstance->saveClusterAddr(discEngEventInd.data.cluster.addr);
            if (discEngEventInd.event_type == NAN_EVENT_ID_STARTED_CLUSTER &&
                (mNanCommandInstance->mConfigDiscoveryIndications &
                 NAN_STARTED_CLUSTER_IND_DISABLED))
                break;
            if (discEngEventInd.event_type == NAN_EVENT_ID_JOINED_CLUSTER &&
                (mNanCommandInstance->mConfigDiscoveryIndications &
                 NAN_JOINED_CLUSTER_IND_DISABLED))
                break;
        }
        if (!res &&
            (discEngEventInd.event_type == NAN_EVENT_ID_DISC_MAC_ADDR)) {
            mNanCommandInstance->saveNmi(discEngEventInd.data.mac_addr.addr);
            if (mNanCommandInstance->mConfigDiscoveryIndications &
                NAN_DISC_ADDR_IND_DISABLED)
                break;
        }
        if (!res && mHandler.EventDiscEngEvent) {
            (*mHandler.EventDiscEngEvent)(&discEngEventInd);
        }
        break;

    case NAN_INDICATION_FOLLOWUP:
        NanFollowupInd followupInd;

        memset(&followupInd, 0, sizeof(followupInd));

        res = handleNanBootstrappingIndication();
        if (res)
           ALOGE("handleNanBootstrappingInd Failed, ret = %d", res);

        res = handleNanSharedKeyDescIndication();
        if (res)
           ALOGE("handleNanSharedKeyDescInd Failed, ret = %d", res);

        res = getNanFollowup(&followupInd);
        if (!res && mHandler.EventFollowup) {
            (*mHandler.EventFollowup)(&followupInd);
        }
        break;

    case NAN_INDICATION_DISABLED:
        NanDisabledInd disabledInd;
        memset(&disabledInd, 0, sizeof(disabledInd));
        res = getNanDisabled(&disabledInd);
        if (!res && mHandler.EventDisabled) {
            (*mHandler.EventDisabled)(&disabledInd);
        }
        break;

    case NAN_INDICATION_TCA:
        NanTCAInd tcaInd;
        memset(&tcaInd, 0, sizeof(tcaInd));
        res = getNanTca(&tcaInd);
        if (!res && mHandler.EventTca) {
            (*mHandler.EventTca)(&tcaInd);
        }
        break;

    case NAN_INDICATION_BEACON_SDF_PAYLOAD:
        NanBeaconSdfPayloadInd beaconSdfPayloadInd;
        memset(&beaconSdfPayloadInd, 0, sizeof(beaconSdfPayloadInd));
        res = getNanBeaconSdfPayload(&beaconSdfPayloadInd);
        if (!res && mHandler.EventBeaconSdfPayload) {
            (*mHandler.EventBeaconSdfPayload)(&beaconSdfPayloadInd);
        }
        break;

    case NAN_INDICATION_SELF_TRANSMIT_FOLLOWUP:
        NanTransmitFollowupInd transmitFollowupInd;
        memset(&transmitFollowupInd, 0, sizeof(NanTransmitFollowupInd));
        res = getNanTransmitFollowupInd(&transmitFollowupInd);
        if (!res && mHandler.EventTransmitFollowup) {
            (*mHandler.EventTransmitFollowup)(&transmitFollowupInd);
        }
        break;

    case NAN_INDICATION_RANGING_REQUEST_RECEIVED:
        NanRangeRequestInd rangeRequestInd;
        memset(&rangeRequestInd, 0, sizeof(NanRangeRequestInd));
        res = getNanRangeRequestReceivedInd(&rangeRequestInd);
        if (!res && mHandler.EventRangeRequest) {
            (*mHandler.EventRangeRequest)(&rangeRequestInd);
        }
        break;

    case NAN_INDICATION_RANGING_RESULT:
        NanRangeReportInd rangeReportInd;
        memset(&rangeReportInd, 0, sizeof(NanRangeReportInd));
        res = getNanRangeReportInd(&rangeReportInd);
        if (!res && mHandler.EventRangeReport) {
            (*mHandler.EventRangeReport)(&rangeReportInd);
        }
        break;

    case NAN_CONTINUOUS_RANGE_RESULT_IND:
        res = getNanContinuousRangingResult();
        break;
#ifdef CONFIG_NAN_VENDOR_AIDL
    case NAN_INDICATION_VENDOR_EVENT:
        NanVendorEventInd EventInd;
        memset(&EventInd, 0, sizeof(EventInd));
        res = getNanVendorEventInd(&EventInd);
        if (!res && mVendorHandler.VendorEventIndication) {
            (*mVendorHandler.VendorEventIndication)(&EventInd);
        }
        break;
#endif
    default:
        ALOGE("handleNanIndication error invalid msg_id:%u", msg_id);
        res = (int)WIFI_ERROR_INVALID_REQUEST_ID;
        break;
    }
    return res;
}

//Function which will return the Nan Indication type based on
//the initial few bytes of mNanVendorEvent
NanIndicationType NanCommand::getIndicationType()
{
    if (mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid argument mNanVendorEvent:%p",
              __func__, mNanVendorEvent);
        return NAN_INDICATION_UNKNOWN;
    }

    NanMsgHeader *pHeader = (NanMsgHeader *)mNanVendorEvent;

    switch (pHeader->msgId) {
    case NAN_MSG_ID_PUBLISH_REPLIED_IND:
        return NAN_INDICATION_PUBLISH_REPLIED;
    case NAN_MSG_ID_PUBLISH_TERMINATED_IND:
        return NAN_INDICATION_PUBLISH_TERMINATED;
    case NAN_MSG_ID_MATCH_IND:
        return NAN_INDICATION_MATCH;
    case NAN_MSG_ID_MATCH_EXPIRED_IND:
        return NAN_INDICATION_MATCH_EXPIRED;
    case NAN_MSG_ID_FOLLOWUP_IND:
        return NAN_INDICATION_FOLLOWUP;
    case NAN_MSG_ID_SUBSCRIBE_TERMINATED_IND:
        return NAN_INDICATION_SUBSCRIBE_TERMINATED;
    case  NAN_MSG_ID_DE_EVENT_IND:
        return NAN_INDICATION_DE_EVENT;
    case NAN_MSG_ID_DISABLE_IND:
        return NAN_INDICATION_DISABLED;
    case NAN_MSG_ID_TCA_IND:
        return NAN_INDICATION_TCA;
    case NAN_MSG_ID_BEACON_SDF_IND:
        return NAN_INDICATION_BEACON_SDF_PAYLOAD;
    case NAN_MSG_ID_SELF_TRANSMIT_FOLLOWUP_IND:
        return NAN_INDICATION_SELF_TRANSMIT_FOLLOWUP;
    case NAN_MSG_ID_RANGING_REQUEST_RECEVD_IND:
        return NAN_INDICATION_RANGING_REQUEST_RECEIVED;
    case NAN_MSG_ID_RANGING_RESULT_IND:
        return NAN_INDICATION_RANGING_RESULT;
    case NAN_MSG_ID_IDENTITY_RESOLUTION_IND:
        return NAN_INDICATION_IDENTITY_RESOLUTION;
    case NAN_MSG_ID_OEM_IND:
        return NAN_INDICATION_VENDOR_EVENT;
    case NAN_MSG_ID_CONTINUOUS_RANGE_RESULT_IND:
        return NAN_CONTINUOUS_RANGE_RESULT_IND;
    default:
        return NAN_INDICATION_UNKNOWN;
    }
}

int NanCommand::getNanPublishReplied(NanPublishRepliedInd *event)
{
    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanPublishRepliedIndMsg pRsp = (pNanPublishRepliedIndMsg)mNanVendorEvent;
    event->requestor_instance_id = pRsp->publishRepliedIndParams.matchHandle;

    event->rssi_value = 0;
    u8 *pInputTlv = pRsp->ptlv;
    NanTlv outputTlv;
    u16 readLen = 0;
    int remainingLen = (mNanDataLen - \
        (sizeof(NanMsgHeader) + sizeof(NanPublishRepliedIndParams)));

    if (remainingLen <= 0) {
        ALOGI("%s: No TLV's present",__func__);
        return WIFI_SUCCESS;
    }
    while ((remainingLen > 0) &&
           (0 != (readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen)))) {
        switch (outputTlv.type) {
        case NAN_TLV_TYPE_MAC_ADDRESS:
            if (outputTlv.length > sizeof(event->addr)) {
                outputTlv.length = sizeof(event->addr);
            }
            memcpy(event->addr, outputTlv.value, outputTlv.length);
            break;
        case NAN_TLV_TYPE_RECEIVED_RSSI_VALUE:
            if (outputTlv.length > sizeof(event->rssi_value)) {
                outputTlv.length = sizeof(event->rssi_value);
            }
            memcpy(&event->rssi_value, outputTlv.value,
                   outputTlv.length);
            break;
        default:
            ALOGI("Unknown TLV type skipped");
            break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
        memset(&outputTlv, 0, sizeof(outputTlv));
    }
    return WIFI_SUCCESS;
}

int NanCommand::getNanPublishTerminated(NanPublishTerminatedInd *event)
{
    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanPublishTerminatedIndMsg pRsp = (pNanPublishTerminatedIndMsg)mNanVendorEvent;
    event->publish_id = pRsp->fwHeader.handle;
    NanErrorTranslation((NanInternalStatusType)pRsp->reason, 0,
                        (void*)event, false);
    return WIFI_SUCCESS;
}

int NanCommand::getNanMatch(NanMatchInd *event)
{
    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanMatchIndMsg pRsp = (pNanMatchIndMsg)mNanVendorEvent;
    event->publish_subscribe_id = pRsp->fwHeader.handle;
    event->requestor_instance_id = pRsp->matchIndParams.matchHandle;
    event->match_occured_flag = pRsp->matchIndParams.matchOccuredFlag;
    event->out_of_resource_flag = pRsp->matchIndParams.outOfResourceFlag;

    u8 *pInputTlv = pRsp->ptlv;
    NanTlv outputTlv;
    u16 readLen = 0;
    int remainingLen = (mNanDataLen - \
        (sizeof(NanMsgHeader) + sizeof(NanMatchIndParams)));
    int ret = 0, idx = 0;

    //Has SDF match filter and service specific info TLV
    if (remainingLen <= 0) {
        ALOGV("%s: No TLV's present",__func__);
        return WIFI_SUCCESS;
    }
    ALOGV("%s: TLV remaining Len:%d",__func__, remainingLen);
    while ((remainingLen > 0) &&
           (0 != (readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen)))) {
        ALOGV("%s: Remaining Len:%d readLen:%d type:%d length:%d",
              __func__, remainingLen, readLen, outputTlv.type,
              outputTlv.length);
        switch (outputTlv.type) {
        case NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO:
            if (outputTlv.length > NAN_MAX_SERVICE_NAME_LEN) {
                outputTlv.length = NAN_MAX_SERVICE_NAME_LEN;
            }
            event->service_specific_info_len = outputTlv.length;
            memcpy(event->service_specific_info, outputTlv.value,
                   outputTlv.length);
            break;
        case NAN_TLV_TYPE_SDF_MATCH_FILTER:
            if (outputTlv.length > NAN_MAX_MATCH_FILTER_LEN) {
                outputTlv.length = NAN_MAX_MATCH_FILTER_LEN;
            }
            event->sdf_match_filter_len = outputTlv.length;
            memcpy(event->sdf_match_filter, outputTlv.value,
                   outputTlv.length);
            break;
        case NAN_TLV_TYPE_MAC_ADDRESS:
            if (outputTlv.length > sizeof(event->addr)) {
                outputTlv.length = sizeof(event->addr);
            }
            memcpy(event->addr, outputTlv.value, outputTlv.length);
            break;
        case NAN_TLV_TYPE_RECEIVED_RSSI_VALUE:
            if (outputTlv.length > sizeof(event->rssi_value)) {
                outputTlv.length = sizeof(event->rssi_value);
            }
            memcpy(&event->rssi_value, outputTlv.value,
                   outputTlv.length);
            break;
        case NAN_TLV_TYPE_POST_NAN_CONNECTIVITY_CAPABILITIES_RECEIVE:
            if (outputTlv.length != sizeof(u32)) {
                ALOGE("NAN_TLV_TYPE_POST_NAN_CONNECTIVITY_CAPABILITIES_RECEIVE"
                      "Incorrect size:%d expecting %zu", outputTlv.length,
                      sizeof(u32));
                break;
            }
            event->is_conn_capability_valid = 1;
            /* Populate conn_capability from received TLV */
            getNanReceivePostConnectivityCapabilityVal(outputTlv.value,
                                                       &event->conn_capability);
            break;
        case NAN_TLV_TYPE_POST_NAN_DISCOVERY_ATTRIBUTE_RECEIVE:
            /* Populate receive discovery attribute from
               received TLV */
            idx = event->num_rx_discovery_attr;
            if (idx < 0 || idx >= NAN_MAX_POSTDISCOVERY_LEN) {
                ALOGE("NAN_TLV_TYPE_POST_NAN_DISCOVERY_ATTRIBUTE_RECEIVE"
                      " Incorrect index:%d >= %d", idx, NAN_MAX_POSTDISCOVERY_LEN);
                break;
            }
            ret = getNanReceivePostDiscoveryVal(outputTlv.value,
                                                outputTlv.length,
                                                &event->discovery_attr[idx]);
            if (ret == 0) {
                event->num_rx_discovery_attr++;
            } else {
                ALOGE("NAN_TLV_TYPE_POST_NAN_DISCOVERY_ATTRIBUTE_RECEIVE"
                      "Incorrect");
            }
            break;
        case NAN_TLV_TYPE_FURTHER_AVAILABILITY_MAP:
            /* Populate further availability bitmap from
               received TLV */
            ret = getNanFurtherAvailabilityMap(outputTlv.value,
                                               outputTlv.length,
                                               &event->num_chans,
                                               &event->famchan[0]);
            if (ret < 0)
                ALOGE("NAN_TLV_TYPE_FURTHER_AVAILABILITY_MAP"
                      "Incorrect");
            break;
        case NAN_TLV_TYPE_CLUSTER_ATTRIBUTE:
            if (outputTlv.length > sizeof(event->cluster_attribute)) {
                outputTlv.length = sizeof(event->cluster_attribute);
            }
            memcpy(event->cluster_attribute,
                   outputTlv.value, outputTlv.length);
            event->cluster_attribute_len = outputTlv.length;
            break;
        case NAN_TLV_TYPE_NAN_CSID:
        case NAN_TLV_TYPE_NAN_CSID_EXT:
            if (outputTlv.length > sizeof(event->peer_cipher_type)) {
                outputTlv.length = sizeof(event->peer_cipher_type);
            }
            memcpy(&event->peer_cipher_type, outputTlv.value,
                   outputTlv.length);
            break;
        case NAN_TLV_TYPE_NAN_SCID:
            if (outputTlv.length > sizeof(event->scid)) {
                outputTlv.length = sizeof(event->scid);
            }
            event->scid_len = outputTlv.length;
            memcpy(event->scid, outputTlv.value, outputTlv.length);
            break;
        case NAN_TLV_TYPE_SDEA_CTRL_PARAMS:
            if (outputTlv.length != sizeof(u32)) {
                ALOGE("NAN_TLV_TYPE_SDEA_CTRL_PARAMS"
                      "Incorrect size:%d expecting %zu", outputTlv.length,
                      sizeof(u32));
                break;
            }
            getNanReceiveSdeaCtrlParams(outputTlv.value,
                                             &event->peer_sdea_params);
            break;
        case NAN_TLV_TYPE_NAN20_RANGING_RESULT:
            if (outputTlv.length > sizeof(event->range_info)) {
                outputTlv.length = sizeof(event->range_info);
            }
            memcpy(&event->range_info, outputTlv.value, outputTlv.length);
            break;
        case NAN_TLV_TYPE_PAIRING_MATCH_PARAMS:
            if (outputTlv.length != sizeof(NanFWPairingParamsMatch)) {
                ALOGE("NAN_TLV_TYPE_PAIRING_MATCH_PARAMS"
                      "Incorrect size:%d expecting %zu", outputTlv.length,
                      sizeof(NanFWPairingParamsMatch));
                break;
            }
            getNanReceivePairingParamsMatch(outputTlv.value,
                                             &event->peer_pairing_config);
            break;
        case NAN_TLV_TYPE_NIRA_NONCE:
            if (outputTlv.length > sizeof(event->nira.nonce))
                outputTlv.length = sizeof(event->nira.nonce);
            memcpy(event->nira.nonce, outputTlv.value, outputTlv.length);
            event->peer_pairing_config.enable_pairing_verification = 1;
            break;
        case NAN_TLV_TYPE_NIRA_TAG:
            if (outputTlv.length > sizeof(event->nira.tag))
                outputTlv.length = sizeof(event->nira.tag);
            memcpy(event->nira.tag, outputTlv.value, outputTlv.length);
            event->peer_pairing_config.enable_pairing_verification = 1;
            break;
        case NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO:
            if (outputTlv.length > NAN_MAX_SDEA_SERVICE_SPECIFIC_INFO_LEN) {
                outputTlv.length = NAN_MAX_SDEA_SERVICE_SPECIFIC_INFO_LEN;
            }
            event->sdea_service_specific_info_len = outputTlv.length;
            memcpy(event->sdea_service_specific_info, outputTlv.value,
                   outputTlv.length);
            break;
        case NAN_TLV_TYPE_SERVICE_ID:
            mNanCommandInstance->saveServiceId(outputTlv.value,
                                               event->publish_subscribe_id,
                                               event->requestor_instance_id,
                                               NAN_ROLE_SUBSCRIBER,
                                               event->addr);
            break;
        default:
            ALOGV("Unknown TLV type skipped");
            break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
        memset(&outputTlv, 0, sizeof(outputTlv));
    }
    return WIFI_SUCCESS;
}

int NanCommand::getNanMatchExpired(NanMatchExpiredInd *event)
{
    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanMatchExpiredIndMsg pRsp = (pNanMatchExpiredIndMsg)mNanVendorEvent;
    event->publish_subscribe_id = pRsp->fwHeader.handle;
    event->requestor_instance_id = pRsp->matchExpiredIndParams.matchHandle;
    return WIFI_SUCCESS;
}

int NanCommand::getNanSubscribeTerminated(NanSubscribeTerminatedInd *event)
{
    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanSubscribeTerminatedIndMsg pRsp = (pNanSubscribeTerminatedIndMsg)mNanVendorEvent;
    event->subscribe_id = pRsp->fwHeader.handle;
    NanErrorTranslation((NanInternalStatusType)pRsp->reason, 0,
                        (void*)event, false);
    return WIFI_SUCCESS;
}

int NanCommand::handleNanBootstrappingIndication()
{
    u8 mac[NAN_MAC_ADDR_LEN];
    int retval = WIFI_SUCCESS;
    NanFWBootstrappingParams *params = NULL;

    if (mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid mNanVendorEvent:%p",
              __func__, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanFollowupIndMsg pRsp = (pNanFollowupIndMsg)mNanVendorEvent;
    u8 *pInputTlv = pRsp->ptlv;
    NanTlv outputTlv;
    u16 readLen = 0;
    u32 cookie_length = 0;
    u8 *cookie = NULL;
    int remainingLen = (mNanDataLen -  \
        (sizeof(NanMsgHeader) + sizeof(NanFollowupIndParams)));

    if (remainingLen <= 0) {
        ALOGV("%s: No TLV's present",__func__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    ALOGV("%s: TLV remaining Len:%d",__func__, remainingLen);
    memset(mac, 0, sizeof(mac));
    while (remainingLen > 0) {
        memset(&outputTlv, 0, sizeof(outputTlv));
        readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen);
        if (!readLen)
            break;

        ALOGV("%s: Remaining Len:%d readLen:%d type:%d length:%d",
              __func__, remainingLen, readLen, outputTlv.type,
              outputTlv.length);
        switch (outputTlv.type) {
        case NAN_TLV_TYPE_MAC_ADDRESS:
            if (outputTlv.length > sizeof(mac)) {
                outputTlv.length = sizeof(mac);
            }
            memcpy(mac, outputTlv.value, outputTlv.length);
            break;
        case NAN_TLV_TYPE_BOOTSTRAPPING_PARAMS:
            if (outputTlv.length != sizeof(NanFWBootstrappingParams)) {
                ALOGE("NAN Bootstrapping Params"
                      "Incorrect size:%d expecting %zu", outputTlv.length,
                      sizeof(NanFWBootstrappingParams));
                break;
            }
            params = (NanFWBootstrappingParams *)outputTlv.value;
            break;
        case NAN_TLV_TYPE_BOOTSTRAPPING_COOKIE:
            ALOGV("Bootstrapping cookie received. len =%d", outputTlv.length);
            cookie_length = outputTlv.length;
            cookie = outputTlv.value;
            break;
        default:
            ALOGV("Unknown TLV type skipped");
            break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
    }

    if (params)
    {
       hal_info *info = getHalInfo(wifiHandle());
       struct nan_pairing_peer_info *entry = NULL;
       if (!info) {
           ALOGE("%s: hal info NULL", __FUNCTION__);
           return WIFI_ERROR_UNKNOWN;
       }
       if (!info->secure_nan) {
           ALOGE("%s: Secure NAN not supported", __FUNCTION__);
           return WIFI_ERROR_UNKNOWN;
       }
       if (params->type == NAN_BS_TYPE_REQUEST) {
           NanBootstrappingRequestInd bootstrapReqInd;

           entry = nan_pairing_get_peer_from_list(info->secure_nan, mac);
           if (entry && entry->is_pairing_in_progress) {
               ALOGV("%s: pairing in progress", __FUNCTION__);
               return WIFI_ERROR_UNKNOWN;
           }

           memset(&bootstrapReqInd, 0, sizeof(bootstrapReqInd));
           bootstrapReqInd.publish_subscribe_id = pRsp->fwHeader.handle;
           info->secure_nan->bootstrapping_id++;
           bootstrapReqInd.bootstrapping_instance_id =
                                         info->secure_nan->bootstrapping_id;
           bootstrapReqInd.requestor_instance_id =
                                         pRsp->followupIndParams.matchHandle;
           memcpy(bootstrapReqInd.peer_disc_mac_addr, mac, NAN_MAC_ADDR_LEN);
           bootstrapReqInd.request_bootstrapping_method =
             get_matching_bootstrap_method(params->bootstrapping_method_bitmap);
           handleNanBootstrappingReqInd(&bootstrapReqInd);
           entry = nan_pairing_add_peer_to_list(info->secure_nan, mac);
           if (entry) {
               entry->requestor_instance_id = bootstrapReqInd.requestor_instance_id;
               entry->pub_sub_id = bootstrapReqInd.publish_subscribe_id;
               entry->bootstrapping_instance_id =
                                      bootstrapReqInd.bootstrapping_instance_id;
               entry->peer_role = SECURE_NAN_BOOTSTRAPPING_INITIATOR;
               entry->peer_supported_bootstrap =
                                      params->bootstrapping_method_bitmap;
               entry->dialog_token = params->dialog_token;
           }
       } else if (params->type == NAN_BS_TYPE_RESPONSE) {
           entry = nan_pairing_get_peer_from_list(info->secure_nan, mac);
           if (entry == NULL) {
               ALOGE("%s: peer not found: ADDR=" MACSTR, __FUNCTION__, MAC2STR(mac));
               return WIFI_ERROR_UNKNOWN;
           }

           if (entry->dialog_token != params->dialog_token) {
               ALOGE("Dialog token not matching. Req token: %d, Rsp token: %d",
                      entry->dialog_token, params->dialog_token);
#ifdef CONFIG_NAN_STRICT_MODE
               return WIFI_ERROR_UNKNOWN;
#endif /* CONFIG_NAN_STRICT_MODE */
           }

           NanBootstrappingConfirmInd *bootstrapConfirmInd =
             (NanBootstrappingConfirmInd *)malloc(sizeof(NanBootstrappingConfirmInd)
                                                  + cookie_length);
           if (!bootstrapConfirmInd) {
               ALOGE("%s: Memory allocation Failed", __FUNCTION__);
               return WIFI_ERROR_OUT_OF_MEMORY;
           }
           memset(bootstrapConfirmInd, 0, sizeof(NanBootstrappingConfirmInd) +
                                          cookie_length);
           bootstrapConfirmInd->bootstrapping_instance_id =
                                  entry->bootstrapping_instance_id;
           if(params->status == NAN_BS_STATUS_ACCEPT)
              bootstrapConfirmInd->rsp_code = NAN_BOOTSTRAPPING_REQUEST_ACCEPT;
           else if (params->status == NAN_BS_STATUS_REJECT)
              bootstrapConfirmInd->rsp_code = NAN_BOOTSTRAPPING_REQUEST_REJECT;
           else if (params->status == NAN_BS_STATUS_COMEBACK)
              bootstrapConfirmInd->rsp_code = NAN_BOOTSTRAPPING_REQUEST_COMEBACK;

           bootstrapConfirmInd->reason_code =
                                          (NanStatusType)params->reason_code;
           bootstrapConfirmInd->come_back_delay =
                                          params->comeback_after;
           bootstrapConfirmInd->cookie_length = cookie_length;
           if (cookie_length)
               memcpy(bootstrapConfirmInd->cookie, cookie, cookie_length);

           handleNanBootstrappingConfirm(bootstrapConfirmInd);
       } else {
            ALOGV("Bootstrapping msg type Invalid, type=%d", params->type);
            retval = WIFI_ERROR_INVALID_ARGS;
       }
    }
    return retval;
}

int NanCommand::handleNanSharedKeyDescIndication()
{
    u8 mac[NAN_MAC_ADDR_LEN];
    int retval = WIFI_SUCCESS;
    u16 shared_key_attr_len = 0;
    u8 shared_key_attr[NAN_MAX_SHARED_KEY_ATTR_LEN];
    wifi_interface_handle ifaceHandle;

    if (mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid mNanVendorEvent:%p",
              __func__, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanFollowupIndMsg pRsp = (pNanFollowupIndMsg)mNanVendorEvent;
    u8 *pInputTlv = pRsp->ptlv;
    NanTlv outputTlv;
    u16 readLen = 0;
    int remainingLen = (mNanDataLen -  \
        (sizeof(NanMsgHeader) + sizeof(NanFollowupIndParams)));

    if (remainingLen <= 0) {
        ALOGV("%s: No TLV's present",__func__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    ALOGV("%s: TLV remaining Len:%d",__func__, remainingLen);
    memset(mac, 0, sizeof(mac));
    while (remainingLen > 0) {
        memset(&outputTlv, 0, sizeof(outputTlv));
        readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen);
        if (!readLen)
            break;

        ALOGV("%s: Remaining Len:%d readLen:%d type:%d length:%d",
              __func__, remainingLen, readLen, outputTlv.type,
              outputTlv.length);
        switch (outputTlv.type) {
        case NAN_TLV_TYPE_MAC_ADDRESS:
            if (outputTlv.length > sizeof(mac)) {
                outputTlv.length = sizeof(mac);
            }
            memcpy(mac, outputTlv.value, outputTlv.length);
            break;
        case NAN_TLV_TYPE_NAN_SHARED_KEY_DESC_ATTR:
            if (outputTlv.length > NAN_MAX_SHARED_KEY_ATTR_LEN) {
                outputTlv.length = NAN_MAX_SHARED_KEY_ATTR_LEN;
            }
            shared_key_attr_len = outputTlv.length;
            memcpy(shared_key_attr, outputTlv.value,
                   outputTlv.length);
            break;
        default:
            ALOGV("Unknown TLV type skipped");
            break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
    }
#ifdef WPA_PASN_LIB
    if (!shared_key_attr_len)
        return retval;

    NanPairingConfirmInd evt;
    struct pasn_data *pasn;
    struct nan_pairing_peer_info *entry;
    hal_info *info = getHalInfo(wifiHandle());
    if (!info) {
        ALOGE("%s: hal info NULL", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    if (!info->secure_nan) {
        ALOGE("%s: Secure NAN not supported", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }
    ifaceHandle = wifi_get_iface_handle(wifiHandle(),
                                        info->secure_nan->iface_name);
    if (!ifaceHandle) {
        ALOGE("%s: ifaceHandle NULL for %s", __FUNCTION__,
              info->secure_nan->iface_name);
        return WIFI_ERROR_INVALID_ARGS;
    }
    if (nan_validate_shared_key_desc(ifaceHandle, mac, shared_key_attr,
                                     shared_key_attr_len)) {
         ALOGV("Pairing handshake Invalid");
         return WIFI_ERROR_INVALID_ARGS;
    }

    entry = nan_pairing_get_peer_from_list(info->secure_nan, mac);
    if (!entry) {
        ALOGE("%s NAN Pairing: peer not found", __FUNCTION__);
        return retval;
    }

    if (entry->peer_role == SECURE_NAN_PAIRING_INITIATOR) {
        info->secure_nan->pending_peer = entry;
        nan_pairing_prepare_skda_data(ifaceHandle);
    }

    pasn = entry->pasn;
    evt.pairing_instance_id = entry->pairing_instance_id;
    evt.rsp_code = NAN_PAIRING_REQUEST_ACCEPT;
    evt.reason_code = NAN_STATUS_SUCCESS;
    if (entry->is_paired)
        evt.nan_pairing_request_type = NAN_PAIRING_VERIFICATION;
    else
        evt.nan_pairing_request_type = NAN_PAIRING_SETUP;

    evt.enable_pairing_cache = !!(entry->dcea_cap_info & DCEA_NPK_CACHING_ENABLED);

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
               info->secure_nan->dev_nik->nik_data, NAN_IDENTITY_KEY_LEN);

    memcpy(evt.npk_security_association.peer_nan_identity_key,
           entry->peer_nik, NAN_IDENTITY_KEY_LEN);

    nan_pairing_remove_peers_with_nik(info, entry->peer_nik, entry->bssid);

    if (pasn_get_pmk_len(pasn) <= sizeof(evt.npk_security_association.npk.pmk)) {
        memcpy(evt.npk_security_association.npk.pmk, pasn_get_pmk(pasn),
               pasn_get_pmk_len(pasn));
        evt.npk_security_association.npk.pmk_len = pasn_get_pmk_len(pasn);
    } else {
        ALOGE("%s: Invalid pmk len: %zu", __FUNCTION__, pasn_get_pmk_len(pasn));
    }
    wpa_pasn_reset(pasn);
    handleNanPairingConfirm(&evt);
    entry->is_paired = true;
    entry->is_pairing_in_progress = false;
#endif
    return retval;
}

int NanCommand::getNanFollowup(NanFollowupInd *event)
{
    struct nan_pairing_peer_info *entry;
    hal_info *info = getHalInfo(wifiHandle());

    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanFollowupIndMsg pRsp = (pNanFollowupIndMsg)mNanVendorEvent;
    event->publish_subscribe_id = pRsp->fwHeader.handle;
    event->requestor_instance_id = pRsp->followupIndParams.matchHandle;
    event->dw_or_faw = pRsp->followupIndParams.window;

    u8 *pInputTlv = pRsp->ptlv;
    NanTlv outputTlv;
    u16 readLen = 0;
    int remainingLen = (mNanDataLen -  \
        (sizeof(NanMsgHeader) + sizeof(NanFollowupIndParams)));

    //Has service specific info and extended service specific info TLV
    if (remainingLen <= 0) {
        ALOGV("%s: No TLV's present",__func__);
        return WIFI_SUCCESS;
    }
    ALOGV("%s: TLV remaining Len:%d",__func__, remainingLen);
    while ((remainingLen > 0) &&
           (0 != (readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen)))) {
        ALOGV("%s: Remaining Len:%d readLen:%d type:%d length:%d",
              __func__, remainingLen, readLen, outputTlv.type,
              outputTlv.length);
        switch (outputTlv.type) {
        case NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO:
        case NAN_TLV_TYPE_EXT_SERVICE_SPECIFIC_INFO:
            if (outputTlv.length > NAN_MAX_SERVICE_SPECIFIC_INFO_LEN) {
                outputTlv.length = NAN_MAX_SERVICE_SPECIFIC_INFO_LEN;
            }
            event->service_specific_info_len = outputTlv.length;
            memcpy(event->service_specific_info, outputTlv.value,
                   outputTlv.length);
            break;
        case NAN_TLV_TYPE_MAC_ADDRESS:
            if (outputTlv.length > sizeof(event->addr)) {
                outputTlv.length = sizeof(event->addr);
            }
            memcpy(event->addr, outputTlv.value, outputTlv.length);
            break;
        case NAN_TLV_TYPE_SDEA_SERVICE_SPECIFIC_INFO:
            if (outputTlv.length > NAN_MAX_SDEA_SERVICE_SPECIFIC_INFO_LEN) {
                outputTlv.length = NAN_MAX_SDEA_SERVICE_SPECIFIC_INFO_LEN;
            }
            event->sdea_service_specific_info_len = outputTlv.length;
            memcpy(event->sdea_service_specific_info, outputTlv.value,
                   outputTlv.length);
            break;
        default:
            ALOGV("Unknown TLV type skipped");
            break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
        memset(&outputTlv, 0, sizeof(outputTlv));
    }

    if (info && info->secure_nan) {
        entry = nan_pairing_get_peer_from_list(info->secure_nan, event->addr);
        if (entry) {
            if (event->publish_subscribe_id &&
                entry->pub_sub_id != event->publish_subscribe_id) {
                ALOGI("Update previous pub sub id: %d with new id: %d",
                      entry->pub_sub_id, event->publish_subscribe_id);
                entry->pub_sub_id = event->publish_subscribe_id;
            }
            if (event->requestor_instance_id &&
                entry->requestor_instance_id != event->requestor_instance_id) {
                ALOGI("Update previous requestor instance id: %d with new id: %d",
                      entry->requestor_instance_id, event->requestor_instance_id);
                entry->requestor_instance_id = event->requestor_instance_id;
            }
        } else {
            entry = nan_pairing_add_peer_to_list(info->secure_nan, event->addr);
            if (entry) {
                entry->pub_sub_id = event->publish_subscribe_id;
                entry->requestor_instance_id = event->requestor_instance_id;
            }
        }
    }

    return WIFI_SUCCESS;
}

int NanCommand::getNanDiscEngEvent(NanDiscEngEventInd *event)
{
    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanEventIndMsg pRsp = (pNanEventIndMsg)mNanVendorEvent;
    memset(&event->data, 0, sizeof(event->data));

    u8 *pInputTlv = pRsp->ptlv;
    NanTlv outputTlv;
    u16 readLen = 0;
    int remainingLen = (mNanDataLen -  \
        (sizeof(NanMsgHeader)));

    //Has Self-STA Mac TLV
    if (remainingLen <= 0) {
        ALOGE("%s: No TLV's present",__func__);
        return WIFI_SUCCESS;
    }

    ALOGV("%s: TLV remaining Len:%d",__func__, remainingLen);
    while ((remainingLen > 0) &&
           (0 != (readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen)))) {
        ALOGV("%s: Remaining Len:%d readLen:%d type:%d length:%d",
              __func__, remainingLen, readLen, outputTlv.type,
              outputTlv.length);
        switch (outputTlv.type) {
        case NAN_TLV_TYPE_EVENT_SELF_STATION_MAC_ADDRESS:
            if (outputTlv.length > NAN_MAC_ADDR_LEN) {
                ALOGV("%s: Reading only first %d bytes of TLV",
                      __func__, NAN_MAC_ADDR_LEN);
                outputTlv.length = NAN_MAC_ADDR_LEN;
            }
            memcpy(event->data.mac_addr.addr, outputTlv.value,
                   outputTlv.length);
            event->event_type = NAN_EVENT_ID_DISC_MAC_ADDR;
            break;
        case NAN_TLV_TYPE_EVENT_STARTED_CLUSTER:
            if (outputTlv.length > NAN_MAC_ADDR_LEN) {
                ALOGV("%s: Reading only first %d bytes of TLV",
                      __func__, NAN_MAC_ADDR_LEN);
                outputTlv.length = NAN_MAC_ADDR_LEN;
            }
            memcpy(event->data.cluster.addr, outputTlv.value,
                   outputTlv.length);
            event->event_type = NAN_EVENT_ID_STARTED_CLUSTER;
            break;
        case NAN_TLV_TYPE_EVENT_JOINED_CLUSTER:
            if (outputTlv.length > NAN_MAC_ADDR_LEN) {
                ALOGV("%s: Reading only first %d bytes of TLV",
                      __func__, NAN_MAC_ADDR_LEN);
                outputTlv.length = NAN_MAC_ADDR_LEN;
            }
            memcpy(event->data.cluster.addr, outputTlv.value,
                   outputTlv.length);
            event->event_type = NAN_EVENT_ID_JOINED_CLUSTER;
            break;
        default:
            ALOGV("Unhandled TLV type:%d", outputTlv.type);
            break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
        memset(&outputTlv,0, sizeof(outputTlv));
    }
    return WIFI_SUCCESS;
}

int NanCommand::getNanDisabled(NanDisabledInd *event)
{
    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanDisableIndMsg pRsp = (pNanDisableIndMsg)mNanVendorEvent;
    NanErrorTranslation((NanInternalStatusType)pRsp->reason, 0,
                        (void*)event, false);
    return WIFI_SUCCESS;

}

int NanCommand::getNanTca(NanTCAInd *event)
{
    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanTcaIndMsg pRsp = (pNanTcaIndMsg)mNanVendorEvent;
    memset(&event->data, 0, sizeof(event->data));

    u8 *pInputTlv = pRsp->ptlv;
    NanTlv outputTlv;
    u16 readLen = 0;

    int remainingLen = (mNanDataLen -  \
        (sizeof(NanMsgHeader)));

    //Has NAN_TCA_ID_CLUSTER_SIZE
    if (remainingLen <= 0) {
        ALOGE("%s: No TLV's present",__func__);
        return WIFI_SUCCESS;
    }

    ALOGV("%s: TLV remaining Len:%d",__func__, remainingLen);
    while ((remainingLen > 0) &&
           (0 != (readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen)))) {
        ALOGV("%s: Remaining Len:%d readLen:%d type:%d length:%d",
              __func__, remainingLen, readLen, outputTlv.type,
              outputTlv.length);
        switch (outputTlv.type) {
        case NAN_TLV_TYPE_CLUSTER_SIZE_RSP:
            if (outputTlv.length != 2 * sizeof(u32)) {
                ALOGE("%s: Wrong length %d in Tca Indication expecting %zu bytes",
                      __func__, outputTlv.length, 2 * sizeof(u32));
                break;
            }
            event->rising_direction_evt_flag = outputTlv.value[0] & 0x01;
            event->falling_direction_evt_flag = (outputTlv.value[0] & 0x02) >> 1;
            memcpy(&(event->data.cluster.cluster_size), &outputTlv.value[4],
                   sizeof(event->data.cluster.cluster_size));
            event->tca_type = NAN_TCA_ID_CLUSTER_SIZE;
            break;
        default:
            ALOGV("Unhandled TLV type:%d", outputTlv.type);
            break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
        memset(&outputTlv,0, sizeof(outputTlv));
    }
    return WIFI_SUCCESS;
}

int NanCommand::getNanBeaconSdfPayload(NanBeaconSdfPayloadInd *event)
{
    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanBeaconSdfPayloadIndMsg pRsp = (pNanBeaconSdfPayloadIndMsg)mNanVendorEvent;
    memset(&event->data, 0, sizeof(event->data));

    u8 *pInputTlv = pRsp->ptlv;
    NanTlv outputTlv;
    u16 readLen = 0;
    int remainingLen = (mNanDataLen -  \
        (sizeof(NanMsgHeader)));

    //Has Mac address
    if (remainingLen <= 0) {
        ALOGV("%s: No TLV's present",__func__);
        return WIFI_SUCCESS;
    }

    ALOGV("%s: TLV remaining Len:%d",__func__, remainingLen);
    while ((remainingLen > 0) &&
           (0 != (readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen)))) {
        ALOGV("%s: Remaining Len:%d readLen:%d type:%d length:%d",
              __func__, remainingLen, readLen, outputTlv.type,
              outputTlv.length);
        switch (outputTlv.type) {
        case NAN_TLV_TYPE_MAC_ADDRESS:
            if (outputTlv.length > sizeof(event->addr)) {
                outputTlv.length = sizeof(event->addr);
            }
            memcpy(event->addr, outputTlv.value,
                   outputTlv.length);
            break;

        case NAN_TLV_TYPE_VENDOR_SPECIFIC_ATTRIBUTE_RECEIVE:
        {
            NanReceiveVendorSpecificAttribute* recvVsaattr = &event->vsa;
            if (outputTlv.length < sizeof(u32)) {
                ALOGE("NAN_TLV_TYPE_VENDOR_SPECIFIC_ATTRIBUTE_RECEIVE"
                      "Incorrect length:%d", outputTlv.length);
                break;
            }
            event->is_vsa_received = 1;
            recvVsaattr->vsa_received_on = (outputTlv.value[0] >> 1) & 0x07;
            memcpy(&recvVsaattr->vendor_oui, &outputTlv.value[1],
                   3);
            recvVsaattr->attr_len = outputTlv.length - 4;
            if (recvVsaattr->attr_len > NAN_MAX_VSA_DATA_LEN) {
                recvVsaattr->attr_len = NAN_MAX_VSA_DATA_LEN;
            }
            if (recvVsaattr->attr_len) {
                memcpy(recvVsaattr->vsa, &outputTlv.value[4],
                       recvVsaattr->attr_len);
            }
            break;
        }

        case NAN_TLV_TYPE_BEACON_SDF_PAYLOAD_RECEIVE:
            event->is_beacon_sdf_payload_received = 1;
            event->data.frame_len = outputTlv.length;
            if (event->data.frame_len > NAN_MAX_FRAME_DATA_LEN) {
                event->data.frame_len = NAN_MAX_FRAME_DATA_LEN;
            }
            memcpy(&event->data.frame_data, &outputTlv.value[0],
                   event->data.frame_len);
            break;

        default:
            ALOGV("Unhandled TLV Type:%d", outputTlv.type);
            break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
        memset(&outputTlv,0, sizeof(outputTlv));
    }
    return WIFI_SUCCESS;
}

void NanCommand::getNanReceivePostConnectivityCapabilityVal(
    const u8 *pInValue,
    NanReceivePostConnectivityCapability *pRxCapab)
{
    if (pInValue && pRxCapab) {
        pRxCapab->is_mesh_supported = (pInValue[0] & (0x01 << 5));
        pRxCapab->is_ibss_supported = (pInValue[0] & (0x01 << 4));
        pRxCapab->wlan_infra_field = (pInValue[0] & (0x01 << 3));
        pRxCapab->is_tdls_supported = (pInValue[0] & (0x01 << 2));
        pRxCapab->is_wfds_supported = (pInValue[0] & (0x01 << 1));
        pRxCapab->is_wfd_supported = pInValue[0] & 0x01;
    }
}

void NanCommand::getNanReceiveSdeaCtrlParams(const u8* pInValue,
    NanSdeaCtrlParams *pPeerSdeaParams)
{
    if (pInValue && pPeerSdeaParams) {
        pPeerSdeaParams->security_cfg =
                          (NanDataPathSecurityCfgStatus)((pInValue[0] & BIT_6) ?
                           NAN_DP_CONFIG_SECURITY : NAN_DP_CONFIG_NO_SECURITY);
        pPeerSdeaParams->ranging_state =
                           (NanRangingState)((pInValue[0] & BIT_7) ?
                            NAN_RANGING_ENABLE : NAN_RANGING_DISABLE);
#if 0
        pPeerSdeaParams->enable_ranging_limit =
                         (NanRangingLimitState)((pInValue[0] & BIT_8) ?
                          NAN_RANGING_LIMIT_ENABLE : NAN_RANGING_LIMIT_DISABLE);
#endif
    }
    return;
}

void NanCommand::getNanReceivePairingParamsMatch(const u8* pInValue,
                                          NanPairingConfig *pPeerPairingParams)
{
    if (pInValue && pPeerPairingParams) {
        NanFWPairingParamsMatch *pRsp = (NanFWPairingParamsMatch *)pInValue;
        pPeerPairingParams->enable_pairing_setup =
                          pRsp->pairing_setup_required;
        pPeerPairingParams->enable_pairing_cache =
                          pRsp->npk_nik_caching_required;
        pPeerPairingParams->supported_bootstrapping_methods =
                                 pRsp->bootstrapping_method_bitmap;
    }
    return;
}

int NanCommand::getNanReceivePostDiscoveryVal(const u8 *pInValue,
                                              u32 length,
                                              NanReceivePostDiscovery *pRxDisc)
{
    int ret = 0;

    if (length <= 8 || pInValue == NULL) {
        ALOGE("%s: Invalid Arg TLV Len %d < 4",
              __func__, length);
        return -1;
    }

    pRxDisc->type = (NanConnectionType) pInValue[0];
    pRxDisc->role = (NanDeviceRole) pInValue[1];
    pRxDisc->duration = (NanAvailDuration) (pInValue[2] & 0x03);
    pRxDisc->mapid = ((pInValue[2] >> 2) & 0x0F);
    memcpy(&pRxDisc->avail_interval_bitmap,
           &pInValue[4],
           sizeof(pRxDisc->avail_interval_bitmap));

    u8 *pInputTlv = (u8 *)&pInValue[8];
    NanTlv outputTlv;
    u16 readLen = 0;
    int remainingLen = (length - 8);

    //Has Mac address
    if (remainingLen <= 0) {
        ALOGE("%s: No TLV's present",__func__);
        return -1;
    }

    ALOGV("%s: TLV remaining Len:%d",__func__, remainingLen);
    while ((remainingLen > 0) &&
           (0 != (readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen)))) {
        ALOGV("%s: Remaining Len:%d readLen:%d type:%d length:%d",
              __func__, remainingLen, readLen, outputTlv.type,
              outputTlv.length);
        switch (outputTlv.type) {
        case NAN_TLV_TYPE_MAC_ADDRESS:
            if (outputTlv.length > sizeof(pRxDisc->addr)) {
                outputTlv.length = sizeof(pRxDisc->addr);
            }
            memcpy(pRxDisc->addr, outputTlv.value, outputTlv.length);
            break;
        case NAN_TLV_TYPE_WLAN_MESH_ID:
            if (outputTlv.length > sizeof(pRxDisc->mesh_id)) {
                outputTlv.length = sizeof(pRxDisc->mesh_id);
            }
            memcpy(pRxDisc->mesh_id, outputTlv.value, outputTlv.length);
            pRxDisc->mesh_id_len = outputTlv.length;
            break;
        case NAN_TLV_TYPE_WLAN_INFRA_SSID:
            if (outputTlv.length > sizeof(pRxDisc->infrastructure_ssid_val)) {
                outputTlv.length = sizeof(pRxDisc->infrastructure_ssid_val);
            }
            memcpy(pRxDisc->infrastructure_ssid_val, outputTlv.value,
                   outputTlv.length);
            pRxDisc->infrastructure_ssid_len = outputTlv.length;
            break;
        default:
            ALOGV("Unhandled TLV Type:%d", outputTlv.type);
            break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
        memset(&outputTlv,0, sizeof(outputTlv));
    }
    return ret;
}

int NanCommand::getNanFurtherAvailabilityMap(const u8 *pInValue,
                                             u32 length,
                                             u8 *num_chans,
                                             NanFurtherAvailabilityChannel *pFac)
{
    int idx = 0;

    if ((length == 0) || pInValue == NULL) {
        ALOGE("%s: Invalid Arg TLV Len %d or pInValue NULL",
              __func__, length);
        return -1;
    }

    *num_chans = pInValue[0];
    if (*num_chans > NAN_MAX_FAM_CHANNELS) {
        ALOGE("%s: Unable to accommodate numchans %d",
              __func__, *num_chans);
        return -1;
    }

    if (length < (sizeof(u8) +
        (*num_chans * sizeof(NanFurtherAvailabilityChan)))) {
        ALOGE("%s: Invalid TLV Length", __func__);
        return -1;
    }

    for (idx = 0; idx < *num_chans; idx++) {
        pNanFurtherAvailabilityChan pRsp = \
              (pNanFurtherAvailabilityChan)((u8 *)&pInValue[1] + \
              (idx * sizeof(NanFurtherAvailabilityChan)));

        pFac->entry_control = \
            (NanAvailDuration)(pRsp->entryCtrl.availIntDuration);
        pFac->mapid = pRsp->entryCtrl.mapId;
        pFac->class_val = pRsp->opClass;
        pFac->channel = pRsp->channel;
        memcpy(&pFac->avail_interval_bitmap,
               &pRsp->availIntBitmap,
               sizeof(pFac->avail_interval_bitmap));
        pFac++;
    }
    return 0;
}

wifi_error NanCommand::getNanStaParameter(wifi_interface_handle iface,
                                   NanStaParameter *pRsp)
{
    wifi_error ret = WIFI_ERROR_NONE;
    transaction_id id = 1;
    interface_info *ifaceInfo = getIfaceInfo(iface);

    ret = create();
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /* Set the interface Id of the message. */
    ret = set_iface_id(ifaceInfo->name);
    if (ret != WIFI_SUCCESS)
        goto cleanup;

    /*
       Construct NL message to get the sync stats parameter
       which has all the parameter required by staparameter.
    */
    NanStatsRequest syncStats;
    memset(&syncStats, 0, sizeof(syncStats));
    syncStats.stats_type = NAN_STATS_ID_DE_TIMING_SYNC;
    syncStats.clear = 0;

    mStaParam = pRsp;
    ret = putNanStats(id, &syncStats);
    if (ret != WIFI_SUCCESS) {
        ALOGE("%s: putNanStats Error:%d",__func__, ret);
        goto cleanup;
    }
    ret = requestEvent();
    if (ret != 0) {
        ALOGE("%s: requestEvent Error:%d",__func__, ret);
        goto cleanup;
    }

    struct timespec abstime;
    abstime.tv_sec = 4;
    abstime.tv_nsec = 0;
    ret = mCondition.wait(abstime);
    if (ret == WIFI_ERROR_TIMED_OUT)
    {
        ALOGE("%s: Time out happened.", __func__);
        goto cleanup;
    }
    ALOGV("%s: NanStaparameter Master_pref:%x," \
          " Random_factor:%x, hop_count:%x " \
          " beacon_transmit_time:%d" \
          " ndp_channel_freq:%d", __func__,
          pRsp->master_pref, pRsp->random_factor,
          pRsp->hop_count, pRsp->beacon_transmit_time, pRsp->ndp_channel_freq);
cleanup:
    mStaParam = NULL;
    return ret;
}

int NanCommand::getNanTransmitFollowupInd(NanTransmitFollowupInd *event)
{
    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanSelfTransmitFollowupIndMsg pRsp = (pNanSelfTransmitFollowupIndMsg)mNanVendorEvent;
    event->id = pRsp->fwHeader.transactionId;
    NanErrorTranslation((NanInternalStatusType)pRsp->reason, 0,
                        (void*)event, false);
    return WIFI_SUCCESS;
}

//Function which calls the necessaryIndication callback
//based on the indication type
int NanCommand::handleNdpIndication(u32 ndpCmdType, struct nlattr **tb_vendor)
{
    //Based on the message_id in the header determine the Indication type
    //and call the necessary callback handler
    int res = 0;

    ALOGI("handleNdpIndication msg_id:%u", ndpCmdType);
    switch (ndpCmdType) {
    case QCA_WLAN_VENDOR_ATTR_NDP_REQUEST_IND:
        NanDataPathRequestInd ndpRequestInd;
        memset(&ndpRequestInd, 0, sizeof(ndpRequestInd));

        res = getNdpRequest(tb_vendor, &ndpRequestInd);
        if (!res && mHandler.EventDataRequest) {
            (*mHandler.EventDataRequest)(&ndpRequestInd);
        }
        break;

    case QCA_WLAN_VENDOR_ATTR_NDP_CONFIRM_IND:
        NanDataPathConfirmInd ndpConfirmInd;
        memset(&ndpConfirmInd, 0, sizeof(ndpConfirmInd));

        res = getNdpConfirm(tb_vendor, &ndpConfirmInd);
        if (!res && mHandler.EventDataConfirm) {
            (*mHandler.EventDataConfirm)(&ndpConfirmInd);
        }
        break;

    case QCA_WLAN_VENDOR_ATTR_NDP_END_IND:
    {
        NanDataPathEndInd *ndpEndInd = NULL;
        u8 num_ndp_ids = 0;
        u8 i;

        if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY]) {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_NDP not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }

        num_ndp_ids = (u8)(nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY])/sizeof(u32));
        ALOGD("%s: NDP Num Instance Ids : val %d", __FUNCTION__, num_ndp_ids);

        if (num_ndp_ids) {
            ndpEndInd =
                (NanDataPathEndInd *)malloc(sizeof(NanDataPathEndInd)+ (sizeof(u32) * num_ndp_ids));
            if (!ndpEndInd) {
                ALOGE("%s: ndp_instance_id malloc Failed", __FUNCTION__);
                return WIFI_ERROR_OUT_OF_MEMORY;
            }
            ndpEndInd->num_ndp_instances = num_ndp_ids;
            nla_memcpy(ndpEndInd->ndp_instance_id,
                       tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY],
                       sizeof(u32) * ndpEndInd->num_ndp_instances);
        }
        for (i = 0; i < num_ndp_ids; i++) {
            mNanCommandInstance->deleteServiceId(0,
                                                 ndpEndInd->ndp_instance_id[i],
                                                 NAN_ROLE_PUBLISHER);
        }
        if (mHandler.EventDataEnd) {
            (*mHandler.EventDataEnd)(ndpEndInd);
        }
        free(ndpEndInd);
        break;
    }

    case QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_IND:
    {
        NanDataPathScheduleUpdateInd *pNdpScheduleUpdateInd;
        u32 num_channels = 0, num_ndp_ids = 0;

        if ((!tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR]) ||
            (!tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_REASON]) ||
            (!tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY])) {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_NDP not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
        if (tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_NUM_CHANNELS]) {
             num_channels = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_NUM_CHANNELS]);
             ALOGD("%s: num_channels = %d", __FUNCTION__, num_channels);
             if ((num_channels > NAN_MAX_CHANNEL_INFO_SUPPORTED) &&
                 (!tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_INFO])) {
                 ALOGE("%s: QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_INFO not found", __FUNCTION__);
                 return WIFI_ERROR_INVALID_ARGS;
            }
        }
        num_ndp_ids = (u8)(nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY])/sizeof(u32));
        ALOGD("%s: NDP Num Instance Ids : val %d", __FUNCTION__, num_ndp_ids);

        pNdpScheduleUpdateInd =
            (NanDataPathScheduleUpdateInd *)malloc(sizeof(NanDataPathScheduleUpdateInd)
            + (sizeof(u32) * num_ndp_ids));
        if (!pNdpScheduleUpdateInd) {
            ALOGE("%s: NdpScheduleUpdate malloc Failed", __FUNCTION__);
            return WIFI_ERROR_OUT_OF_MEMORY;
        }
        pNdpScheduleUpdateInd->num_channels = num_channels;
        pNdpScheduleUpdateInd->num_ndp_instances = num_ndp_ids;

        res = getNdpScheduleUpdate(tb_vendor, pNdpScheduleUpdateInd);
        if (!res && mHandler.EventScheduleUpdate) {
            (*mHandler.EventScheduleUpdate)(pNdpScheduleUpdateInd);
        }
        free(pNdpScheduleUpdateInd);
        break;
    }
    default:
        ALOGE("handleNdpIndication error invalid ndpCmdType:%u", ndpCmdType);
        res = (int)WIFI_ERROR_INVALID_REQUEST_ID;
        break;
    }
    return res;
}

int NanCommand::getNdpRequest(struct nlattr **tb_vendor,
                              NanDataPathRequestInd *event)
{
    u32 len = 0;
    hal_info *info = getHalInfo(wifiHandle());
    struct nan_pairing_peer_info *peer = NULL;

    if (event == NULL || tb_vendor == NULL) {
        ALOGE("%s: Invalid input argument event:%p tb_vendor:%p",
              __FUNCTION__, event, tb_vendor);
        return WIFI_ERROR_INVALID_ARGS;
    }
    if ((!tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID]) ||
        (!tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR]) ||
        (!tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID])) {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_NDP not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    event->service_instance_id = nla_get_u16(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID]);
    ALOGD("%s: Service Instance id : val %d", __FUNCTION__, event->service_instance_id);

    len = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR]);
    len = ((sizeof(event->peer_disc_mac_addr) <= len) ? sizeof(event->peer_disc_mac_addr) : len);
    memcpy(&event->peer_disc_mac_addr[0], nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR]), len);

    event->ndp_instance_id = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID]);
    ALOGD("%s: Ndp Instance id: %d", __FUNCTION__, event->ndp_instance_id);
    if (tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO]) {
        len = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO]);
        len = ((sizeof(event->app_info.ndp_app_info) <= len) ? sizeof(event->app_info.ndp_app_info) : len);
        memcpy(&event->app_info.ndp_app_info[0], nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO]), len);
        event->app_info.ndp_app_info_len = len;
    } else {
        ALOGD("%s: NDP App Info not present", __FUNCTION__);
    }

    if (tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_ID]) {
            mNanCommandInstance->saveServiceId((u8 *)nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_ID]),
                                               event->service_instance_id,
                                               event->ndp_instance_id,
                                               NAN_ROLE_PUBLISHER,
                                               event->peer_disc_mac_addr);
    } else {
        ALOGD("%s: Service ID not present", __FUNCTION__);
    }

    if (info && info->secure_nan) {
        peer = nan_pairing_get_peer_from_list(info->secure_nan, event->peer_disc_mac_addr);
        if (peer)
            peer->ndp_instance_id = event->ndp_instance_id;
    }

    if (tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_CSIA_CAPABILITIES]) {
        event->csia_capabilities =
              nla_get_u8(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_CSIA_CAPABILITIES]);
        ALOGV("%s: csia capabilities: %d", __FUNCTION__, event->csia_capabilities);
    }

    if (tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_GTK_REQUIRED]) {
        event->gtk_protection = 1;
        ALOGV("%s: GTK protection enabled", __FUNCTION__);
    }
    return WIFI_SUCCESS;
}

int NanCommand::getNdpConfirm(struct nlattr **tb_vendor,
                              NanDataPathConfirmInd *event)
{
    u32 len = 0;
    NanInternalStatusType drv_reason_code;
    struct nlattr *chInfo;
    NanChannelInfo *pChInfo;
    int rem;
    u32 i = 0;

    if (event == NULL || tb_vendor == NULL) {
        ALOGE("%s: Invalid input argument event:%p tb_vendor:%p",
              __FUNCTION__, event, tb_vendor);
        return WIFI_ERROR_INVALID_ARGS;
    }
    if ((!tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID]) ||
        (!tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR]) ||
        (!tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE])) {
        ALOGE("%s: QCA_WLAN_VENDOR_ATTR_NDP not found", __FUNCTION__);
        return WIFI_ERROR_INVALID_ARGS;
    }

    event->ndp_instance_id = nla_get_u16(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID]);
    ALOGD("%s: Service Instance id : val %d", __FUNCTION__, event->ndp_instance_id);

    len = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR]);
    len = ((sizeof(event->peer_ndi_mac_addr) <= len) ? sizeof(event->peer_ndi_mac_addr) : len);
    memcpy(&event->peer_ndi_mac_addr[0], nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR]), len);

    event->rsp_code = (NanDataPathResponseCode)nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE]);
    ALOGD("%s: Response code %d", __FUNCTION__, event->rsp_code);

    if (tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO]) {
        len = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO]);
        len = ((sizeof(event->app_info.ndp_app_info) <= len) ? sizeof(event->app_info.ndp_app_info) : len);
        memcpy(&event->app_info.ndp_app_info[0], nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO]), len);
        event->app_info.ndp_app_info_len = len;
    } else {
        ALOGD("%s: NDP App Info not present", __FUNCTION__);
    }
    drv_reason_code = (NanInternalStatusType)nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE]);
    ALOGD("%s: Drv reason code %d", __FUNCTION__, drv_reason_code);
    switch (drv_reason_code) {
        case NDP_I_MGMT_FRAME_REQUEST_FAILED:
        case NDP_I_MGMT_FRAME_RESPONSE_FAILED:
        case NDP_I_MGMT_FRAME_CONFIRM_FAILED:
        case NDP_I_MGMT_FRAME_END_REQUEST_FAILED:
        case NDP_I_MGMT_FRAME_SECURITY_INSTALL_FAILED:
            event->reason_code = NAN_STATUS_PROTOCOL_FAILURE;
            break;
        case NDP_I_END_FAILED:
            event->reason_code = NAN_STATUS_INTERNAL_FAILURE;
            break;
        default:
            event->reason_code = (NanStatusType)drv_reason_code;
            break;
    }
    ALOGD("%s: Reason code %d", __FUNCTION__, event->reason_code);

    if (tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_NUM_CHANNELS]) {
        event->num_channels =
            nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_NUM_CHANNELS]);
        ALOGD("%s: num_channels = %d", __FUNCTION__, event->num_channels);
        if ((event->num_channels > NAN_MAX_CHANNEL_INFO_SUPPORTED) &&
            (!tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_INFO])) {
            ALOGE("%s: QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_INFO not found", __FUNCTION__);
            return WIFI_ERROR_INVALID_ARGS;
        }
    }

    if (event->num_channels != 0) {
        for (chInfo =
            (struct nlattr *) nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_INFO]),
            rem = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_INFO]);
            (i < NAN_MAX_CHANNEL_INFO_SUPPORTED && nla_ok(chInfo, rem));
            chInfo = nla_next(chInfo, &(rem))) {
             struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX + 1];

             pChInfo =
                 (NanChannelInfo *) ((u8 *)event->channel_info + (i++ * (sizeof(NanChannelInfo))));
             nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX,
                 (struct nlattr *) nla_data(chInfo), nla_len(chInfo), NULL);

            if (!tb2[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL]) {
                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_CHANNEL not found", __FUNCTION__);
                return WIFI_ERROR_INVALID_ARGS;
            }
            pChInfo->channel = nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL]);
            ALOGD("%s: Channel = %d", __FUNCTION__, pChInfo->channel);

            if (!tb2[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_WIDTH]) {
                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_WIDTH not found", __FUNCTION__);
                return WIFI_ERROR_INVALID_ARGS;
            }
            pChInfo->bandwidth = nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_WIDTH]);
            ALOGD("%s: Channel BW = %d", __FUNCTION__, pChInfo->bandwidth);

            if (!tb2[QCA_WLAN_VENDOR_ATTR_NDP_NSS]) {
                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_NDP_NSS not found", __FUNCTION__);
                return WIFI_ERROR_INVALID_ARGS;
            }
            pChInfo->nss = nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_NDP_NSS]);
            ALOGD("%s: No. Spatial Stream = %d", __FUNCTION__, pChInfo->nss);
        }
    }
    return WIFI_SUCCESS;
}

int NanCommand::getNdpScheduleUpdate(struct nlattr **tb_vendor,
                                     NanDataPathScheduleUpdateInd *event)
{
    u32 len = 0;
    struct nlattr *chInfo;
    NanChannelInfo *pChInfo;
    int rem;
    u32 i = 0;

    len = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR]);
    len = ((sizeof(event->peer_mac_addr) <= len) ? sizeof(event->peer_mac_addr) : len);
    memcpy(&event->peer_mac_addr[0], nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR]), len);

    event->schedule_update_reason_code = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_REASON]);
    ALOGD("%s: Reason code %d", __FUNCTION__, event->schedule_update_reason_code);

    if (event->num_channels != 0) {
        for (chInfo =
            (struct nlattr *) nla_data(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_INFO]),
            rem = nla_len(tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_INFO]);
            (i < NAN_MAX_CHANNEL_INFO_SUPPORTED && nla_ok(chInfo, rem));
            chInfo = nla_next(chInfo, &(rem))) {
            struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX + 1];

            pChInfo =
                (NanChannelInfo *) ((u8 *)event->channel_info + (i++ * (sizeof(NanChannelInfo))));
            nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX,
                (struct nlattr *) nla_data(chInfo), nla_len(chInfo), NULL);

            if (!tb2[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL]) {
                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_CHANNEL not found", __FUNCTION__);
                return WIFI_ERROR_INVALID_ARGS;
            }
            pChInfo->channel = nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL]);
            ALOGD("%s: Channel = %d", __FUNCTION__, pChInfo->channel);

            if (!tb2[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_WIDTH]) {
                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_WIDTH not found", __FUNCTION__);
                return WIFI_ERROR_INVALID_ARGS;
            }
            pChInfo->bandwidth = nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_WIDTH]);
            ALOGD("%s: Channel BW = %d", __FUNCTION__, pChInfo->bandwidth);

           if (!tb2[QCA_WLAN_VENDOR_ATTR_NDP_NSS]) {
                ALOGE("%s: QCA_WLAN_VENDOR_ATTR_NDP_NSS not found", __FUNCTION__);
                return WIFI_ERROR_INVALID_ARGS;
            }
            pChInfo->nss = nla_get_u32(tb2[QCA_WLAN_VENDOR_ATTR_NDP_NSS]);
            ALOGD("%s: No. Spatial Stream = %d", __FUNCTION__, pChInfo->nss);
        }
    }

    if (event->num_ndp_instances) {
        nla_memcpy(event->ndp_instance_id,
                   tb_vendor[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY],
                   sizeof(u32) * event->num_ndp_instances);
    }
    return WIFI_SUCCESS;
}

int NanCommand::getNanRangeRequestReceivedInd(NanRangeRequestInd *event)
{
    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanFWRangeReqRecvdInd pRsp = (pNanFWRangeReqRecvdInd)mNanVendorEvent;

    u8 *pInputTlv = pRsp->ptlv;
    NanTlv outputTlv;
    u16 readLen = 0;

    int remainingLen = (mNanDataLen -  \
        (sizeof(NanMsgHeader)));

    if (remainingLen <= 0) {
        ALOGE("%s: No TLV's present",__func__);
        return WIFI_SUCCESS;
    }

    ALOGV("%s: TLV remaining Len:%d",__func__, remainingLen);
    while ((remainingLen > 0) &&
           (0 != (readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen)))) {
        ALOGV("%s: Remaining Len:%d readLen:%d type:%d length:%d",
              __func__, remainingLen, readLen, outputTlv.type,
              outputTlv.length);
        switch (outputTlv.type) {
        case NAN_TLV_TYPE_NAN20_RANGING_REQUEST_RECEIVED:
            NanFWRangeReqRecvdMsg fwRangeReqRecvd;
            if (outputTlv.length > sizeof(fwRangeReqRecvd)) {
                outputTlv.length = sizeof(fwRangeReqRecvd);
            }
            memcpy(&fwRangeReqRecvd, outputTlv.value, outputTlv.length);
            FW_MAC_ADDR_TO_CHAR_ARRAY(fwRangeReqRecvd.range_mac_addr, event->range_req_intf_addr);
            event->publish_id = fwRangeReqRecvd.range_id;
            break;
        default:
            ALOGV("Unhandled TLV type:%d", outputTlv.type);
            break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
        memset(&outputTlv,0, sizeof(outputTlv));
    }
    return WIFI_SUCCESS;
}

int NanCommand::getNanRangeReportInd(NanRangeReportInd *event)
{
    if (event == NULL || mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument event:%p mNanVendorEvent:%p",
              __func__, event, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    pNanFWRangeReportInd pRsp = (pNanFWRangeReportInd)mNanVendorEvent;

    u8 *pInputTlv = pRsp->ptlv;
    NanTlv outputTlv;
    u16 readLen = 0;

    int remainingLen = (mNanDataLen -  \
        (sizeof(NanMsgHeader)));

    if (remainingLen <= 0) {
        ALOGE("%s: No TLV's present",__func__);
        return WIFI_SUCCESS;
    }

    ALOGV("%s: TLV remaining Len:%d",__func__, remainingLen);
    while ((remainingLen > 0) &&
           (0 != (readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen)))) {
        ALOGV("%s: Remaining Len:%d readLen:%d type:%d length:%d",
              __func__, remainingLen, readLen, outputTlv.type,
              outputTlv.length);
        switch (outputTlv.type) {
        case NAN_TLV_TYPE_MAC_ADDRESS:
            if (outputTlv.length > NAN_MAC_ADDR_LEN) {
                outputTlv.length = NAN_MAC_ADDR_LEN;
            }
            memcpy(event->range_req_intf_addr, outputTlv.value, outputTlv.length);
            break;

        case NAN_TLV_TYPE_NAN20_RANGING_RESULT:
            NanFWRangeReportParams range_params;
            if (outputTlv.length > sizeof(NanFWRangeReportParams)) {
                outputTlv.length = sizeof(NanFWRangeReportParams);
            }
            memcpy(&range_params, outputTlv.value, outputTlv.length);
            event->range_measurement_mm = range_params.range_measurement;
            event->publish_id = range_params.publish_id;
//          event->event_type = range_params.event_type;
            break;
        default:
            ALOGV("Unhandled TLV type:%d", outputTlv.type);
            break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
        memset(&outputTlv,0, sizeof(outputTlv));
    }
    return WIFI_SUCCESS;
}

/*
 * Function: getNanContinuousRangingResult
 * Populates periodic ranging result from FW
 * @callback:- EventRangingResults
*/
int NanCommand::getNanContinuousRangingResult()
{
    if (mNanVendorEvent == NULL) {
        ALOGE("%s: Invalid input argument mNanVendorEvent:%p",
              __func__, mNanVendorEvent);
        return WIFI_ERROR_INVALID_ARGS;
    }

    NanTlv outputTlv;
    u16 readLen = 0;
    int index = 0;
    unsigned num_results = 0;
    u16 session_id = 0;
    int ret  = WIFI_SUCCESS;

    pNanContRangeResultInd pRsp = (pNanContRangeResultInd)mNanVendorEvent;
    u8 *pInputTlv = pRsp->ptlv;

    ALOGD("mNanDataLen:%" PRIu32, mDataLen);
    int remainingLen = (mNanDataLen - (sizeof(NanMsgHeader)));

    if (remainingLen <= 0) {
        ALOGE("%s: No TLV's present",__func__);
        return WIFI_SUCCESS;
    }

    session_id = pRsp->fwHeader.handle;
    ALOGV("%s: TLV remaining Len:%d, session id: %u",__func__, remainingLen,
                                                    session_id);
    while ((remainingLen > 0) &&
     (0 != (readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen)))) {
     ALOGV("%s: Remaining Len:%d readLen:%d type:%d length:%d",
              __func__, remainingLen, readLen, outputTlv.type,
              outputTlv.length);
        switch (outputTlv.type) {

            case NAN_TLV_TYPE_CONTINUOUS_RANGING_RESULT:
                index += 1;
                break;
            default:
                ALOGV("Unhandled TLV type:%d", outputTlv.type);
                break;
        }
        remainingLen -= readLen;
        pInputTlv += readLen;
        memset(&outputTlv,0, sizeof(outputTlv));
    }

    wifi_rtt_result **result = (wifi_rtt_result **)
                               malloc(index * sizeof(wifi_rtt_result *));
    if (result == NULL) {
        ALOGE("Memory allocation failed for wifi rtt result array");
        return WIFI_ERROR_OUT_OF_MEMORY;
    }
    memset((void *)result, 0, index * sizeof(wifi_rtt_result*));

    for (int i = 0; i < index; i++) {
        result[i] = (wifi_rtt_result*)malloc(sizeof(wifi_rtt_result));
         if (result[i] == NULL) {
            ALOGE("Memory allocation failed for wifi rtt result");
            index = i;
            ret = WIFI_ERROR_OUT_OF_MEMORY;
            goto cleanup;
         }
    }

    index = 0;
    pInputTlv = pRsp->ptlv;
    remainingLen = (mNanDataLen - (sizeof(NanMsgHeader)));

    while ((remainingLen > 0) &&
      (0 != (readLen = NANTLV_ReadTlv(pInputTlv, &outputTlv, remainingLen)))) {
        switch (outputTlv.type) {

            case NAN_TLV_TYPE_CONTINUOUS_RANGING_RESULT:
                NanContinuousRangeResult rangeResult;
                if (outputTlv.length > sizeof(NanContinuousRangeResult)) {
                    outputTlv.length = sizeof(NanContinuousRangeResult);
                }
                memcpy(&rangeResult, outputTlv.value, outputTlv.length);

                // Populating values in rtt_result
                FW_MAC_ADDR_TO_CHAR_ARRAY(rangeResult.fw_addr, result[index]->addr);
                result[index]->measurement_number = rangeResult.num_measurements;
                result[index]->success_number = rangeResult.num_successful_measurements;
                result[index]->number_per_burst_peer = rangeResult.number_per_burst_peer;
                result[index]->burst_duration = (int)rangeResult.burst_duration_ms;
                // avg rssi in steps of 0.5dB and positive
                result[index]->rssi =
                   (wifi_rssi) ((rangeResult.avg_rssi + NOISE_FLOOR) * -2);
                result[index]->distance_mm = (int)rangeResult.distance_mm;
                result[index]->distance_sd_mm = (int)rangeResult.distance_stdev_mm;
                result[index]->ts = static_cast<wifi_timestamp>(rangeResult.meas_start_time);
                result[index]->type = RTT_TYPE_2_SIDED_11MC;
                ALOGV("Mac Address: " MAC_ADDR_STR "\n"
                      "timestamp:%" PRIu64 "\n"
                      "measurement_number:%u\n"
                      "success_number:%u\n"
                      "number_per_burst_peer:%u\n"
                      "burst_duration:%d\n"
                      "distance_mm:%d\n"
                      "distance_sd_mm:%d rssi %d\n",
                      MAC_ADDR_ARRAY(result[index]->addr),
                      result[index]->ts,
                      result[index]->measurement_number,
                      result[index]->success_number,
                      result[index]->number_per_burst_peer,
                      result[index]->burst_duration,
                      result[index]->distance_mm,
                      result[index]->distance_sd_mm,
                      result[index]->rssi);
                break;

            default:
                ALOGV("Unhandled TLV type:%d", outputTlv.type);
                break;
        }
        index += 1;
        remainingLen -= readLen;
        pInputTlv += readLen;
        memset(&outputTlv,0, sizeof(outputTlv));
    }
    num_results = (unsigned)index;
    ALOGV("%s: Number of Rtt Results: %u", __func__, num_results);

    if ( mHandler.EventRangingResults) {
     (*mHandler.EventRangingResults)(result, num_results,
                                    session_id);
    }
cleanup:
    for (int i=0; i < index; i++) {
        if (result[i]) {
            free(result[i]);
            result[i] = NULL;
        }
    }
    if (result) {
        free(result);
        result = NULL;
    }

    return ret;
}

int NanCommand::handleNanBootstrappingReqInd(NanBootstrappingRequestInd  *evt)
{

    if (mHandler.EventBootstrappingRequest)
       (*mHandler.EventBootstrappingRequest)(evt);
    return 0;
}

int NanCommand::handleNanBootstrappingConfirm(NanBootstrappingConfirmInd *evt)
{
    if (mHandler.EventBootstrappingConfirm)
       (*mHandler.EventBootstrappingConfirm)(evt);
    return 0;
}

int NanCommand::handleNanPairingReqInd(NanPairingRequestInd *evt)
{

    if (mHandler.EventPairingRequest)
       (*mHandler.EventPairingRequest)(evt);
    return 0;
}

int NanCommand::handleNanPairingConfirm(NanPairingConfirmInd *evt)
{
    if (mHandler.EventPairingConfirm)
       (*mHandler.EventPairingConfirm)(evt);
    return 0;
}
