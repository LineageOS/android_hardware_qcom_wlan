/* Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "includes.h"
#include "common.h"
#include "wpa_driver_common_lib.h"
#include "qca-vendor_copy.h"
#include "driver_cmd_nl80211_common.h"
#include "driver_cmd_nl80211_extn.h"
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/object-api.h>
#include <linux/pkt_sched.h>
#include <net/if.h>

#define MGMT_FRAME_RX_TX BIT(0)
#define DATA_FRAME_RX_TX BIT(1)
#define CTRL_FRAME_RX_TX BIT(2)

#define MIN_MONITOR_MODE BIT(0)
#define MAX_MONITOR_MODE (MGMT_FRAME_RX_TX | DATA_FRAME_RX_TX | CTRL_FRAME_RX_TX)

#define MON_STATUS_RUNNING "running"
#define MON_STATUS_STOPPED "stopped"

static int add_interface(struct wpa_driver_nl80211_data *drv,
			 char *ifname,
			 char *add_ifname,
			 enum nl80211_iftype add_iftype)
{
	struct nl_msg *nlmsg = NULL;
	int ret = 0;

	if (!ifname || !add_ifname)
		return -1;

	if (!(nlmsg = prepare_nlmsg(drv, ifname, NL80211_CMD_NEW_INTERFACE, 0, 0)) ||
	    nla_put_u32(nlmsg, NL80211_ATTR_IFTYPE, add_iftype) ||
	    nla_put_string(nlmsg, NL80211_ATTR_IFNAME, add_ifname)) {
		wpa_printf(MSG_ERROR, "Failed to prepare nlmsg for add interface");
		nlmsg_free(nlmsg);
		return -1;
	}

	ret = send_nlmsg((struct nl_sock *)drv->global->nl, nlmsg, NULL, NULL);
	if (ret)
		wpa_printf(MSG_ERROR, "add_interface() Failed - %s", add_ifname);

	return ret;
}

static int delete_interface(struct wpa_driver_nl80211_data *drv,
			    char *ifname)
{
	struct nl_msg *nlmsg = NULL;
	int ret = 0;

	if (!ifname)
		return 0;

	wpa_printf(MSG_DEBUG, "Deleting ifname: %s ifindex:%d", ifname, if_nametoindex(ifname));

	nlmsg = prepare_nlmsg(drv, ifname, NL80211_CMD_DEL_INTERFACE, 0, 0);
	if (!nlmsg) {
		wpa_printf(MSG_ERROR, "Failed to allocate nlmsg for interface deletion");
		return -1;
	}

	ret = send_nlmsg((struct nl_sock *)drv->global->nl, nlmsg,
			  NULL, NULL);
	if (ret)
		wpa_printf(MSG_ERROR, "delete_interface() failed - %s", ifname);

	return ret;
}

static int up_interface(char *ifname)
{
	struct ifreq ifr;
	int sock;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		wpa_printf(MSG_ERROR, "%s :socket error, Failed to bring up iface\n", __func__);
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(sock, SIOCGIFFLAGS, &ifr) != 0) {
		wpa_printf(MSG_ERROR, "%s :Could not read interface %s flags\n", __func__, ifname);
		close(sock);
		return -1;
	}
	ifr.ifr_flags |= IFF_UP;
	if (ioctl(sock, SIOCSIFFLAGS, &ifr) != 0) {
		wpa_printf(MSG_ERROR, "%s :Could not bring iface %s up\n", __func__, ifname);
		close(sock);
		return -1;
	}
	close(sock);
	return 0;
}

int wpa_driver_start_mon(struct i802_bss *bss, char *cmd)
{
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *nlmsg = NULL;
	struct nlattr *attr = NULL;
	char *mon_ifname;
	int ret = -1;
	int nlmsg_fail = 0;
	int monitor_mode = atoi(cmd);

	wpa_printf(MSG_DEBUG, "monitor mode: %d", monitor_mode);

	if (monitor_mode < MIN_MONITOR_MODE || monitor_mode > MAX_MONITOR_MODE)
		return -1;

	cmd = get_next_arg(cmd);
	mon_ifname = skip_white_space(cmd);
	if (if_nametoindex(mon_ifname)) {
		wpa_printf(MSG_ERROR, "%s interface is already started", mon_ifname);
		return -1;
	}

	if (add_interface(drv, bss->ifname, mon_ifname, NL80211_IFTYPE_MONITOR))
		return -1;

	if (up_interface(mon_ifname))
		goto err;

	nlmsg = prepare_vendor_nlmsg(drv, mon_ifname,
				     QCA_NL80211_VENDOR_SUBCMD_SET_MONITOR_MODE);
	if (!nlmsg) {
		wpa_printf(MSG_ERROR, "Failed to allocate nlmsg");
		goto err;
	}

	attr = nla_nest_start(nlmsg, NL80211_ATTR_VENDOR_DATA);
	if (attr == NULL)
		goto err;

	if (monitor_mode & MGMT_FRAME_RX_TX) {
		if (nla_put_u32(nlmsg,
				QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE,
				QCA_WLAN_VENDOR_MONITOR_MGMT_FRAME_TYPE_ALL) ||
		    nla_put_u32(nlmsg,
				QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE,
				QCA_WLAN_VENDOR_MONITOR_MGMT_FRAME_TYPE_ALL))
			goto err;
	}
	if (monitor_mode & DATA_FRAME_RX_TX) {
		if (nla_put_u32(nlmsg,
				QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_DATA_TX_FRAME_TYPE,
				QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_ALL) ||
		    nla_put_u32(nlmsg,
				QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_DATA_RX_FRAME_TYPE,
				QCA_WLAN_VENDOR_MONITOR_DATA_FRAME_TYPE_ALL))
			goto err;
	}
	if (monitor_mode & CTRL_FRAME_RX_TX) {
		if (nla_put_u32(nlmsg,
				QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE,
				QCA_WLAN_VENDOR_MONITOR_CTRL_FRAME_TYPE_ALL) ||
		    nla_put_u32(nlmsg,
				QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE,
				QCA_WLAN_VENDOR_MONITOR_CTRL_FRAME_TYPE_ALL))
			goto err;
	}
	nla_nest_end(nlmsg, attr);

	ret = send_nlmsg((struct nl_sock *)drv->global->nl, nlmsg, NULL, NULL);
	if (ret) {
		// send_nlmsg cleansup nlmsg, thus delete interface instead of goto err
		delete_interface(drv, mon_ifname);
		wpa_printf(MSG_ERROR, "send_nlmsg() Failed - %s", mon_ifname);
	}

	return ret;
err:
	delete_interface(drv, mon_ifname);
	if (nlmsg)
		nlmsg_free(nlmsg);
	return ret;
}

int wpa_driver_stop_mon(struct i802_bss *bss, char *cmd)
{
	struct nl_msg *nlmsg = NULL;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	char *ifname;
	int ret = -1;

	ifname = skip_white_space(cmd);
	if (if_nametoindex(ifname))
		ret = delete_interface(drv, ifname);

	return ret;
}

int wpa_driver_get_mon_status(struct i802_bss *bss,
					 char *cmd,
					 char *buf,
					 size_t buf_len)
{
	struct nl_msg *nlmsg = NULL;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int ret;
	struct resp_info reply_info;
	char *ifname;

	if (!buf || !buf_len)
		return -1;

	ifname = skip_white_space(cmd);
	wpa_printf(MSG_DEBUG, "ifname is: %s", ifname);

	if (!if_nametoindex(ifname))
		return snprintf(buf, buf_len, MON_STATUS_STOPPED);

	nlmsg = prepare_vendor_nlmsg(drv, ifname,
				     QCA_NL80211_VENDOR_SUBCMD_GET_MONITOR_MODE);
	if (!nlmsg) {
		wpa_printf(MSG_ERROR, "Failed to allocate nlmsg");
		return -1;
	}

	os_memset(&reply_info, 0, sizeof(struct resp_info));
	reply_info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_MONITOR_MODE;

	os_memset(buf, 0, buf_len);
	reply_info.reply_buf = buf;
	reply_info.reply_buf_len = buf_len;

	ret = send_nlmsg((struct nl_sock *)drv->global->nl, nlmsg,
			  response_handler, &reply_info);
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to send nlmsg to get monitor mode");
		return ret;
	}

	return strlen(reply_info.reply_buf);
}


void mon_response_handler(struct resp_info *info,
			  struct nlattr *vendata,
			  int datalen)
{
	struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_GET_MONITOR_MODE_MAX + 1];
	int status = 0;

	if (nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_GET_MONITOR_MODE_MAX,
		      vendata, datalen, NULL)) {
		wpa_printf(MSG_ERROR, "NL80211_ATTR_VENDOR_DATA parse error");
		return;
	}

	if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_GET_MONITOR_MODE_STATUS]) {
		wpa_printf(MSG_ERROR, "Failed to get monitor mode status");
		return;
	}

	switch (nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_GET_MONITOR_MODE_STATUS])) {
	case QCA_WLAN_VENDOR_MONITOR_MODE_CAPTURE_RUNNING:
		status = 1;
		break;
	default:
		status = 0;
		break;
	}
	snprintf(info->reply_buf, info->reply_buf_len, status ?
		 MON_STATUS_RUNNING : MON_STATUS_STOPPED);
}
