// SPDX-License-Identifier: (GPL-2.0 OR MIT)
// Copyright (c) 2017 Synopsys, Inc. and/or its affiliates.
// stmmac Support for 5.xx Ethernet QoS cores

#include <linux/bitops.h>
#include <linux/iopoll.h>
#include <linux/types.h>
#include <linux/byteorder/generic.h>
#include "common.h"
#include "dwmac4.h"
#include "dwmac5.h"
#include "stmmac.h"
#include "stmmac_ptp.h"
#include "stmmac_frp.h"

struct dwmac5_error_desc {
	bool valid;
	const char *desc;
	const char *detailed_desc;
};

#define STAT_OFF(field)		offsetof(struct stmmac_safety_stats, field)

static void dwmac5_log_error(struct net_device *ndev, u32 value, bool corr,
		const char *module_name, const struct dwmac5_error_desc *desc,
		unsigned long field_offset, struct stmmac_safety_stats *stats)
{
	unsigned long loc, mask;
	u8 *bptr = (u8 *)stats;
	unsigned long *ptr;

	ptr = (unsigned long *)(bptr + field_offset);

	mask = value;
	for_each_set_bit(loc, &mask, 32) {
		netdev_err(ndev, "Found %s error in %s: '%s: %s'\n", corr ?
				"correctable" : "uncorrectable", module_name,
				desc[loc].desc, desc[loc].detailed_desc);

		/* Update counters */
		ptr[loc]++;
	}
}

static const struct dwmac5_error_desc dwmac5_mac_errors[32]= {
	{ true, "ATPES", "Application Transmit Interface Parity Check Error" },
	{ true, "TPES", "TSO Data Path Parity Check Error" },
	{ true, "RDPES", "Read Descriptor Parity Check Error" },
	{ true, "MPES", "MTL Data Path Parity Check Error" },
	{ true, "MTSPES", "MTL TX Status Data Path Parity Check Error" },
	{ true, "ARPES", "Application Receive Interface Data Path Parity Check Error" },
	{ true, "CWPES", "CSR Write Data Path Parity Check Error" },
	{ true, "ASRPES", "AXI Slave Read Data Path Parity Check Error" },
	{ true, "TTES", "TX FSM Timeout Error" },
	{ true, "RTES", "RX FSM Timeout Error" },
	{ true, "CTES", "CSR FSM Timeout Error" },
	{ true, "ATES", "APP FSM Timeout Error" },
	{ true, "PTES", "PTP FSM Timeout Error" },
	{ true, "T125ES", "TX125 FSM Timeout Error" },
	{ true, "R125ES", "RX125 FSM Timeout Error" },
	{ true, "RVCTES", "REV MDC FSM Timeout Error" },
	{ true, "MSTTES", "Master Read/Write Timeout Error" },
	{ true, "SLVTES", "Slave Read/Write Timeout Error" },
	{ true, "ATITES", "Application Timeout on ATI Interface Error" },
	{ true, "ARITES", "Application Timeout on ARI Interface Error" },
	{ false, "UNKNOWN", "Unknown Error" }, /* 20 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 21 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 22 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 23 */
	{ true, "FSMPES", "FSM State Parity Error" },
	{ false, "UNKNOWN", "Unknown Error" }, /* 25 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 26 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 27 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 28 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 29 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 30 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 31 */
};

static void dwmac5_handle_mac_err(struct net_device *ndev,
		void __iomem *ioaddr, bool correctable,
		struct stmmac_safety_stats *stats)
{
	u32 value;

	value = readl(ioaddr + MAC_DPP_FSM_INT_STATUS);
	writel(value, ioaddr + MAC_DPP_FSM_INT_STATUS);

	dwmac5_log_error(ndev, value, correctable, "MAC", dwmac5_mac_errors,
			STAT_OFF(mac_errors), stats);
}

static const struct dwmac5_error_desc dwmac5_mtl_errors[32]= {
	{ true, "TXCES", "MTL TX Memory Error" },
	{ true, "TXAMS", "MTL TX Memory Address Mismatch Error" },
	{ true, "TXUES", "MTL TX Memory Error" },
	{ false, "UNKNOWN", "Unknown Error" }, /* 3 */
	{ true, "RXCES", "MTL RX Memory Error" },
	{ true, "RXAMS", "MTL RX Memory Address Mismatch Error" },
	{ true, "RXUES", "MTL RX Memory Error" },
	{ false, "UNKNOWN", "Unknown Error" }, /* 7 */
	{ true, "ECES", "MTL EST Memory Error" },
	{ true, "EAMS", "MTL EST Memory Address Mismatch Error" },
	{ true, "EUES", "MTL EST Memory Error" },
	{ false, "UNKNOWN", "Unknown Error" }, /* 11 */
	{ true, "RPCES", "MTL RX Parser Memory Error" },
	{ true, "RPAMS", "MTL RX Parser Memory Address Mismatch Error" },
	{ true, "RPUES", "MTL RX Parser Memory Error" },
	{ false, "UNKNOWN", "Unknown Error" }, /* 15 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 16 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 17 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 18 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 19 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 20 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 21 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 22 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 23 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 24 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 25 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 26 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 27 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 28 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 29 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 30 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 31 */
};

static void dwmac5_handle_mtl_err(struct net_device *ndev,
		void __iomem *ioaddr, bool correctable,
		struct stmmac_safety_stats *stats)
{
	u32 value;

