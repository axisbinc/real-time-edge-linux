#include "stmmac_rxc.h"
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
	int ret, i;

	if (count > 64)
		return -EINVAL;

	ret = dwmac5_rxp_disable(priv->ioaddr);
	if (ret)
		return ret;

	ret = dwmac5_frp_update_num_entries(priv->ioaddr, count);
	if (ret)
		return ret;

	for (i = 0; i < count; i++) {
		stmmac_frp_set_ethertype_match(&instr, eth_types[i], false, STMMAC_AVB_CHANNEL);
		ret = dwmac5_frp_update_single_entry(priv->ioaddr, &instr, i);
		if (ret)
			return ret;
	}

	dwmac5_rxp_enable(priv->ioaddr);
	return 0;
}

/**
 * stmmac_rxp_clear - Clear FRP RX parser rules
 * @priv: Driver private data
 */
int stmmac_rxp_clear(struct stmmac_priv *priv)
{
	int ret;

	ret = dwmac5_rxp_disable(priv->ioaddr);
	if (ret)
		return ret;

	ret = dwmac5_frp_update_num_entries(priv->ioaddr, 0);
	if (ret)
		return ret;

	dwmac5_rxp_enable(priv->ioaddr);
	return 0;
}

/**
 * stmmac_avb_xmit_avb_tx_desc - Send AVB packet using channel 4
 * @priv: Driver private data
 * @queue: Not used (DMA queue 4 is hardcoded in FRP)
 * @avb_desc: Data and size to send
 */
int stmmac_avb_xmit_avb_tx_desc(struct stmmac_priv *priv, int queue,
				struct avb_tx_desc *avb_desc)
{
	struct sk_buff *skb;

	skb = netdev_alloc_skb_ip_align(priv->dev, avb_desc->len);
	if (!skb)
		return -ENOMEM;

	memcpy(skb_put(skb, avb_desc->len), avb_desc->data, avb_desc->len);

	skb->dev = priv->dev;
	skb->priority = 7; // Use high priority
	skb->queue_mapping = STMMAC_AVB_CHANNEL; // Ensure it maps to DMA channel 4

	// Use the registered network transmit callback
	return priv->dev->netdev_ops->ndo_start_xmit(skb, priv->dev);
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
