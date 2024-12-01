#include "stmmac.h"
#include "stmmac_genavb.h"
#include "stmmac_frp.h"

int stmmac_rxp_setup(struct stmmac_priv *priv, u16 eth_types[], u16 count)
{
    union frp_instruction instr = { };
    int ret = 0;
    uint32_t config = 0;
    int entry_index = 0;

    // Disable the RXP for configuration
    dwmac5_disable_rx(priv->hw->pcsr, &config);
    dwmac5_rxp_disable(priv->hw->pcsr);

    // Iterate over each Ethertype in the array
    for (int i = 0; i < count; i++) {
        u16 eth_type = eth_types[i];

        // Set up FRP instruction for standard Ethernet header (non-VLAN)
        stmmac_frp_set_ethertype_match(&instr, eth_type, false, STMMAC_AVB_CHANNEL);
        ret |= dwmac5_frp_update_single_entry(priv->hw->pcsr, &instr,
                entry_index++);
        
        // Set up FRP instruction for VLAN-tagged Ethernet header
        stmmac_frp_set_ethertype_match(&instr, eth_type, true, STMMAC_AVB_CHANNEL);
        ret |= dwmac5_frp_update_single_entry(priv->hw->pcsr, &instr,
                entry_index++);
    }

    // Add a final entry to accept all remaining packets
    stmmac_frp_accept_all(&instr);
    ret |= dwmac5_frp_update_single_entry(priv->hw->pcsr, &instr, entry_index);

    // Update the number of FRP entries
    dwmac5_frp_update_num_entries(priv->hw->pcsr, entry_index + 1);

    // Re-enable RXP
    dwmac5_rxp_enable(priv->hw->pcsr);
    dwmac5_restore_rx(priv->hw->pcsr, config);

    return ret;
}

int stmmac_rxp_clear(struct stmmac_priv *priv)
{
    uint32_t config = 0;
    dwmac5_disable_rx(priv->hw->pcsr, &config);

    dwmac5_rxp_disable(priv->hw->pcsr);
    dwmac5_frp_update_num_entries(priv->hw->pcsr, 0);
    //dwmac5_rxp_enable(priv->hw->pcsr);
    dwmac5_restore_rx(priv->hw->pcsr, config);
    return 0;
}