	value = readl(ioaddr + MTL_ECC_INT_STATUS);
	writel(value, ioaddr + MTL_ECC_INT_STATUS);

	dwmac5_log_error(ndev, value, correctable, "MTL", dwmac5_mtl_errors,
			STAT_OFF(mtl_errors), stats);
}

static const struct dwmac5_error_desc dwmac5_dma_errors[32]= {
	{ true, "TCES", "DMA TSO Memory Error" },
	{ true, "TAMS", "DMA TSO Memory Address Mismatch Error" },
	{ true, "TUES", "DMA TSO Memory Error" },
	{ false, "UNKNOWN", "Unknown Error" }, /* 3 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 4 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 5 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 6 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 7 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 8 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 9 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 10 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 11 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 12 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 13 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 14 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 15 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 16 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 17 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 18 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 19 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 20 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 21 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 22 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 23 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 24 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 25 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 26 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 27 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 28 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 29 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 30 */
	{ false, "UNKNOWN", "Unknown Error" }, /* 31 */
};

static void dwmac5_handle_dma_err(struct net_device *ndev,
		void __iomem *ioaddr, bool correctable,
		struct stmmac_safety_stats *stats)
{
	u32 value;

	value = readl(ioaddr + DMA_ECC_INT_STATUS);
	writel(value, ioaddr + DMA_ECC_INT_STATUS);

	dwmac5_log_error(ndev, value, correctable, "DMA", dwmac5_dma_errors,
			STAT_OFF(dma_errors), stats);
}

int dwmac5_safety_feat_config(void __iomem *ioaddr, unsigned int asp,
			      struct stmmac_safety_feature_cfg *safety_feat_cfg)
{
	struct stmmac_safety_feature_cfg all_safety_feats = {
		.tsoee = 1,
		.mrxpee = 1,
		.mestee = 1,
		.mrxee = 1,
		.mtxee = 1,
		.epsi = 1,
		.edpp = 1,
		.prtyen = 1,
		.tmouten = 1,
	};
	u32 value;

	if (!asp)
		return -EINVAL;

	if (!safety_feat_cfg)
		safety_feat_cfg = &all_safety_feats;

	/* 1. Enable Safety Features */
	value = readl(ioaddr + MTL_ECC_CONTROL);
	value |= MEEAO; /* MTL ECC Error Addr Status Override */
	if (safety_feat_cfg->tsoee)
		value |= TSOEE; /* TSO ECC */
	if (safety_feat_cfg->mrxpee)
		value |= MRXPEE; /* MTL RX Parser ECC */
	if (safety_feat_cfg->mestee)
		value |= MESTEE; /* MTL EST ECC */
	if (safety_feat_cfg->mrxee)
		value |= MRXEE; /* MTL RX FIFO ECC */
	if (safety_feat_cfg->mtxee)
		value |= MTXEE; /* MTL TX FIFO ECC */
	writel(value, ioaddr + MTL_ECC_CONTROL);

	/* 2. Enable MTL Safety Interrupts */
	value = readl(ioaddr + MTL_ECC_INT_ENABLE);
	value |= RPCEIE; /* RX Parser Memory Correctable Error */
	value |= ECEIE; /* EST Memory Correctable Error */
	value |= RXCEIE; /* RX Memory Correctable Error */
	value |= TXCEIE; /* TX Memory Correctable Error */
	writel(value, ioaddr + MTL_ECC_INT_ENABLE);

	/* 3. Enable DMA Safety Interrupts */
	value = readl(ioaddr + DMA_ECC_INT_ENABLE);
	value |= TCEIE; /* TSO Memory Correctable Error */
	writel(value, ioaddr + DMA_ECC_INT_ENABLE);

	/* Only ECC Protection for External Memory feature is selected */
	if (asp <= 0x1)
		return 0;

	/* 5. Enable Parity and Timeout for FSM */
	value = readl(ioaddr + MAC_FSM_CONTROL);
	if (safety_feat_cfg->prtyen)
		value |= PRTYEN; /* FSM Parity Feature */
	if (safety_feat_cfg->tmouten)
		value |= TMOUTEN; /* FSM Timeout Feature */
	writel(value, ioaddr + MAC_FSM_CONTROL);

	/* 4. Enable Data Parity Protection */
	value = readl(ioaddr + MTL_DPP_CONTROL);
	if (safety_feat_cfg->edpp)
		value |= EDPP;
	writel(value, ioaddr + MTL_DPP_CONTROL);

	/*
	 * All the Automotive Safety features are selected without the "Parity
	 * Port Enable for external interface" feature.
	 */
	if (asp <= 0x2)
		return 0;

	if (safety_feat_cfg->epsi)
		value |= EPSI;
	writel(value, ioaddr + MTL_DPP_CONTROL);
	return 0;
}

int dwmac5_safety_feat_irq_status(struct net_device *ndev,
		void __iomem *ioaddr, unsigned int asp,
		struct stmmac_safety_stats *stats)
{
	bool err, corr;
	u32 mtl, dma;
	int ret = 0;

	if (!asp)
		return -EINVAL;

	mtl = readl(ioaddr + MTL_SAFETY_INT_STATUS);
	dma = readl(ioaddr + DMA_SAFETY_INT_STATUS);

