/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "includes.h"
#include "common.h"
#include "wpa_driver_common_lib.h"
#include "driver_cmd_nl80211_common.h"
#include "driver_cmd_nl80211_extn.h"
#include "driver_cmd_nl80211_mlo.h"

struct mlo_link_state_info {
	int num_links;
	int control_mode;
	struct mlo_link_state {
		int link_id;
		int link_state;
	} link_state_info[MAX_NUM_MLD_LINKS];
};

static int get_mlo_links_state(struct nl_msg *msg, void *arg)
{
	struct genlmsghdr *msg_hdr;
	struct nlattr *tb[NL80211_ATTR_MAX_INTERNAL + 1];
	struct nlattr *tb_vendor[NL80211_ATTR_MAX_INTERNAL + 1];
	struct nlattr *vendor_data;
	int vendor_len, rem, i = 0;

	struct mlo_link_state_info *info = (struct mlo_link_state_info *)arg;
	struct nlattr *filledattr;

	msg_hdr = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
	if (nla_parse(tb, NL80211_ATTR_MAX_INTERNAL,
		      genlmsg_attrdata(msg_hdr, 0),
		      genlmsg_attrlen(msg_hdr, 0), NULL)) {
		wpa_printf(MSG_ERROR, "NL80211_ATTR_VENDOR_DATA parse error");
		return NL_SKIP;
	}

	if (!tb[NL80211_ATTR_VENDOR_DATA]) {
		wpa_printf(MSG_ERROR, "NL80211_ATTR_VENDOR_DATA not found");
		return NL_SKIP;
	}

	vendor_data = nla_data(tb[NL80211_ATTR_VENDOR_DATA]);
	vendor_len = nla_len(tb[NL80211_ATTR_VENDOR_DATA]);

	if (nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_LINK_STATE_MAX,
		      vendor_data, vendor_len, NULL)) {
		wpa_printf(MSG_ERROR, "NL80211_ATTR_VENDOR_DATA parse error");
		return NL_SKIP;
	}

	if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE]) {
		wpa_printf(MSG_ERROR,
			   "Control mode information of link state missing");
		return NL_SKIP;
	}

	if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG]) {
		wpa_printf(MSG_ERROR, "Link state config information missing");
		return NL_SKIP;
	}

	info->control_mode =
		nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE]);

	nla_for_each_nested(filledattr,
			    tb_vendor[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG],
			    rem) {
		int attr_id;
		struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_MAX + 1];

		if (nla_parse_nested(tb2,
				     QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_MAX,
				     filledattr, NULL)) {
			wpa_printf(MSG_ERROR, "get_link_state: nla_parse fail");
			return NL_SKIP;
		}

		attr_id = QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_LINK_ID;
		if (!tb2[attr_id]) {
			wpa_printf(MSG_ERROR,
				   "link_state: %s: link_id of mlo links missing",
				   __func__);
			return NL_SKIP;
		}
		info->link_state_info[i].link_id = nla_get_u8(tb2[attr_id]);

		attr_id = QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_STATE;
		if (!tb2[attr_id]) {
			wpa_printf(MSG_ERROR,
				   "link_state: %s: link_state of link_id missing",
				   __func__);
			return NL_SKIP;
		}
		info->link_state_info[i].link_state = nla_get_u32(tb2[attr_id]);

		i++;
		if (i >= MAX_NUM_MLD_LINKS)
			break;
	}

	info->num_links = i;

	return NL_OK;
}

static int check_mlo_support(struct wpa_driver_nl80211_data *drv)
{
	if (!(drv->capa.flags2 & WPA_DRIVER_FLAGS2_MLO) ||
	    !drv->sta_mlo_info.valid_links) {
		wpa_printf(MSG_ERROR,
			   "No MLO connection or MLO connection not supported");
		return WPA_DRIVER_OEM_STATUS_FAILURE;
	}
	return WPA_DRIVER_OEM_STATUS_SUCCESS;
}

static const char *mlo_mode_to_str(int mode)
{
	switch (mode) {
	case QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_DEFAULT:
		return "Default";
	case QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_USER:
		return "User";
	case QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_MIXED:
		return "Mixed";
	default:
		wpa_printf(MSG_ERROR, "Incorrect control mode");
	}
	return "Unknown control mode";
}

