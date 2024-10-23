/**
 * 
 */

#ifndef __STMMAC_FRP_H__
#define __STMMAC_FRP_H__

#include <linux/types.h>

union frp_instruction {
    struct {
        uint32_t match_data;   // The 4-byte data to match (Ethertype)
        uint32_t match_en;     // Match enable mask
        uint8_t af : 1;        // Accept Frame
        uint8_t rf : 1;        // Reject Frame
        uint8_t im : 1;        // Inverse Match
        uint8_t nc : 1;        // Next Instruction Control
        uint8_t res1 : 4;      // Reserved
        uint8_t frame_offset;  // Frame offset in 4-byte units
        uint8_t ok_index;      // OK Index (next instruction)
        uint8_t dma_ch_no;     // DMA Channel Number
        uint32_t res2;         // Reserved
    } fields; // Named section for accessing individual fields

    uint32_t as_array[4]; // View the data as an array of 4x 4-byte elements
} __attribute__((packed));

void stmmac_frp_set_ethertype_match(union frp_instruction *instr,
        uint16_t ethertype, uint8_t dma_channel);
void stmmac_frp_accept_all(union frp_instruction *instr);

int dwmac5_rxp_disable(void __iomem *ioaddr);
void dwmac5_rxp_enable(void __iomem *ioaddr);
int dwmac5_frp_update_num_entries(void __iomem *ioaddr, uint32_t num_entries);
int dwmac5_frp_update_single_entry(void __iomem *ioaddr, 
        union frp_instruction *instr, int pos);

void dwmac5_frp_dump_stats(void __iomem *ioaddr);

int dwmac5_restore_rx(void __iomem *ioaddr, uint32_t config);
int dwmac5_disable_rx(void __iomem *ioaddr, uint32_t *config);

#endif //__STMMAC_FRP_H__