	err = (mtl & MCSIS) || (dma & MCSIS);
	corr = false;
	if (err) {
		dwmac5_handle_mac_err(ndev, ioaddr, corr, stats);
		ret |= !corr;
	}

	err = (mtl & (MEUIS | MECIS)) || (dma & (MSUIS | MSCIS));
	corr = (mtl & MECIS) || (dma & MSCIS);
	if (err) {
		dwmac5_handle_mtl_err(ndev, ioaddr, corr, stats);
		ret |= !corr;
	}

	err = dma & (DEUIS | DECIS);
	corr = dma & DECIS;
	if (err) {
		dwmac5_handle_dma_err(ndev, ioaddr, corr, stats);
		ret |= !corr;
	}

	return ret;
}

static const struct dwmac5_error {
	const struct dwmac5_error_desc *desc;
} dwmac5_all_errors[] = {
	{ dwmac5_mac_errors },
	{ dwmac5_mtl_errors },
	{ dwmac5_dma_errors },
};

int dwmac5_safety_feat_dump(struct stmmac_safety_stats *stats,
			int index, unsigned long *count, const char **desc)
{
	int module = index / 32, offset = index % 32;
	unsigned long *ptr = (unsigned long *)stats;

	if (module >= ARRAY_SIZE(dwmac5_all_errors))
		return -EINVAL;
	if (!dwmac5_all_errors[module].desc[offset].valid)
		return -EINVAL;
	if (count)
		*count = *(ptr + index);
	if (desc)
		*desc = dwmac5_all_errors[module].desc[offset].desc;
	return 0;
}

int dwmac5_disable_rx(void __iomem *ioaddr, uint32_t *config)
{
	u32 old_val, val;

	/* Force disable RX */
	*config = old_val = readl(ioaddr + GMAC_CONFIG);
	val = old_val & ~GMAC_CONFIG_RE;
	writel(val, ioaddr + GMAC_CONFIG);
    return 0;
}

int dwmac5_restore_rx(void __iomem *ioaddr, uint32_t config)
{
	writel(config, ioaddr + GMAC_CONFIG);
    return 0;
}

int dwmac5_rxp_disable(void __iomem *ioaddr)
{
	u32 val;

	val = readl(ioaddr + MTL_OPERATION_MODE);
	val &= ~MTL_FRPE;
	writel(val, ioaddr + MTL_OPERATION_MODE);

	return readl_poll_timeout(ioaddr + MTL_RXP_CONTROL_STATUS, val,
			val & RXPI, 1, 10000);
}

void dwmac5_rxp_enable(void __iomem *ioaddr)
{
	u32 val;

	val = readl(ioaddr + MTL_OPERATION_MODE);
	val |= MTL_FRPE;
	writel(val, ioaddr + MTL_OPERATION_MODE);
}

#include "dwmac4_dma.h"
#define DWC_EQOS_NUM_DMA_RX_CH 5
#define DWC_EQOS_NUM_DMA_TX_CH 5

static void _dwmac4_dump_dma_regs(void __iomem *ioaddr, u32 channel)
{
    pr_info("    DMA_CHAN_CONTROL: 0x%x\n",
        readl(ioaddr + DMA_CHAN_CONTROL(channel)));
    pr_info("    DMA_CHAN_TX_CONTROL: 0x%x\n",
        readl(ioaddr + DMA_CHAN_TX_CONTROL(channel)));
    pr_info("    DMA_CHAN_RX_CONTROL: 0x%x\n",
        readl(ioaddr + DMA_CHAN_RX_CONTROL(channel)));
    pr_info("    DMA_CHAN_TX_BASE_ADDR: 0x%x\n",
        readl(ioaddr + DMA_CHAN_TX_BASE_ADDR(channel)));
    pr_info("    DMA_CHAN_RX_BASE_ADDR: 0x%x\n",
        readl(ioaddr + DMA_CHAN_RX_BASE_ADDR(channel)));
    pr_info("    DMA_CHAN_TX_END_ADDR: 0x%x\n",
        readl(ioaddr + DMA_CHAN_TX_END_ADDR(channel)));
    pr_info("    DMA_CHAN_RX_END_ADDR: 0x%x\n",
        readl(ioaddr + DMA_CHAN_RX_END_ADDR(channel)));
    pr_info("    DMA_CHAN_TX_RING_LEN: 0x%x\n",
        readl(ioaddr + DMA_CHAN_TX_RING_LEN(channel)));
    pr_info("    DMA_CHAN_RX_RING_LEN: 0x%x\n",
        readl(ioaddr + DMA_CHAN_RX_RING_LEN(channel)));
    pr_info("    DMA_CHAN_INTR_ENA: 0x%x\n",
        readl(ioaddr + DMA_CHAN_INTR_ENA(channel)));
    pr_info("    DMA_CHAN_RX_WATCHDOG: 0x%x\n",
        readl(ioaddr + DMA_CHAN_RX_WATCHDOG(channel)));
    pr_info("    DMA_CHAN_SLOT_CTRL_STATUS: 0x%x\n",
        readl(ioaddr + DMA_CHAN_SLOT_CTRL_STATUS(channel)));
    pr_info("    DMA_CHAN_CUR_TX_DESC: 0x%x\n",
        readl(ioaddr + DMA_CHAN_CUR_TX_DESC(channel)));
    pr_info("    DMA_CHAN_CUR_RX_DESC: 0x%x\n",
        readl(ioaddr + DMA_CHAN_CUR_RX_DESC(channel)));
    pr_info("    DMA_CHAN_CUR_TX_BUF_ADDR: 0x%x\n",
        readl(ioaddr + DMA_CHAN_CUR_TX_BUF_ADDR(channel)));
    pr_info("    DMA_CHAN_CUR_RX_BUF_ADDR: 0x%x\n",
        readl(ioaddr + DMA_CHAN_CUR_RX_BUF_ADDR(channel)));
    pr_info("    DMA_CHAN_STATUS: 0x%x\n",
        readl(ioaddr + DMA_CHAN_STATUS(channel)));
}

