/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define MLO_RSP_MAX_LEN 200

int wpa_driver_get_mlo_links_control_mode(struct i802_bss *bss, char *buf,
					  size_t buf_len);
int wpa_driver_set_mlo_links_control_mode(struct i802_bss *bss, char *cmd,
					  char *buf, size_t buf_len);
