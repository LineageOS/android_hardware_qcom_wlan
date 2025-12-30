/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include <sys/types.h>
#include <fcntl.h>
#include <net/if.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>

#include "qca-vendor_copy.h"
#include "common.h"
#include "driver_nl80211.h"
#include "wpa_supplicant_i.h"
#include "wpa_driver_common_lib.h"
#include "driver_cmd_nl80211_common.h"
#include "driver_cmd_nl80211_extn.h"

/* OEM Command IDs */
/*
 * PA detection:
 * - Trigger: Detection is triggered via a specific command.
 * - Data Collection: PDADC values are gathered during channel transitions.
 * - Auto-Stop: Detection halts automatically after the transition to prevent
 *   performance degradation.
 * - Reporting: Results are sent back using TLVs with lengths indicating the
 *   number of channels involved
 */
/* OEM command to trigger PA detection */
#define OEM_CMD_TRIGGER_PA_DETECTION  74
/* OEM command to get PA report (PA channel freq, PDADC per chain) */
#define OEM_CMD_GET_PA_REPORT  75

/* OEM TLV Types */
#define OEM_DATA_TLV_TYPE_HEADER  1
#define OEM_DATA_TLV_PA_REPORT 147

#define SIZEOF_TLV_HDR 4
#define MAX_OEM_BUF_LEN 1024

static int oem_data_response_handler(struct nl_msg *msg, void *arg);

struct oem_data_header {
	u16 cmd_id;
	u16 request_idx;
};

struct oem_data_tlv {
	u16 type;
	u16 length;
	u8 *value;
};

/* Helper function to write TLV data */
static u16 oem_tlv_write_tlv(struct oem_data_tlv *in_tlv, u8 *out_tlv)
{
	u16 write_len = 0;
	u16 i;

	if (!in_tlv || !out_tlv) {
		return write_len;
	}

	*out_tlv++ = in_tlv->type & 0xFF;
	*out_tlv++ = (in_tlv->type & 0xFF00) >> 8;
	write_len += 2;

	*out_tlv++ = in_tlv->length & 0xFF;
	*out_tlv++ = (in_tlv->length & 0xFF00) >> 8;
	write_len += 2;

	for (i = 0; i < in_tlv->length; ++i)
		*out_tlv++ = in_tlv->value[i];

	write_len += in_tlv->length;
	return write_len;
}

/* Helper function to add TLV data */
static u8 *add_tlv(u16 type, u16 length, const u8 *value, u8 *out_tlv)
{
	struct oem_data_tlv oem_tlv;
	u16 len;

	oem_tlv.type = type;
	oem_tlv.length = length;
	oem_tlv.value = (u8 *)value;

	len = oem_tlv_write_tlv(&oem_tlv, out_tlv);
	return (out_tlv + len);
}

/* Helper function to initialize OEM data command header */
static u8 *init_oem_data_cmd_hdr(u8 *tlvs, u16 cmd_id)
{
	struct oem_data_header hdr;
	hdr.cmd_id = cmd_id;
	hdr.request_idx = 0;
	return add_tlv(OEM_DATA_TLV_TYPE_HEADER, sizeof(hdr), (u8 *)&hdr, tlvs);
}

/* Helper function to read TLV data */
static u16 oem_read_tlv(u8 *p_in_tlv, struct oem_data_tlv *p_out_tlv, int rem_len)
{
	u16 read_len = 0;

	if (!p_in_tlv || !p_out_tlv || rem_len < SIZEOF_TLV_HDR) {
		return 0;
	}

	p_out_tlv->type = *p_in_tlv++;
	p_out_tlv->type |= (*p_in_tlv++) << 8;
	read_len += 2;

	p_out_tlv->length = *p_in_tlv++;
	p_out_tlv->length |= (*p_in_tlv++) << 8;
	read_len += 2;

	if (p_out_tlv->length > (rem_len - SIZEOF_TLV_HDR)) {
		return 0;
	}

	p_out_tlv->value = p_in_tlv;
	read_len += p_out_tlv->length;

	return read_len;
}

