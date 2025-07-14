/*
 Copyright (c) 2025 AxisB Inc.
*/
#ifndef STMMAC_FRP_H
#define STMMAC_FRP_H
#include <linux/types.h>

#define STMMAC_AVB_CHANNEL 4

/**
 * Flexible Receive Parser (FRP) Instruction
 * Each FRP instruction is 128 bits (4 x 32-bit words).
 */
union frp_instruction {
    struct {
		u32 match_data;   // Data to match (e.g., EtherType)
		u32 match_en;     // Bit mask indicating active match bits
		u8 af : 1;        // Accept Frame
		u8 rf : 1;        // Reject Frame
		u8 im : 1;        // Inverse Match
		u8 nc : 1;        // Next Instruction Control
		u8 res1 : 4;      // Reserved
		u8 frame_offset;  // Offset into frame (in 4-byte words)
		u8 ok_index;      // Next instruction index or 0xFF
		u8 dma_ch_no;     // DMA channel to deliver matched frame
		u32 res2;         // Reserved
	} fields;
    u32 as_array[4]; // Hardware view: 4x32-bit words
} __attribute__((packed));

/* Instruction setup helpers */
void stmmac_frp_set_ethertype_match(union frp_instruction *instr,
				    u16 ethertype, bool is_vlan,
				    u8 dma_channel);
void stmmac_frp_accept_all(union frp_instruction *instr);

/* RX Parser Management */
int dwmac5_rxp_disable(void __iomem *ioaddr);
void dwmac5_rxp_enable(void __iomem *ioaddr);
int dwmac5_frp_update_num_entries(void __iomem *ioaddr, u32 num_entries);
int dwmac5_frp_update_single_entry(void __iomem *ioaddr,
				   union frp_instruction *instr, int pos);
void dwmac5_frp_dump_stats(void __iomem *ioaddr);
int dwmac5_restore_rx(void __iomem *ioaddr, u32 config);
int dwmac5_disable_rx(void __iomem *ioaddr, u32 *config);

#endif // STMMAC_FRP_H