void dwmac5_frp_get_stats(void __iomem *ioaddr, uint8_t dma_channel,
        uint32_t *accept_count)
{
    u32 val;
    if (dma_channel >= DWC_EQOS_NUM_DMA_RX_CH) {
        pr_err("Invalid DMA channel: %d\n", dma_channel);
        return;
    }

    val = readl(ioaddr + DMA_CHAN_RXP_ACCEPT_CNT(dma_channel));
    if (accept_count)
        *accept_count = val;
}

static int dwmac5_rxp_iacc_read_word(void __iomem *base, u16 addr, u32 *data)
{
	u32 ctrl;
	int ret;

	if (!data)
		return -EINVAL;

	/* Wait for ready */
	ret = readl_poll_timeout(base + MTL_RXP_IACC_CTRL_STATUS,
				 ctrl, !(ctrl & STARTBUSY), 1, 10000);
	if (ret)
		return ret;

	/* Program address (read op => WRRDN cleared) */
	ctrl = addr & ADDR;
	writel(ctrl, base + MTL_RXP_IACC_CTRL_STATUS);

	/* Start read */
	ctrl |= STARTBUSY;
	writel(ctrl, base + MTL_RXP_IACC_CTRL_STATUS);

	/* Wait for done */
	ret = readl_poll_timeout(base + MTL_RXP_IACC_CTRL_STATUS,
				 ctrl, !(ctrl & STARTBUSY), 1, 10000);
	if (ret)
		return ret;

	*data = readl(base + MTL_RXP_IACC_DATA);
	return 0;
}

void dwmac5_frp_dump_rxp(void __iomem *ioaddr)
{
	union frp_instruction instr;
	u32 ctrl;
	u32 nve;
	u32 npe;
	int pos;

	ctrl = readl(ioaddr + MTL_RXP_CONTROL_STATUS);
	nve = ctrl & NVE;
	npe = (ctrl & NPE) >> 16;

	pr_info("FRP table: NVE=%u, NPE=0x%x, CTRL=0x%x\n", nve, npe, ctrl);

	/* Keep output bounded in case something goes wrong */
	if (nve > 64) {
		pr_info("FRP table: limiting dump to first 64 entries (reported NVE=%u)\n", nve);
		nve = 64;
	}

	for (pos = 0; pos < (int)nve; pos++) {
		int ret;
		u32 w0 = 0, w1 = 0, w2 = 0, w3 = 0;
		u16 ethertype;

		ret = dwmac5_rxp_iacc_read_word(ioaddr, (u16)(pos * 4 + 0), &w0);
		ret |= dwmac5_rxp_iacc_read_word(ioaddr, (u16)(pos * 4 + 1), &w1);
		ret |= dwmac5_rxp_iacc_read_word(ioaddr, (u16)(pos * 4 + 2), &w2);
		ret |= dwmac5_rxp_iacc_read_word(ioaddr, (u16)(pos * 4 + 3), &w3);
		if (ret) {
			pr_info("FRP[%d]: read failed (%d)\n", pos, ret);
			continue;
		}

		instr.as_array[0] = w0;
		instr.as_array[1] = w1;
		instr.as_array[2] = w2;
		instr.as_array[3] = w3;

		ethertype = be16_to_cpu((__force __be16)(instr.fields.match_data & 0xffff));

		pr_info("FRP[%02d] raw: %08x %08x %08x %08x\n", pos, w0, w1, w2, w3);
		pr_info("FRP[%02d] dec: af=%u rf=%u im=%u nc=%u off=%u ok=%u dma_mask=0x%02x match_en=0x%08x eth=0x%04x\n",
			pos,
			instr.fields.af,
			instr.fields.rf,
			instr.fields.im,
			instr.fields.nc,
			instr.fields.frame_offset,
			instr.fields.ok_index,
			instr.fields.dma_ch_no,
			instr.fields.match_en,
			ethertype);
	}
}