static int handle_trigger_pa_detection(struct i802_bss *bss, char *cmd)
{
	struct wpa_driver_nl80211_data *drv;
	u8 *oem_buf, *tlvs;
	int oem_buf_len, ret;
	struct nlattr *vendor_data;
	struct nl_msg *nlmsg;

	if (!bss || !bss->drv) {
		wpa_printf(MSG_ERROR, "%s: invalid bss context", __func__);
		return -EINVAL;
	}
	drv = bss->drv;

	oem_buf_len = MAX_OEM_BUF_LEN;
	oem_buf = os_malloc(oem_buf_len);
	if (!oem_buf) {
		return -ENOMEM;
	}

	tlvs = oem_buf;
	tlvs = init_oem_data_cmd_hdr(tlvs, OEM_CMD_TRIGGER_PA_DETECTION);
	if (!tlvs) {
		os_free(oem_buf);
		return -EINVAL;
	}

	oem_buf_len = tlvs - oem_buf;

	nlmsg = prepare_vendor_nlmsg(drv, bss->ifname, QCA_NL80211_VENDOR_SUBCMD_OEM_DATA);
	if (!nlmsg) {
		os_free(oem_buf);
		return -ENOMEM;
	}

	vendor_data = nla_nest_start(nlmsg, NL80211_ATTR_VENDOR_DATA);
	if (!vendor_data) {
		nlmsg_free(nlmsg);
		os_free(oem_buf);
		return -ENOMEM;
	}

	ret = nla_put(nlmsg, QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_DATA, oem_buf_len, oem_buf);
	if (ret) {
		nlmsg_free(nlmsg);
		os_free(oem_buf);
		return ret < 0 ? ret : -1;
	}

	nla_nest_end(nlmsg, vendor_data);
	ret = send_and_recv_msgs(drv, nlmsg, NULL, NULL, NULL, NULL);
	os_free(oem_buf);

	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: oem_data command failed; err=%d", ret);
		return ret < 0 ? ret : -1;
	}

	return 0;
}

static int handle_get_pa_report(struct i802_bss *bss, char *cmd, char *buf, size_t buf_len)
{
	struct wpa_driver_nl80211_data *drv;
	u8 *oem_buf, *tlvs;
	int oem_buf_len, ret;
	struct nlattr *vendor_data, *attr;
	struct nl_msg *nlmsg;
	struct resp_info info;

	if (!bss || !bss->drv) {
		wpa_printf(MSG_ERROR, "%s: invalid bss context\n", __func__);
		return -EINVAL;
	}
	drv = bss->drv;

	os_memset(&info, 0, sizeof(info));
	info.reply_buf = buf;
	info.reply_buf_len = buf_len;
	info.subcmd = OEM_CMD_GET_PA_REPORT;

	oem_buf_len = SIZEOF_TLV_HDR + sizeof(struct oem_data_header);
	oem_buf = os_malloc(oem_buf_len);
	if (!oem_buf) {
		return -ENOMEM;
	}

	tlvs = oem_buf;
	tlvs = init_oem_data_cmd_hdr(tlvs, OEM_CMD_GET_PA_REPORT);
	if (!tlvs) {
		os_free(oem_buf);
		return -EINVAL;
	}

	oem_buf_len = tlvs - oem_buf;

	nlmsg = prepare_vendor_nlmsg(drv, bss->ifname, QCA_NL80211_VENDOR_SUBCMD_OEM_DATA);
	if (!nlmsg) {
		os_free(oem_buf);
		return -ENOMEM;
	}

	vendor_data = nla_nest_start(nlmsg, NL80211_ATTR_VENDOR_DATA);
	if (!vendor_data) {
		nlmsg_free(nlmsg);
		os_free(oem_buf);
		return -ENOMEM;
	}

	ret = nla_put(nlmsg, QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_DATA, oem_buf_len, oem_buf);
	if (ret) {
		nlmsg_free(nlmsg);
		os_free(oem_buf);
		return ret;
	}

	ret = nla_put_flag(nlmsg, QCA_WLAN_VENDOR_ATTR_OEM_DATA_RESPONSE_EXPECTED);
	if (ret) {
		nlmsg_free(nlmsg);
		os_free(oem_buf);
		return ret < 0 ? ret : -1;
	}

	nla_nest_end(nlmsg, vendor_data);
	ret = send_and_recv_msgs(drv, nlmsg, oem_data_response_handler, &info, NULL, NULL);
	os_free(oem_buf);

	if (ret) {
		wpa_printf(MSG_DEBUG, "nl80211: oem_data command failed; err=%d", ret);
		return ret < 0 ? ret : -1;
	}

	return strlen(buf);
}