static const char *mlo_link_state_to_str(int state)
{
	switch (state) {
	case QCA_WLAN_VENDOR_LINK_STATE_INACTIVE:
		return "Inactive";
	case QCA_WLAN_VENDOR_LINK_STATE_ACTIVE:
		return "Active";
	default:
		wpa_printf(MSG_ERROR, "Incorrect link state");
	}
	return "Unknown link state";
}

int wpa_driver_get_mlo_links_control_mode(struct i802_bss *bss,
					  char *buf, size_t buf_len)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *nlmsg;
	struct nlattr *attr;
	struct mlo_link_state_info info;
	int len, i, ret;
	char *pos;

	ret = check_mlo_support(drv);
	if (ret != 0)
		return ret;

	os_memset(&info, 0, sizeof(info));
	nlmsg = prepare_vendor_nlmsg(drv, bss->ifname,
				     QCA_NL80211_VENDOR_SUBCMD_MLO_LINK_STATE);
	if (!nlmsg) {
		wpa_printf(MSG_ERROR,
			   "Failed to allocate nlmsg for get_ml_link_state cmd");
		return -ENOMEM;
	}

	attr = nla_nest_start(nlmsg, NL80211_ATTR_VENDOR_DATA);
	if (!attr) {
		wpa_printf(MSG_ERROR, "nlmsg start failure");
		goto error;
	}

	if (nla_put_u32(nlmsg, QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE,
			QCA_WLAN_VENDOR_LINK_STATE_OP_GET)) {
		wpa_printf(MSG_ERROR, "nlmsg put failure");
		goto error;
	}

	nla_nest_end(nlmsg, attr);

	ret = send_nlmsg((struct nl_sock *)drv->global->nl, nlmsg,
			 get_mlo_links_state, &info);

	if (ret) {
		wpa_printf(MSG_ERROR, "nlmsg send failure: %d", ret);
		return ret;
	}

	if (!info.num_links) {
		wpa_printf(MSG_ERROR, "Num of links not present");
		return -EINVAL;
	}

	/*
	 * Sample output
	 * Links: 2
	 * Control mode:0 (Default)
	 * ID: 2 State: 1 (Active)
	 * ID: 1 State: 0 (Inactive)
	 */
	ret = os_snprintf(buf, buf_len, "\nLinks: %d\nControl mode:%d (%s)\n",
			  info.num_links, info.control_mode,
			  mlo_mode_to_str(info.control_mode));
	if (os_snprintf_error(buf_len, ret)) {
		wpa_printf(MSG_ERROR, "Failed to fill the response buf");
		return -ENOMEM;
	}

	len = buf_len - ret;
	pos = buf + ret;

	for (i = 0; i < info.num_links; i++) {
		char temp[MLO_RSP_MAX_LEN];
		int link_id, link_state;

		link_id = info.link_state_info[i].link_id;
		link_state = info.link_state_info[i].link_state;

		ret = os_snprintf(temp, sizeof(temp),
				  "ID: %d State: %d (%s)\n", link_id,
				  link_state,
				  mlo_link_state_to_str(link_state));
		if (os_snprintf_error(sizeof(temp), ret)) {
			wpa_printf(MSG_ERROR,
				   "Failed to fill the response buf");
			return -ENOMEM;
		}
		pos = result_copy_to_buf(temp, pos, &len);
		if (!pos) {
			wpa_printf(MSG_ERROR,
				   "Failed to copy the response buf");
			return -ENOMEM;
		}
	}
	*pos = '\0';
	wpa_msg(drv->ctx, MSG_INFO, "%s", buf);
	return WPA_DRIVER_OEM_STATUS_SUCCESS;
error:
	free(nlmsg);
	return -ENOMEM;
}

