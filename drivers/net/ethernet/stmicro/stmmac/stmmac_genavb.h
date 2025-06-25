#ifndef STMMAC_GENAVB_H
#define STMMAC_GENAVB_H

#include "stmmac.h"

#define STMMAC_AVB_CHANNEL 4     // Use DMA channel 4 for AVTP flows

/* self-tests */
int stmmac_avb_test_rxp(struct stmmac_priv *priv);
bool stmmac_avb_test_is_in_progress(void);

/* RX parser programming */
int stmmac_rxp_setup(struct stmmac_priv *priv, u16 eth_types[], u16 count);
int stmmac_rxp_clear(struct stmmac_priv *priv);

/* AVB transmit descriptor interface */
int stmmac_avb_xmit_avb_tx_desc(struct stmmac_priv *priv, int queue,
        struct avb_tx_desc *avb_desc);

#endif // STMMAC_GENAVB_H