void dwmac5_frp_dump_stats(void __iomem *ioaddr)
{
    u32 val;

	val = readl(ioaddr + GMAC_CONFIG);
    pr_info("GMAC_CONFIG: 0x%x\n", val);

    val = readl(ioaddr + MTL_RXQ_DMA_MAP0);
    pr_info("MTL_RXQ_DMA_MAP0: 0x%x\n", val);
    val = readl(ioaddr + MTL_RXQ_DMA_MAP1);
    pr_info("MTL_RXQ_DMA_MAP1: 0x%x\n", val);

    val = readl(ioaddr + MTL_RXP_CONTROL_STATUS);
    pr_info("MTL_RXP_CONTROL_STATUS: 0x%x\n", val);
    val = readl(ioaddr + MTL_OPERATION_MODE);
    pr_info("MTL_OPERATION_MODE: 0x%x\n", val);

    val = readl(ioaddr + MTL_RXP_DROP_CNT);
    pr_info("MTL_RXP_DROP_CNT: 0x%x\n", val);
    val = readl(ioaddr + MTL_RXP_ERROR_CNT);
    pr_info("MTL_RXP_ERROR_CNT: 0x%x\n", val);

    val = readl(ioaddr + DMA_CH0_RXP_ACCEPT_CNT);
    pr_info("DMA_CH0_RXP_ACCEPT_CNT: 0x%x\n", val);
    val = readl(ioaddr + DMA_CH1_RXP_ACCEPT_CNT);
    pr_info("DMA_CH1_RXP_ACCEPT_CNT: 0x%x\n", val);
    val = readl(ioaddr + DMA_CH2_RXP_ACCEPT_CNT);
    pr_info("DMA_CH2_RXP_ACCEPT_CNT: 0x%x\n", val);
    val = readl(ioaddr + DMA_CH3_RXP_ACCEPT_CNT);
    pr_info("DMA_CH3_RXP_ACCEPT_CNT: 0x%x\n", val);
    val = readl(ioaddr + DMA_CH4_RXP_ACCEPT_CNT);
    pr_info("DMA_CH4_RXP_ACCEPT_CNT: 0x%x\n", val);

    for (int i = 0; i < 5; i++) {
        pr_info("DMA_CHANNEL(%d) Registers\n", i);
        _dwmac4_dump_dma_regs(ioaddr, i);
        val = readl(ioaddr + MTL_CHAN_TX_OP_MODE(i));
        pr_info("    MTL_CHAN_TX_OP_MODE(%d): 0x%x\n", i, val);
        // print MTL_CHAN_TX_DEBUG
        //val = readl(ioaddr + DMA_CHAN_CONTROL(i));
        //pr_info("DMA_CHAN_CONTROL(%d): 0x%x\n", i, val);
        //val = readl(ioaddr + DMA_CHAN_TX_CONTROL(i));
        //pr_info("DMA_CHAN_TX_CONTROL(%d): 0x%x\n", i, val);
        val = readl(ioaddr + MTL_CHAN_TX_DEBUG(i));
        pr_info("    MTL_CHAN_TX_DEBUG(%d): 0x%x\n", i, val);
    }


    return;
}

int dwmac5_frp_update_num_entries(void __iomem *ioaddr, uint32_t num_entries)
{
    u32 val;

    val = readl(ioaddr + MTL_RXP_CONTROL_STATUS);

	/*
	 * Program Number of Valid Entries (NVE) and Number of Parsable Entries
	 * (NPE). Do not OR-in: stale values lead to bogus table sizes and
	 * unpredictable matching.
	 */
	val &= ~(NVE | NPE);
	val |= num_entries & NVE;
	val |= (num_entries << 16) & NPE;
	writel(val, ioaddr + MTL_RXP_CONTROL_STATUS);

    return 0;
}

int dwmac5_frp_update_single_entry(void __iomem *ioaddr, 
        union frp_instruction *instr, int pos)
{
    int ret, i;

    pr_info("dwmac5_frp_update_single_entry... pos: %d\n", pos);

    // Iterate through the 4 x 32-bit parts of the instruction
    for (i = 0; i < 4; i++) {
        int real_pos = pos * 4 + i;
        u32 val;

        /* Wait for ready */
        ret = readl_poll_timeout(ioaddr + MTL_RXP_IACC_CTRL_STATUS,
                val, !(val & STARTBUSY), 1, 10000);
        if (ret)
            return ret;

        /* Write data */
        val = instr->as_array[i];
        pr_info("Writing data: 0x%x\n", val);
        writel(val, ioaddr + MTL_RXP_IACC_DATA);

        /* Write pos */
        val = real_pos & ADDR;
        writel(val, ioaddr + MTL_RXP_IACC_CTRL_STATUS);

        /* Write OP */
        val |= WRRDN;
        writel(val, ioaddr + MTL_RXP_IACC_CTRL_STATUS);

        /* Start Write */
        val |= STARTBUSY;
        writel(val, ioaddr + MTL_RXP_IACC_CTRL_STATUS);

        /* Wait for done */
        ret = readl_poll_timeout(ioaddr + MTL_RXP_IACC_CTRL_STATUS,
                val, !(val & STARTBUSY), 1, 10000);
        if (ret)
            return ret;
    }

    return 0;  // Success
}