static bool mlo_process_cmd_string(char *cmd, struct mlo_link_state_info *info)
{
	char *context = NULL;
	char *token = cmd;
	int ret;

	if (*token == '\0') {
		wpa_printf(MSG_ERROR,
			   "Link_id and Link_state info not present");
		return false;
	}

	/*
	 * Input command examples
	 * driver SET_ML_LINK_CONTROL_MODE control_mode 1 link_id 2
	 * link_state 0 link_id 1 link_state 1
	 * driver SET_ML_LINK_CONTROL_MODE control_mode 0
	 */

	info->num_links = 0;
	while (*token != '\0') {
		if (info->num_links >= MAX_NUM_MLD_LINKS)
			return false;
		if (os_strncasecmp(token, "link_id ", 8) != 0) {
			wpa_printf(MSG_ERROR,
				   "link_id param is missing");
			return false;
		}
		token += 8;
		token = skip_white_space(token);
		info->link_state_info[info->num_links].link_id =
			get_u8_from_string(token, &ret);
		if (ret < 0 ||
		    info->link_state_info[info->num_links].link_id >=
		    MAX_NUM_MLD_LINKS) {
			wpa_printf(MSG_ERROR, "Link_id:%d is invalid",
				   info->link_state_info[info->num_links].link_id);
			return false;
		}
		token = move_to_next_str(token);
		if (os_strncasecmp(token, "link_state ", 11) != 0) {
			wpa_printf(MSG_ERROR,
				   "link_state param is missing");
			return false;
		}
		token += 11;
		token = skip_white_space(token);
		info->link_state_info[info->num_links].link_state =
			get_u8_from_string(token, &ret);
		if (ret < 0 ||
		    info->link_state_info[info->num_links].link_state >
		    QCA_WLAN_VENDOR_LINK_STATE_ACTIVE) {
			wpa_printf(MSG_ERROR,
				   "Link_id:%d Link_state:%d invalid",
				   info->link_state_info[info->num_links].link_id,
				   info->link_state_info[info->num_links].link_state);
			return false;
		}
		wpa_printf(MSG_DEBUG,
			   "Link_id = %d Link_state = %d num_links = %d",
			   info->link_state_info[info->num_links].link_id,
			   info->link_state_info[info->num_links].link_state,
			   info->num_links);
		info->num_links++;
		token = move_to_next_str(token);
	}
	return true;
}

static int mlo_prepare_user_mode_nlmsg(struct nl_msg *nlmsg,
				       struct mlo_link_state_info *info)
{
	struct nlattr *attr, *link;
	int i;

	if (nla_put_u32(nlmsg, QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE,
			QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_USER)) {
		wpa_printf(MSG_ERROR,
			   "Failed to put link state control mode value");
		return -ENOMEM;
	}

	attr = nla_nest_start(nlmsg, QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG);
	if (!attr) {
		wpa_printf(MSG_ERROR,
			   "Failed to create LINK_STATE_CONFIG attribute");
		return -ENOMEM;
	}

	for (i = 0; i < info->num_links; i++) {
		link = nla_nest_start(nlmsg, i + 1);
		if (!link) {
			wpa_printf(MSG_ERROR,
				   "Failed to create LINK_STATE_CONFIG nest");
			return -ENOMEM;
		}

		if (nla_put_u8(nlmsg,
			       QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_LINK_ID,
			       info->link_state_info[i].link_id)) {
			wpa_printf(MSG_ERROR, "Failed to put Link_id");
			return -ENOMEM;
		}
		if (nla_put_u32(nlmsg,
				QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONFIG_STATE,
				info->link_state_info[i].link_state)) {
			wpa_printf(MSG_ERROR, "Failed to put Link_state");
			return -ENOMEM;
		}
		nla_nest_end(nlmsg, link);
	}
	nla_nest_end(nlmsg, attr);

	return WPA_DRIVER_OEM_STATUS_SUCCESS;
}

static int mlo_prepare_mixed_mode_nlmsg(struct nl_msg *nlmsg,
					char *cmd)
{
	struct nlattr *attr;
	u8 active_links;
	int ret;

	active_links = get_u8_from_string(cmd, &ret);
	if (ret < 0 || active_links == 0 || active_links > MAX_NUM_MLD_LINKS) {
		wpa_printf(MSG_ERROR, "Invalid Number of active links %d",
			   active_links);
		return -ENOMEM;
	}

	if (nla_put_u32(nlmsg, QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE,
			QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_MIXED)) {
		wpa_printf(MSG_ERROR,
			   "Failed to put link state control mode value");
		return -ENOMEM;
	}

	if (nla_put_u8(nlmsg,
		       QCA_WLAN_VENDOR_ATTR_LINK_STATE_MIXED_MODE_ACTIVE_NUM_LINKS,
		       active_links)) {
		wpa_printf(MSG_ERROR,
			   "Failed to put mixed mode active num value");
		return -ENOMEM;
	}

	return WPA_DRIVER_OEM_STATUS_SUCCESS;
}

