#ifndef STMMAC_RXC_H
#define STMMAC_RXH_H

#include <linux/types.h>
#include "stmmac.h"

// DMA Channel to use for AVB (FRP rules will direct AVTP traffic here)
#define STMMAC_AVB_CHANNEL 4

// Generic structure for AVB TX data
struct avb_tx_desc {
	const u8 *data;  // Pointer to raw packet data
	u32 len;         // Length of the packet data
};

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
 * stmmac_avb_xmit_avb_tx_desc - Transmit AVB frame using DMA channel 4
 * @priv: stmmac private structure
 * @queue: not used (reserved for future use)
 * @avb_desc: structure with AVB data pointer and size
 */
int stmmac_avb_xmit_avb_tx_desc(struct stmmac_priv *priv, int queue,
				struct avb_tx_desc *avb_desc);

/**
 * stmmac_avb_test_rxp - Example test to program FRP rules for AVTP traffic
 * @priv: stmmac private structure
 * Returns: 0 on success or error code
 */
int stmmac_avb_test_rxp(struct stmmac_priv *priv);

#endif // STMMAC_RXC_H