static int dwmac5_rxp_update_single_entry(void __iomem *ioaddr,
					  struct stmmac_tc_entry *entry,
					  int pos)
{
	int ret, i;
	const int entry_size_words = sizeof(entry->val) / sizeof(u32);

	pr_debug("dwmac5_rxp_update_single_entry: pos=%d, entry_size=%d words\n", 
		 pos, entry_size_words);

	/* Validate inputs */
	if (!ioaddr || !entry || pos < 0) {
		pr_err("dwmac5_rxp_update_single_entry: invalid parameters\n");
		return -EINVAL;
	}

	for (i = 0; i < entry_size_words; i++) {
		int real_pos = pos * entry_size_words + i;
		u32 val;
		u32 *data_ptr = (u32 *)&entry->val + i;

		/* Validate position is within valid range */
		if (real_pos > ADDR) {
			pr_err("dwmac5_rxp_update_single_entry: position %d exceeds maximum %lu\n",
			       real_pos, ADDR);
			return -EINVAL;
		}

		/* Wait for hardware to be ready */
		ret = readl_poll_timeout(ioaddr + MTL_RXP_IACC_CTRL_STATUS,
					val, !(val & STARTBUSY), 1, 10000);
		if (ret) {
			pr_err("dwmac5_rxp_update_single_entry: timeout waiting for ready, word %d\n", i);
			return ret;
		}

		/* Write data */
		val = *data_ptr;
		pr_debug("dwmac5_rxp_update_single_entry: writing data[%d]=0x%08x to pos=%d\n", 
			 i, val, real_pos);
		writel(val, ioaddr + MTL_RXP_IACC_DATA);

		/* Write position */
		val = real_pos & ADDR;
		writel(val, ioaddr + MTL_RXP_IACC_CTRL_STATUS);

		/* Set write operation flag */
		val |= WRRDN;
		writel(val, ioaddr + MTL_RXP_IACC_CTRL_STATUS);

		/* Start the write operation */
		val |= STARTBUSY;
		writel(val, ioaddr + MTL_RXP_IACC_CTRL_STATUS);

		/* Wait for operation completion */
		ret = readl_poll_timeout(ioaddr + MTL_RXP_IACC_CTRL_STATUS,
					val, !(val & STARTBUSY), 1, 10000);
		if (ret) {
			pr_err("dwmac5_rxp_update_single_entry: timeout waiting for completion, word %d\n", i);
			return ret;
		}
	}

	pr_debug("dwmac5_rxp_update_single_entry: successfully updated entry at pos %d\n", pos);
	return 0;
}

static struct stmmac_tc_entry *
dwmac5_rxp_get_next_entry(struct stmmac_tc_entry *entries, unsigned int count,
			  u32 curr_prio)
{
	struct stmmac_tc_entry *entry;
	u32 min_prio = ~0x0;
	int i, min_prio_idx;
	bool found = false;

	for (i = count - 1; i >= 0; i--) {
		entry = &entries[i];

		/* Do not update unused entries */
		if (!entry->in_use)
			continue;
		/* Do not update already updated entries (i.e. fragments) */
		if (entry->in_hw)
			continue;
		/* Let last entry be updated last */
		if (entry->is_last)
			continue;
		/* Do not return fragments */
		if (entry->is_frag)
			continue;
		/* Check if we already checked this prio */
		if (entry->prio < curr_prio)
			continue;
		/* Check if this is the minimum prio */
		if (entry->prio < min_prio) {
			min_prio = entry->prio;
			min_prio_idx = i;
			found = true;
		}
	}

	if (found)
		return &entries[min_prio_idx];

	return NULL;
}

int dwmac5_rxp_config(void __iomem *ioaddr, struct stmmac_tc_entry *entries,
		      unsigned int count)
{
	struct stmmac_tc_entry *entry, *frag;
	int i, ret, nve = 0;
	u32 curr_prio = 0;
	u32 old_val, val;
	bool has_valid_entry = false;

    pr_info("dwmac5_rxp_config... %d entries\n", count);
	/* Force disable RX */
	old_val = readl(ioaddr + GMAC_CONFIG);
	val = old_val & ~GMAC_CONFIG_RE;
	writel(val, ioaddr + GMAC_CONFIG);
	pr_info("dwmac5_rxp_config: RX disabled (GMAC_CONFIG)\n");

	/* Disable RX Parser */
	ret = dwmac5_rxp_disable(ioaddr);
	if (ret) {
		pr_err("dwmac5_rxp_config: Failed to disable RX parser\n");
		goto re_enable;
	}
	/* Clear all entries in_hw flags */
	for (i = 0; i < count; i++)
		entries[i].in_hw = false;

	/* Check for at least one valid entry (excluding last entry) */
	for (i = 0; i < count; i++) {
		entry = &entries[i];
		if (entry->in_use && !entry->is_last && !entry->is_frag)
			has_valid_entry = true;
	}

	if (!has_valid_entry) {
		pr_warn("dwmac5_rxp_config: No valid RXP entries to configure.\n");
		ret = -EINVAL;
		goto re_enable;
	}

	/* Update entries by reverse order */
	while (1) {
		entry = dwmac5_rxp_get_next_entry(entries, count, curr_prio);
		pr_info("Checking for next RX entry: prio=%d, count=%d\n", curr_prio, count);
		if (!entry) {
			pr_info("No RX entry found for priority %d. Exiting loop.\n", curr_prio);
			break;
		}
		
		curr_prio = entry->prio;
		frag = entry->frag_ptr;

		/* Set special fragment requirements */
		if (frag) {
			entry->val.af = 0;
			entry->val.rf = 0;
			entry->val.nc = 1;
			entry->val.ok_index = nve + 2;
		}

		ret = dwmac5_rxp_update_single_entry(ioaddr, entry, nve);
		if (ret)
			goto re_enable;

		entry->table_pos = nve++;
		entry->in_hw = true;

		if (frag && !frag->in_hw) {
			ret = dwmac5_rxp_update_single_entry(ioaddr, frag, nve);
			if (ret)
				goto re_enable;
			frag->table_pos = nve++;
			frag->in_hw = true;
		}
	}

	if (!nve)
		goto re_enable;

	/* Update all pass entry */
	for (i = 0; i < count; i++) {
		entry = &entries[i];
		if (!entry->is_last)
			continue;

		ret = dwmac5_rxp_update_single_entry(ioaddr, entry, nve);
		if (ret)
			goto re_enable;

		entry->table_pos = nve++;
	}

	/* Assume n. of parsable entries == n. of valid entries */
	val = (nve << 16) & NPE;
	val |= nve & NVE;
	writel(val, ioaddr + MTL_RXP_CONTROL_STATUS);

	/* Enable RX Parser */
	dwmac5_rxp_enable(ioaddr);

re_enable:
	/* Re-enable RX */
	writel(old_val, ioaddr + GMAC_CONFIG);
	return ret;
}