int wpa_driver_set_mlo_links_control_mode(struct i802_bss *bss, char *cmd,
					  char *buf, size_t buf_len)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *nlmsg;
	struct nlattr *attr;
	struct mlo_link_state_info info;
	int ret;

	ret = check_mlo_support(drv);
	if (ret != 0)
		return ret;

	/*
	 * Input command examples
	 * driver SET_ML_LINK_CONTROL_MODE control_mode 1 link_id 2
	 * link_state 0 link_id 1 link_state 1
	 * driver SET_ML_LINK_CONTROL_MODE control_mode 0
	 */

	cmd = skip_white_space(cmd);
	if (os_strncasecmp(cmd, "control_mode ", 13) != 0) {
		wpa_printf(MSG_ERROR, "control_mode param is missing");
		return -EINVAL;
	}
	cmd += 13;
	cmd = skip_white_space(cmd);
	info.control_mode = get_s32_from_string(cmd, &ret);
	if (ret < 0 ||
	    info.control_mode <
	    QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_DEFAULT ||
	    info.control_mode > QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_MIXED) {
		wpa_printf(MSG_ERROR,
			   "Invalid control_mode %d", info.control_mode);
		return -EINVAL;
	}

	cmd = move_to_next_str(cmd);

	nlmsg = prepare_vendor_nlmsg(drv, bss->ifname,
				     QCA_NL80211_VENDOR_SUBCMD_MLO_LINK_STATE);
	if (!nlmsg) {
		wpa_printf(MSG_ERROR, "Failed to allocate nlmsg");
		return -ENOMEM;
	}
	attr = nla_nest_start(nlmsg, NL80211_ATTR_VENDOR_DATA);
	if (!attr) {
		wpa_printf(MSG_ERROR, "nl nest error");
		goto error;
	}

	if (nla_put_u32(nlmsg, QCA_WLAN_VENDOR_ATTR_LINK_STATE_OP_TYPE,
			QCA_WLAN_VENDOR_LINK_STATE_OP_SET)) {
		wpa_printf(MSG_ERROR, "nl put error");
		goto error;
	}

	switch (info.control_mode) {
	case QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_DEFAULT:
		if (nla_put_u32(nlmsg,
				QCA_WLAN_VENDOR_ATTR_LINK_STATE_CONTROL_MODE,
				QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_DEFAULT)) {
			wpa_printf(MSG_ERROR,
				   "nl put for link state control error");
			goto error;
		}
		break;
	case QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_USER:
		if (!mlo_process_cmd_string(cmd, &info)) {
			wpa_printf(MSG_ERROR, "Failed to parse cmd string");
			nlmsg_free(nlmsg);
			return -EINVAL;
		}

		ret = mlo_prepare_user_mode_nlmsg(nlmsg, &info);
		if (ret != WPA_DRIVER_OEM_STATUS_SUCCESS)
			goto error;
		break;
	case QCA_WLAN_VENDOR_LINK_STATE_CONTROL_MODE_MIXED:
		ret = mlo_prepare_mixed_mode_nlmsg(nlmsg, cmd);
		if (ret != WPA_DRIVER_OEM_STATUS_SUCCESS)
			goto error;
		break;
	default:
		wpa_printf(MSG_ERROR, "Invalid control_mode parameter");
		nlmsg_free(nlmsg);
		return -EINVAL;
	}
	nla_nest_end(nlmsg, attr);

	ret = send_nlmsg((struct nl_sock *)drv->global->nl, nlmsg,
			 NULL, NULL);
	if (ret) {
		wpa_printf(MSG_ERROR, "nlmsg send failure: %d", ret);
		return ret;
	}
	return WPA_DRIVER_OEM_STATUS_SUCCESS;
error:
	nlmsg_free(nlmsg);
	return -ENOMEM;
}