/* Response handler for OEM data commands */
static int oem_data_response_handler(struct nl_msg *msg, void *arg)
{
	struct resp_info *info = (struct resp_info *)arg;
	struct nlattr *vendor_data, *attr;
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct oem_data_tlv tlv;
	u8 *data;
	int len, rem;

	if (!info) {
		return NL_SKIP;
	}

	vendor_data = nla_find(genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NL80211_ATTR_VENDOR_DATA);
	if (!vendor_data) {
		return NL_SKIP;
	}

	attr = nla_find(nla_data(vendor_data), nla_len(vendor_data), QCA_WLAN_VENDOR_ATTR_OEM_DATA_CMD_DATA);
	if (!attr) {
		return NL_SKIP;
	}

	data = nla_data(attr);
	len = nla_len(attr);

	/* Process the response based on the command */
	if (info->subcmd == OEM_CMD_GET_PA_REPORT) {
		int valid_report = 0;

		wpa_printf(MSG_DEBUG, "oem event received, subcmd: %d", info->subcmd);

		/* Skip the header TLV */
		rem = oem_read_tlv(data, &tlv, len);
		if (rem <= 0) {
			wpa_printf(MSG_DEBUG, "oem event skipped, no content");
			return NL_SKIP;
		}

		data += rem;
		len -= rem;

		/* Process the response TLVs */
		while (len >= SIZEOF_TLV_HDR) {
			rem = oem_read_tlv(data, &tlv, len);
			if (rem <= 0) {
				break;
			}

			switch (tlv.type) {
			case OEM_DATA_TLV_PA_REPORT:
				valid_report = 1;
				if (tlv.length == 0) {
					// No data available
					wpa_printf(MSG_DEBUG, "PA_REPORT: Empty data");
					snprintf(info->reply_buf, info->reply_buf_len, "PA_REPORT: Empty data");
				} else {
					// Case 1: Single channel info (8 bytes total: 4 freq + 4 pdadc)
					if (tlv.length >= 8) {
						u32 freq = WPA_GET_LE32(tlv.value);
						u32 pdadc = WPA_GET_LE32(tlv.value + 4);

						u8 chain0_pdadc = pdadc & 0xFF;
						u8 chain1_pdadc = (pdadc >> 8) & 0xFF;
						u8 chain2_pdadc = (pdadc >> 16) & 0xFF;
						u8 chain3_pdadc = (pdadc >> 24) & 0xFF;

						wpa_printf(MSG_DEBUG,
						   "PA_REPORT: Channel[0] freq=%u, PDADC values: "
						   "chain0=%u, chain1=%u, chain2=%u, chain3=%u",
						   freq, chain0_pdadc, chain1_pdadc, chain2_pdadc, chain3_pdadc);

						snprintf(info->reply_buf, info->reply_buf_len,
						    "PA_REPORT: Channel[0] freq=%u, PDADC values: "
						    "chain0=%u, chain1=%u, chain2=%u, chain3=%u",
						    freq, chain0_pdadc, chain1_pdadc, chain2_pdadc, chain3_pdadc);
					}

					// Case 2: Two channel info (16 bytes total: 2*(4 freq + 4 pdadc))
					if (tlv.length >= 16) {
						u32 freq2 = WPA_GET_LE32(tlv.value + 8);
						u32 pdadc2 = WPA_GET_LE32(tlv.value + 12);

						u8 chain0_pdadc2 = pdadc2 & 0xFF;
						u8 chain1_pdadc2 = (pdadc2 >> 8) & 0xFF;
						u8 chain2_pdadc2 = (pdadc2 >> 16) & 0xFF;
						u8 chain3_pdadc2 = (pdadc2 >> 24) & 0xFF;

						wpa_printf(MSG_DEBUG,
						   "PA_REPORT: channel[1] freq=%u, PDADC values: "
						   "chain0=%u, chain1=%u, chain2=%u, chain3=%u",
						   freq2, chain0_pdadc2, chain1_pdadc2, chain2_pdadc2, chain3_pdadc2);

						// For second channel, append to existing buffer
						int current_len = strlen(info->reply_buf);
						if (current_len < info->reply_buf_len - 1) {
							snprintf(info->reply_buf + current_len,
							    info->reply_buf_len - current_len,
							    "\nPA_REPORT: Channel[1] freq=%u, PDADC values: "
							    "chain0=%u, chain1=%u, chain2=%u, chain3=%u",
							freq2, chain0_pdadc2, chain1_pdadc2, chain2_pdadc2, chain3_pdadc2);
						}
					}
				}
				break;
			}
			data += rem;
			len -= rem;
		}

		if (!valid_report)
			snprintf(info->reply_buf, info->reply_buf_len, "FAIL (handle oem data event)");
	}

	return NL_SKIP;
}

/* Function to integrate with wpa_driver_nl80211_driver_cmd */
/* returns:  (< 0 Error) or (= 0 OK) or (> 0 buf content) */
int wpa_driver_oem_data_cmd(struct i802_bss *bss, char *cmd, char *buf, size_t buf_len)
{
	wpa_printf(MSG_DEBUG, "oem command: %s", cmd);

	if (os_strncasecmp(cmd, "TRIGGER_PA_DETECTION", 20) == 0) {
		return handle_trigger_pa_detection(bss, cmd + 20);
	} else if (os_strncasecmp(cmd, "GET_PA_REPORT", 13) == 0) {
		return handle_get_pa_report(bss, cmd + 13, buf, buf_len);
	}

	return -EOPNOTSUPP;
}
