/*
 Copyright (c) 2025 AxisB Inc.
*/
#ifndef STMMAC_RXP_H
#define STMMAC_RXP_H

#include <linux/types.h>
#include "stmmac.h"

// DMA Channel to use for AVB (FRP rules will direct AVTP traffic here)
#define STMMAC_AVB_CHANNEL 4

/**
 * stmmac_avb_test_is_in_progress - Check if AVB self-test is running
 * Returns: true if test is active, false otherwise
 */
bool stmmac_avb_test_is_in_progress(void);

/**
 * stmmac_rxp_setup - Install FRP rules to redirect given EtherTypes to AVB DMA
 * @priv: stmmac private structure
 * @eth_types: Array of 16-bit EtherTypes to match
 * @count: Number of EtherTypes
 */
int stmmac_rxp_setup(struct stmmac_priv *priv, u16 eth_types[], u16 count);

/**
 * stmmac_rxp_clear - Remove all FRP rules from MAC
 * @priv: stmmac private structure
 */
int stmmac_rxp_clear(struct stmmac_priv *priv);

/**
 * stmmac_avb_test_rxp - Example test to program FRP rules for AVTP traffic
 * @priv: stmmac private structure
 * Returns: 0 on success or error code
 */
int stmmac_avb_test_rxp(struct stmmac_priv *priv);

/**
 * stmmac_avb_rx_poll - Handle AVB RX polling
 * @data: Pointer to struct stmmac_priv
 *
 * Returns non-zero if packets were processed.
 */
int stmmac_avb_rx_poll(void *data);

/**
 * stmmac_avb_start_xmit - Start AVB TX transmission
 * @data: Pointer to struct stmmac_priv
 * @avb_desc: Pointer to AVB buffer descriptor
 *
 * Returns 0 on success, negative error code on failure.
 */
int stmmac_avb_start_xmit(void *data, struct stmmac_avb_buffer *avb_desc);

/**
 * stmmac_avb_tx_complete - Handle completed AVB TX descriptors
 * @data: Pointer to struct stmmac_priv
 *
 * Returns non-zero if timestamp callback was triggered.
 */
int stmmac_avb_tx_complete(void *data);

#endif // STMMAC_RXP_H