int dwmac5_flex_pps_config(void __iomem *ioaddr, int index,
			   struct stmmac_pps_cfg *cfg, bool enable,
			   u32 sub_second_inc, u32 systime_flags)
{
	u32 tnsec = readl(ioaddr + MAC_PPSx_TARGET_TIME_NSEC(index));
	u32 val = readl(ioaddr + MAC_PPS_CONTROL);
	u64 period;

	if (!cfg->available)
		return -EINVAL;
	if (tnsec & TRGTBUSY0)
		return -EBUSY;
	if (!sub_second_inc || !systime_flags)
		return -EINVAL;

	val &= ~PPSx_MASK(index);

	if (!enable) {
		val |= PPSCMDx(index, 0x5);
		val |= PPSEN0;
		writel(val, ioaddr + MAC_PPS_CONTROL);
		return 0;
	}

	val |= TRGTMODSELx(index, 0x2);
	val |= PPSEN0;
	writel(val, ioaddr + MAC_PPS_CONTROL);

	writel(cfg->start.tv_sec, ioaddr + MAC_PPSx_TARGET_TIME_SEC(index));

	if (!(systime_flags & PTP_TCR_TSCTRLSSR))
		cfg->start.tv_nsec = (cfg->start.tv_nsec * 1000) / 465;
	writel(cfg->start.tv_nsec, ioaddr + MAC_PPSx_TARGET_TIME_NSEC(index));

	period = cfg->period.tv_sec * 1000000000;
	period += cfg->period.tv_nsec;

	do_div(period, sub_second_inc);

	if (period <= 1)
		return -EINVAL;

	writel(period - 1, ioaddr + MAC_PPSx_INTERVAL(index));

	period >>= 1;
	if (period <= 1)
		return -EINVAL;

	writel(period - 1, ioaddr + MAC_PPSx_WIDTH(index));

	/* Finally, activate it */
	val |= PPSCMDx(index, 0x2);
	writel(val, ioaddr + MAC_PPS_CONTROL);
	return 0;
}

static int dwmac5_est_write(void __iomem *ioaddr, u32 reg, u32 val, bool gcl)
{
	u32 ctrl;

	writel(val, ioaddr + MTL_EST_GCL_DATA);

	ctrl = (reg << ADDR_SHIFT);
	ctrl |= gcl ? 0 : GCRR;

	writel(ctrl, ioaddr + MTL_EST_GCL_CONTROL);

	ctrl |= SRWO;
	writel(ctrl, ioaddr + MTL_EST_GCL_CONTROL);

	return readl_poll_timeout(ioaddr + MTL_EST_GCL_CONTROL,
				  ctrl, !(ctrl & SRWO), 100, 5000);
}

int dwmac5_est_configure(void __iomem *ioaddr, struct stmmac_est *cfg,
			 unsigned int ptp_rate)
{
	int i, ret = 0x0;
	u32 ctrl;

	ret |= dwmac5_est_write(ioaddr, BTR_LOW, cfg->btr[0], false);
	ret |= dwmac5_est_write(ioaddr, BTR_HIGH, cfg->btr[1], false);
	ret |= dwmac5_est_write(ioaddr, TER, cfg->ter, false);
	ret |= dwmac5_est_write(ioaddr, LLR, cfg->gcl_size, false);
	ret |= dwmac5_est_write(ioaddr, CTR_LOW, cfg->ctr[0], false);
	ret |= dwmac5_est_write(ioaddr, CTR_HIGH, cfg->ctr[1], false);
	if (ret)
		return ret;

	for (i = 0; i < cfg->gcl_size; i++) {
		ret = dwmac5_est_write(ioaddr, i, cfg->gcl[i], true);
		if (ret)
			return ret;
	}

	ctrl = readl(ioaddr + MTL_EST_CONTROL);
	ctrl &= ~PTOV;
	ctrl |= ((1000000000 / ptp_rate) * 6) << PTOV_SHIFT;
	if (cfg->enable)
		ctrl |= EEST | SSWL;
	else
		ctrl &= ~EEST;

	writel(ctrl, ioaddr + MTL_EST_CONTROL);

	/* Configure EST interrupt */
	if (cfg->enable)
		ctrl = (IECGCE | IEHS | IEHF | IEBE | IECC);
	else
		ctrl = 0;

	writel(ctrl, ioaddr + MTL_EST_INT_EN);

	return 0;
}

void dwmac5_est_irq_status(void __iomem *ioaddr, struct net_device *dev,
			  struct stmmac_extra_stats *x, u32 txqcnt)
{
	u32 status, value, feqn, hbfq, hbfs, btrl;
	u32 txqcnt_mask = (1 << txqcnt) - 1;

