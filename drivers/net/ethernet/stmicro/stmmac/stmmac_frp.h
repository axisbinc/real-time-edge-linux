#ifndef STMMAC_FRP_H
#define STMMAC_FRP_H
#include <linux/types.h>

union frp_instruction {
    struct {
        uint32_t match_data;   // The 4-byte data to match (EtherType)
        uint32_t match_en;     // Match enable mask
        uint8_t af : 1;        // Accept Frame
        uint8_t rf : 1;        // Reject Frame
        uint8_t im : 1;        // Inverse Match
        uint8_t nc : 1;        // Next Instruction Control
        uint8_t res1 : 4;
        uint8_t frame_offset;  // Frame offset in 4-byte words
        uint8_t ok_index;
        uint8_t dma_ch_no;     // DMA Channel Number
        uint32_t res2;
    } fields;
    uint32_t as_array[4];
} __attribute__((packed));

void stmmac_frp_set_ethertype_match(union frp_instruction *instr,
        uint16_t ethertype, bool is_vlan, uint8_t dma_channel);
void stmmac_frp_accept_all(union frp_instruction *instr);
/* FRP ops from dwmac5 */
int dwmac5_rxp_disable(void __iomem *ioaddr);
void dwmac5_rxp_enable(void __iomem *ioaddr);
int dwmac5_frp_update_num_entries(void __iomem *ioaddr, uint32_t num_entries);
int dwmac5_frp_update_single_entry(void __iomem *ioaddr,
        union frp_instruction *instr, int pos);
void dwmac5_frp_dump_stats(void __iomem *ioaddr);
int dwmac5_restore_rx(void __iomem *ioaddr, uint32_t config);
int dwmac5_disable_rx(void __iomem *ioaddr, uint32_t *config);

#endif // STMMAC_FRP_H
