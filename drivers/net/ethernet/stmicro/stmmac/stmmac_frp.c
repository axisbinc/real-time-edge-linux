#include "stmmac_frp.h"
#include <asm/byteorder.h>

// Function to set up an Ethertype match instruction for the FRP
void stmmac_frp_set_ethertype_match(union frp_instruction *instr,
        uint16_t ethertype, uint8_t dma_channel)
{
    if (!instr) {
        return;  // Ensure the provided pointer is valid
    }

    // Access the fields using 'fields' structure inside the union

    // Set the match data to the Ethertype (Ethernet Type)
    instr->fields.match_data = htons(ethertype);  // Ethertype is 2 bytes, so we use the lower 16 bits of match_data

    // Enable all 16 bits of the Ethertype for comparison (matching all bits)
    instr->fields.match_en = 0xFFFF;  // Enable mask for the 16 lower bits

    // Set the frame offset to 12 bytes (where Ethertype is located in the Ethernet frame)
    instr->fields.frame_offset = 3;  // 3 indicates byte offset 12 (in 4-byte chunks)

    // Accept the frame if the Ethertype matches
    instr->fields.af = 1;  // Accept frame

    // Set the DMA channel where matching packets should be sent
    instr->fields.dma_ch_no = (1 << dma_channel);  // Directly assign DMA channel

    // Zero out the rest of the fields
    instr->fields.rf = 0;  // Do not reject
    instr->fields.im = 0;  // Do not inverse the match
    instr->fields.nc = 0;  // No next instruction control
    instr->fields.ok_index = 0;  // No next instruction
    instr->fields.res1 = 0;  // Reserved, set to 0
    instr->fields.res2 = 0;  // Reserved, set to 0
}

// function to accept all packets
void stmmac_frp_accept_all(union frp_instruction *instr)
{
    if (!instr) {
        return;  // Ensure the provided pointer is valid
    }

    // Access the fields using 'fields' structure inside the union

    // Set the match data to 0 (match all packets)
    instr->fields.match_data = 0;  // Match all packets

    // Enable all 32 bits for comparison (matching all bits)
    instr->fields.match_en = 0x0;  // Enable mask for all 32 bits

    // Set the frame offset to 0 bytes (start of the Ethernet frame)
    instr->fields.frame_offset = 0;  // 0 indicates byte offset 0 (in 4-byte chunks)

    // Accept the frame
    instr->fields.af = 1;  // Accept frame

    // Set the DMA channel where matching packets should be sent
    instr->fields.dma_ch_no = 0; 

    // Zero out the rest of the fields
    instr->fields.rf = 0;  // Do not reject
    instr->fields.im = 0;  // Do not inverse the match
    instr->fields.nc = 0;  // No next instruction control
    instr->fields.ok_index = 0;  // No next instruction
    instr->fields.res1 = 0;  // Reserved, set to 0
    instr->fields.res2 = 0;  // Reserved, set to 0
}