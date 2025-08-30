/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * include/linux/stmmac_avb.h
 *
 * Copyright (C) 2023 NXP
 *
 * Header file for the STMMAC AVB driver exported functions
 */
#ifndef __LINUX_STMMAC_AVB_H__
#define __LINUX_STMMAC_AVB_H__

#include <linux/fec.h>
#include <linux/device.h>
#include <linux/types.h>

/* STMMAC AVB Interface Functions */

/**
 * stmmac_enet_avb_get_device - Get device from interface name
 * @ifname: Network interface name
 *
 * Retrieves the device structure associated with the given interface name.
 * Return: Pointer to device on success, NULL on failure
 */
struct device *stmmac_enet_avb_get_device(const char *ifname);

/**
 * stmmac_enet_avb_register - Register AVB operations
 * @ifname: Network interface name
 * @avb: AVB operations structure
 * @data: Private data pointer
 *
 * Registers AVB operations for the specified network interface.
 * Return: Interface index on success, negative error code on failure
 */
int stmmac_enet_avb_register(const char *ifname, const struct avb_ops *avb, void *data);

/**
 * stmmac_enet_avb_unregister - Unregister AVB operations
 * @ifindex: Network interface index
 * @avb: AVB operations structure
 *
 * Unregisters AVB operations for the specified network interface.
 * Return: 0 on success, negative error code on failure
 */
int stmmac_enet_avb_unregister(int ifindex, const struct avb_ops *avb);

/**
 * stmmac_enet_get_tx_queue_properties - Get TX queue properties
 * @ifindex: Network interface index
 * @prop: Pointer to queue properties structure to fill
 *
 * Retrieves the transmit queue properties for the specified interface.
 * Return: 0 on success, negative error code on failure
 */
int stmmac_enet_get_tx_queue_properties(int ifindex, struct tx_queue_properties *prop);

/**
 * stmmac_enet_set_idle_slope - Set idle slope for queue
 * @data: Private data pointer
 * @queue_id: Queue identifier
 * @idle_slope: Idle slope value
 *
 * Sets the idle slope parameter for credit-based shaper on specified queue.
 * Return: 0 on success, negative error code on failure
 */
int stmmac_enet_set_idle_slope(void *data, unsigned int queue_id, __u32 idle_slope);

/**
 * stmmac_enet_rx_poll_avb - Poll for AVB RX packets
 * @data: Private data pointer
 *
 * Polls for received AVB packets and processes them.
 * Return: Number of packets processed, negative error code on failure
 */
int stmmac_enet_rx_poll_avb(void *data);

/**
 * stmmac_enet_start_xmit_avb - Start AVB packet transmission
 * @data: Private data pointer
 * @avb_buff: AVB transmit descriptor
 *
 * Initiates transmission of an AVB packet.
 * Return: 0 on success, negative error code on failure
 */
int stmmac_enet_start_xmit_avb(void *data, struct avb_tx_desc *avb_buff);

/**
 * stmmac_enet_finish_xmit_avb - Finish AVB packet transmission
 * @data: Private data pointer
 * @queue_id: Queue identifier
 *
 * Completes the transmission process for AVB packets on specified queue.
 */
void stmmac_enet_finish_xmit_avb(void *data, unsigned int queue_id);

/**
 * stmmac_enet_tx_avb - Transmit AVB packets
 * @data: Private data pointer
 *
 * Processes pending AVB transmit packets.
 * Return: Number of packets transmitted, negative error code on failure
 */
int stmmac_enet_tx_avb(void *data);

#endif /* __LINUX_STMMAC_AVB_H__ */
