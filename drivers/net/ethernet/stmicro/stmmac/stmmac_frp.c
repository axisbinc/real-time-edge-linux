/*
 Copyright (c) 2025 AxisB Inc.
*/
#include "stmmac_frp.h"
#include <linux/io.h>
#include <linux/etherdevice.h>

// Utility: Bitmask for match_en field (field word selector)
#define FRP_FIELD_WORD(n)   (1U << (n))

/**
 * stmmac_frp_set_ethertype_match - Create an FRP rule to match EtherType
 * @instr:       Pointer to FRP instruction structure
 * @ethertype:   16-bit EtherType to match (e.g., 0x22F0 for AVTP)
 * @is_vlan:     Unused; reserved for future VLAN match support
 * @dma_channel: Target DMA channel number to route matching packets
 *
 * This sets up an FRP instruction to match packets with the specified
 * EtherType and route them to a specific DMA channel.
 */
void stmmac_frp_set_ethertype_match(union frp_instruction *instr,
				    u16 ethertype, bool is_vlan,
				    u8 dma_channel)
{
	memset(instr, 0, sizeof(*instr));

	// EtherType is located in the second 4-byte word (offset 1)
	instr->fields.match_data = cpu_to_be32((u32)ethertype << 16);
	instr->fields.match_en = cpu_to_be32(FRP_FIELD_WORD(1)); // Match word 1
	instr->fields.af = 1;           // Accept frame if matched
	instr->fields.rf = 0;           // Do not reject
	instr->fields.im = 0;           // Normal match (not inverse)
	instr->fields.nc = 0;           // No next instruction (end)
	instr->fields.frame_offset = 1; // Offset in 4-byte units
	instr->fields.ok_index = 0xFF;  // No next instruction
	instr->fields.dma_ch_no = dma_channel;
}

/**
 * stmmac_frp_accept_all - Create a default rule to accept all packets
 * @instr: Pointer to FRP instruction structure
 *
 * This rule allows all unmatched packets to be accepted and routed to
 * the default AVB DMA channel.
 */
void stmmac_frp_accept_all(union frp_instruction *instr)
{
	memset(instr, 0, sizeof(*instr));

	instr->fields.af = 1;                 // Accept all frames
	instr->fields.rf = 0;                 // Do not reject
	instr->fields.nc = 0;                 // No next instruction
	instr->fields.ok_index = 0xFF;        // No chaining
	instr->fields.dma_ch_no = STMMAC_AVB_CHANNEL; // Route to AVB channel (e.g., 4)
}