	status = readl(ioaddr + MTL_EST_STATUS);

	value = (CGCE | HLBS | HLBF | BTRE | SWLC);

	/* Return if there is no error */
	if (!(status & value))
		return;

	if (status & CGCE) {
		/* Clear Interrupt */
		writel(CGCE, ioaddr + MTL_EST_STATUS);

		x->mtl_est_cgce++;
	}

	if (status & HLBS) {
		value = readl(ioaddr + MTL_EST_SCH_ERR);
		value &= txqcnt_mask;

		x->mtl_est_hlbs++;

		/* Clear Interrupt */
		writel(value, ioaddr + MTL_EST_SCH_ERR);

		/* Collecting info to shows all the queues that has HLBS
		 * issue. The only way to clear this is to clear the
		 * statistic
		 */
		if (net_ratelimit())
			netdev_err(dev, "EST: HLB(sched) Queue 0x%x\n", value);
	}

	if (status & HLBF) {
		value = readl(ioaddr + MTL_EST_FRM_SZ_ERR);
		feqn = value & txqcnt_mask;

		value = readl(ioaddr + MTL_EST_FRM_SZ_CAP);
		hbfq = (value & SZ_CAP_HBFQ_MASK(txqcnt)) >> SZ_CAP_HBFQ_SHIFT;
		hbfs = value & SZ_CAP_HBFS_MASK;

		x->mtl_est_hlbf++;

		/* Clear Interrupt */
		writel(feqn, ioaddr + MTL_EST_FRM_SZ_ERR);

		if (net_ratelimit())
			netdev_err(dev, "EST: HLB(size) Queue %u Size %u\n",
				   hbfq, hbfs);
	}

	if (status & BTRE) {
		if ((status & BTRL) == BTRL_MAX)
			x->mtl_est_btrlm++;
		else
			x->mtl_est_btre++;

		btrl = (status & BTRL) >> BTRL_SHIFT;

		if (net_ratelimit())
			netdev_info(dev, "EST: BTR Error Loop Count %u\n",
				    btrl);

		writel(BTRE, ioaddr + MTL_EST_STATUS);
	}

	if (status & SWLC) {
		writel(SWLC, ioaddr + MTL_EST_STATUS);
		netdev_info(dev, "EST: SWOL has been switched\n");
	}
}

void dwmac5_fpe_configure(void __iomem *ioaddr, u32 num_txq, u32 num_rxq,
			  bool enable, struct stmmac_fpe *fpe)
{
	u32 value;

	if (fpe) {
		value = fpe->p_queues << MTL_FPECTRL_PEC_SHIFT | fpe->fragsize;
		writel(value, ioaddr + MTL_FPE_CTRL_STS);
	}

	if (!enable) {
		value = readl(ioaddr + MAC_FPE_CTRL_STS);

		value &= ~EFPE;

		writel(value, ioaddr + MAC_FPE_CTRL_STS);
		return;
	}

	value = readl(ioaddr + GMAC_RXQ_CTRL1);
	value &= ~GMAC_RXQCTRL_FPRQ;
	value |= (num_rxq - 1) << GMAC_RXQCTRL_FPRQ_SHIFT;
	writel(value, ioaddr + GMAC_RXQ_CTRL1);

	value = readl(ioaddr + MAC_FPE_CTRL_STS);
	value |= EFPE;
	writel(value, ioaddr + MAC_FPE_CTRL_STS);
}

void dwmac5_fpe_configure_get(void __iomem *ioaddr, struct stmmac_fpe *fpe)
{
	u32 value;

	value = readl(ioaddr + MAC_FPE_CTRL_STS);
	fpe->enable = value & EFPE;

	value = readl(ioaddr + MTL_FPE_CTRL_STS);
	fpe->p_queues = (value >> MTL_FPECTRL_PEC_SHIFT) & GENMASK(7, 0);
	fpe->fragsize = value & GENMASK(1, 0);
}

int dwmac5_fpe_irq_status(void __iomem *ioaddr, struct net_device *dev)
{
	u32 value;
	int status;

	status = FPE_EVENT_UNKNOWN;

	value = readl(ioaddr + MAC_FPE_CTRL_STS);

	if (value & TRSP) {
		status |= FPE_EVENT_TRSP;
		netdev_info(dev, "FPE: Respond mPacket is transmitted\n");
	}

	if (value & TVER) {
		status |= FPE_EVENT_TVER;
		netdev_info(dev, "FPE: Verify mPacket is transmitted\n");
	}

	if (value & RRSP) {
		status |= FPE_EVENT_RRSP;
		netdev_info(dev, "FPE: Respond mPacket is received\n");
	}

	if (value & RVER) {
		status |= FPE_EVENT_RVER;
		netdev_info(dev, "FPE: Verify mPacket is received\n");
	}

	return status;
}

void dwmac5_fpe_send_mpacket(void __iomem *ioaddr, enum stmmac_mpacket_type type)
{
	u32 value;

	value = readl(ioaddr + MAC_FPE_CTRL_STS);

	if (type == MPACKET_VERIFY) {
		value &= ~SRSP;
		value |= SVER;
	} else {
		value &= ~SVER;
		value |= SRSP;
	}

	writel(value, ioaddr + MAC_FPE_CTRL_STS);
}
