#include "stmmac_genavb.h"
#include "stmmac_frp.h"
#include "stmmac.h"
#include "stmmac_dv_mac.h"

/* Internal state: single ethertype match rule */
static struct {
    u16 types[2];
    u16 count;
} frp_cfg;

static bool test_in_progress;

int stmmac_avb_test_rxp(struct stmmac_priv *priv)
{
    test_in_progress = true;

    /* Single Ethertype 0x22F0 match */
    frp_cfg.types[0] = htons(ETH_P_AVTP);
    frp_cfg.count = 1;

    return stmmac_rxp_setup(priv, frp_cfg.types, frp_cfg.count);
}

bool stmmac_avb_test_is_in_progress(void)
{
    return test_in_progress;
}

int stmmac_rxp_setup(struct stmmac_priv *priv, u16 eth_types[], u16 count)
{
    union frp_instruction instr;
    int i, ret;

    /* Disable RX parser before update */
    dwmac5_rxp_disable(priv->ioaddr);

    /* Program each Ethertype into FRP */
    for (i = 0; i < count; i++) {
        stmmac_frp_set_ethertype_match(&instr, ntohs(eth_types[i]), false,
                                      STMMAC_AVB_CHANNEL);
        ret = dwmac5_frp_update_single_entry(priv->ioaddr, &instr, i);
        if (ret)
            goto restore_rx;
    }

    dwmac5_frp_update_num_entries(priv->ioaddr, count);
    dwmac5_rxp_enable(priv->ioaddr);

    test_in_progress = false;
    return 0;

restore_rx:
    stmmac_rxp_clear(priv);
    return ret;
}

int stmmac_rxp_clear(struct stmmac_priv *priv)
{
    dwmac5_rxp_disable(priv->ioaddr);
    dwmac5_frp_update_num_entries(priv->ioaddr, 0);
    dwmac5_rxp_enable(priv->ioaddr);
    return 0;
}

int stmmac_avb_xmit_avb_tx_desc(struct stmmac_priv *priv, int queue,
        struct avb_tx_desc *avb_desc)
{
    /* Set the DMA channel on the TX descriptor to channel 4 */
    avb_desc->desc[0].ctrl |= STMMAC_AVB_CHANNEL << DMA_TX_CH_SHIFT;
    /* Push to DMA engine */
    return stmmac_dma_xmit(priv, queue, avb_desc->skb);
}
