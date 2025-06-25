#include "stmmac_frp.h"
#include <linux/io.h>
#include <linux/etherdevice.h>
#include "dma_engine.h" // Adjust include as necessary

#define FRP_FIELD_WORD(n)   (1 << (n)) // placeholder utility

void stmmac_frp_set_ethertype_match(union frp_instruction *instr,
        uint16_t ethertype, bool is_vlan, uint8_t dma_channel)
{
    memset(instr, 0, sizeof(*instr));
    instr->fields.match_data = __cpu_to_be32(ethertype << 16);
    instr->fields.match_en = __cpu_to_be32(FRP_FIELD_WORD(1)); // match EtherType field
    instr->fields.af = 1;
    instr->fields.rf = 0;
    instr->fields.im = 0;
    instr->fields.nc = 0;
    instr->fields.frame_offset = 1;           // EtherType offset
    instr->fields.ok_index = 0xFF;            // no chaining
    instr->fields.dma_ch_no = dma_channel;    // DMA channel assignment
}

void stmmac_frp_accept_all(union frp_instruction *instr)
{
    memset(instr, 0, sizeof(*instr));
    instr->fields.af = 1;
    instr->fields.rf = 0;
    instr->fields.nc = 0;
    instr->fields.ok_index = 0xFF;
    instr->fields.dma_ch_no = STMMAC_AVB_CHANNEL;
}
