/*
 Copyright (c) 2025 AxisB Inc.
*/
#include "stmmac_rxp.h"
#include "stmmac_frp.h"
#include "stmmac.h"

static bool avb_test_running;

bool stmmac_avb_test_is_in_progress(void)
{
	return avb_test_running;
}

/**
 * stmmac_rxp_setup - Configure RX Parser to divert specific ethertypes to AVB DMA channel
 * @priv: Driver private data
 * @eth_types: Array of ethertypes to match (e.g., 0x22F0 for AVTP)
 * @count: Number of ethertypes
 */
int stmmac_rxp_setup(struct stmmac_priv *priv, u16 eth_types[], u16 count)
{
	union frp_instruction instr;
	uint32_t config = 0;
	int ret = 0, i, entry = 0;
	
	pr_info("STMMAC_RXP: Starting RXP setup for %u ethertypes\n", count);
	
	if (count * 2 + 1 > 64) {
		pr_err("STMMAC_RXP: Too many entries. Max supported: 64, requested: %u\n", count * 2 + 1);
		return -EINVAL;
	}

	// Disable RX and RXP
	dwmac5_disable_rx(priv->ioaddr, &config);
	ret = dwmac5_rxp_disable(priv->ioaddr);
	if (ret)
		pr_warn("STMMAC_RXP: Failed to disable RXP (ret=%d)\n", ret);
	else
		pr_info("STMMAC_RXP: RXP disabled\n");

	for (i = 0; i < count; i++) {
		u16 eth_type = eth_types[i];

		// Add rule for untagged frame
		stmmac_frp_set_ethertype_match(&instr, eth_type, false, STMMAC_AVB_CHANNEL);
		ret = dwmac5_frp_update_single_entry(priv->ioaddr, &instr, entry++);
		if (ret) {
			pr_err("STMMAC_RXP: Failed to add rule (eth=0x%04X, vlan=0) at entry %d\n", eth_type, entry - 1);
			dwmac5_restore_rx(priv->ioaddr, config);
			return ret;
		}

		// Add rule for VLAN-tagged frame
		stmmac_frp_set_ethertype_match(&instr, eth_type, true, STMMAC_AVB_CHANNEL);
		ret = dwmac5_frp_update_single_entry(priv->ioaddr, &instr, entry++);
		if (ret) {
			pr_err("STMMAC_RXP: Failed to add rule (eth=0x%04X, vlan=1) at entry %d\n", eth_type, entry - 1);
			dwmac5_restore_rx(priv->ioaddr, config);
			return ret;
		}

		pr_info("STMMAC_RXP: Added ethertype 0x%04X (vlan & non-vlan)\n", eth_type);
	}

	// Add final "accept all" rule
	stmmac_frp_accept_all(&instr);
	ret = dwmac5_frp_update_single_entry(priv->ioaddr, &instr, entry++);
	if (ret) {
		pr_err("STMMAC_RXP: Failed to add 'accept all' rule\n");
		dwmac5_restore_rx(priv->ioaddr, config);
		return ret;
	}
	pr_info("STMMAC_RXP: Added fallback accept-all rule at entry %d\n", entry - 1);

	// Update the number of active entries
	ret = dwmac5_frp_update_num_entries(priv->ioaddr, entry);
	if (ret) {
		pr_err("STMMAC_RXP: Failed to update entry count to %d\n", entry);
		dwmac5_restore_rx(priv->ioaddr, config);
		return ret;
	}

	dwmac5_rxp_enable(priv->ioaddr);
	pr_info("STMMAC_RXP: RXP enabled with %d total entries\n", entry);

	dwmac5_restore_rx(priv->ioaddr, config);
	return 0;
}

/**
 * stmmac_rxp_clear - Clear FRP RX parser rules
 * @priv: Driver private data
 */
int stmmac_rxp_clear(struct stmmac_priv *priv)
{
	int32_t config = 0;
	int ret;

	dwmac5_disable_rx(priv->ioaddr, &config);
	ret = dwmac5_rxp_disable(priv->ioaddr);
	if (ret)
		pr_warn("STMMAC_RXP: Failed to disable RXP (ret=%d)\n", ret);

	ret = dwmac5_frp_update_num_entries(priv->ioaddr, 0);
	if (ret)
		pr_err("STMMAC_RXP: Failed to clear FRP entries (ret=%d)\n", ret);
	else
		pr_info("STMMAC_RXP: Cleared FRP entries\n");

	dwmac5_restore_rx(priv->ioaddr, config);

	return ret;
}

/**
 * stmmac_avb_test_rxp - Sample test function to configure AVB FRP rules
 * @priv: Driver private data
 */
int stmmac_avb_test_rxp(struct stmmac_priv *priv)
{
	u16 avb_ethertypes[] = { 0x22F0, 0x8100 }; // AVTP, VLAN

	avb_test_running = true;

	stmmac_rxp_clear(priv);

	pr_info("stmmac: Installing FRP rules for AVB ethertypes\n");
	stmmac_rxp_setup(priv, avb_ethertypes, ARRAY_SIZE(avb_ethertypes));

	dwmac5_frp_dump_stats(priv->ioaddr);

	avb_test_running = false;
	return 0;
}
