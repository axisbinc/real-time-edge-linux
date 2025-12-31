/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __STMMAC_GENAVB_PRIV_H__
#define __STMMAC_GENAVB_PRIV_H__

#include "stmmac.h"
#include <linux/stmmac_genavb.h>

// todo: priv->plat->avb_dma_cfg->avb_dma_chan
#define STMMAC_AVB_CHANNEL 4

// selftests
int stmmac_avb_test_rxp(struct stmmac_priv *priv);
bool stmmac_avb_test_is_in_progress(void);

// program rx parser in to the mac to divert the packets of the eth_types to the avb channel
int stmmac_rxp_setup(struct stmmac_priv *priv, u16 eth_types[], u16 count);
// clear the rx parser configuration
int stmmac_rxp_clear(struct stmmac_priv *priv);

// transmit packet on the given queue identifier
int stmmac_avb_xmit_avb_tx_desc(struct stmmac_priv *priv, int queue,
        struct avb_tx_desc *avb_desc);

int stmmac_enet_start_xmit_avb(void *data, struct avb_tx_desc *avb_buff);

#endif /* __STMMAC_GENAVB_PRIV_H__ */