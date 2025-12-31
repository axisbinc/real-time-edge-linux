/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __STMMAC_GENAVB_H__
#define __STMMAC_GENAVB_H__

#include <linux/types.h>
#include <linux/device.h>
#include <linux/fec.h>

#ifdef CONFIG_STMMAC_GENAVB

struct device *stmmac_enet_avb_get_device(const char *ifname);
int stmmac_enet_avb_register(const char *ifname, const struct avb_ops *avb, void *data);
int stmmac_enet_avb_unregister(int ifindex, const struct avb_ops *avb);

int stmmac_enet_get_tx_queue_properties(int ifindex, struct tx_queue_properties *prop);
int stmmac_enet_set_idle_slope(void *data, unsigned int queue_id, u32 idle_slope);

int stmmac_enet_rx_poll_avb(void *data);
int stmmac_enet_start_xmit_avb(void *data, struct avb_tx_desc *avb_buff);
void stmmac_enet_finish_xmit_avb(void *data, unsigned int queue_id);
int stmmac_enet_tx_avb(void *data);

int stmmac_ptp_read_cnt(void *data, u32 *cnt);
int stmmac_ptp_tc_start(void *data, u8 id, u32 ts_0, u32 ts_1, u32 tcsr_val);
void stmmac_ptp_tc_stop(void *data, u8 id);
int stmmac_ptp_tc_reload(void *data, u8 id, u32 ts);

#endif /* CONFIG_STMMAC_GENAVB */

#endif /* __STMMAC_GENAVB_H__ */
