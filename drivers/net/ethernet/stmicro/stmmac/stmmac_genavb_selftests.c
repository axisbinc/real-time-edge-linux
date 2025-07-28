#include <net/pkt_cls.h>
#include <net/tc_act/tc_gact.h>
#include "stmmac.h"
#include "stmmac_frp.h"
#ifdef CONFIG_STMMAC_GENAVB
#include "stmmac_genavb.h"
#define STMMAC_AVB_TX_ROOT_CAUSE_TESTS 1

static bool stmmac_avb_test_in_progress = false;

bool stmmac_avb_test_is_in_progress(void)
{
    return stmmac_avb_test_in_progress;
}

#if STMMAC_AVB_TX_ROOT_CAUSE_TESTS
static struct sk_buff *stmmac_avb_create_skb_from_frame(struct stmmac_priv *priv,
        const u8 eth_frame[], u16 eth_frame_len)
{
	struct sk_buff *skb;

    skb = netdev_alloc_skb(priv->dev, eth_frame_len + 2);
    if (!skb) {
        netdev_err(priv->dev, "Failed to alloc skb\n");
        return NULL;
    }

    skb_reset_mac_header(skb);

    // Copy the entire Ethernet frame (including header) into the skb
    memcpy(skb_put(skb, eth_frame_len), eth_frame, eth_frame_len);

    // Set up skb metadata
    skb->dev = priv->dev;
    // skb->protocol = eth_type_trans(skb, priv->dev);
    skb->protocol = htons(ETH_P_TSN);
    skb->ip_summed = CHECKSUM_UNNECESSARY;

    return skb;
}

struct sk_buff *stmmac_avb_create_adp(struct stmmac_priv *priv)
{
    const uint8_t avtp_discovery[] = {
        // Destination MAC: 00:04:9f:08:54:33
        0x00, 0x04, 0x9f, 0x08, 0x54, 0x33,
        // Source MAC: 00:04:9f:08:54:33
        0x00, 0x04, 0x9f, 0x08, 0x54, 0x33,
        // EtherType
        0x22, 0xf0,
        // AVTPDU
        0xfa,  // AVTP_SUBTYPE_ADP
        0x02,  // Version, Sub-version
        0x00, 0x38, // Length
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    return stmmac_avb_create_skb_from_frame(priv,
            avtp_discovery, sizeof(avtp_discovery));
}

#define STMMAC_LB_TIMEOUT	msecs_to_jiffies(200)

struct stmmac_avb_test_priv {
	struct packet_type pt;
    const uint8_t *dst;
	struct completion comp;
	int double_vlan;
	int vlan_id;
	int ok;
	int payload_checksum;
	int payload_length;
	int payload_offset;
};

/* Simple checksum calculation for payload validation */
static int calculate_payload_checksum(const unsigned char *data, int length)
{
	int checksum = 0;
	int i;
	
	for (i = 0; i < length; i++) {
		checksum += data[i];
	}
	return checksum;
}

static int stmmac_avb_test_loopback_validate(struct sk_buff *skb,
					 struct net_device *ndev,
					 struct packet_type *pt,
					 struct net_device *orig_ndev)
{
	struct stmmac_avb_test_priv *tpriv = pt->af_packet_priv;
	const unsigned char *dst = tpriv->dst;
	struct ethhdr *ehdr;

	skb = skb_unshare(skb, GFP_ATOMIC);
	if (!skb) {
        netdev_err(ndev, "Failed to unshare skb\n");
		goto out;
    }

	if (skb_linearize(skb)) {
        netdev_err(ndev, "Failed to linearize skb\n");
		goto out;
    }

	ehdr = (struct ethhdr *)skb_mac_header(skb);
    pr_info("Eth: from %pM to: %pM\n", ehdr->h_source, ehdr->h_dest);
    pr_info("Eth: type: 0x%04x\n", ntohs(ehdr->h_proto));

	if (dst && !ether_addr_equal_unaligned(ehdr->h_dest, dst)) {
        netdev_err(ndev, "Failed to validate destination MAC\n");
		goto out;
    }
    if (!ether_addr_equal_unaligned(ehdr->h_source, ehdr->h_dest)) {
        netdev_err(ndev, "Failed to validate source MAC\n");
		goto out;
    }

	/* Validate payload checksum if payload is present */
	if (tpriv->payload_length > 0) {
		const unsigned char *payload_data;
		int computed_checksum;
		int full_packet_len;
		
		/* The MAC header contains the Ethernet header, and skb->data points to IP header.
		 * To access the full packet with Ethernet header, we use skb_mac_header().
		 * Calculate the payload offset from the MAC header. */
		
		full_packet_len = skb->len + ETH_HLEN;
		
		/* Check if packet has enough data for the payload */
		if (full_packet_len < tpriv->payload_offset + tpriv->payload_length) {
			netdev_err(ndev, "Packet too short for payload validation (got %d, need %d)\n",
				   full_packet_len, tpriv->payload_offset + tpriv->payload_length);
			goto out;
		}
		
		/* Access payload from MAC header base + offset */
		payload_data = skb_mac_header(skb) + tpriv->payload_offset;
		computed_checksum = calculate_payload_checksum(payload_data, tpriv->payload_length);
		
		if (computed_checksum != tpriv->payload_checksum) {
			netdev_err(ndev, "Payload checksum mismatch (expected %d, got %d)\n",
				   tpriv->payload_checksum, computed_checksum);
			goto out;
		}
		
		pr_info("Payload validation passed (%d bytes, checksum %d)\n",
			tpriv->payload_length, computed_checksum);
	}

	tpriv->ok = true;
	complete(&tpriv->comp);
out:
	kfree_skb(skb);
	return 0;
}

static int stmmac_avb_test_mac_loopback(struct stmmac_priv *priv,
        struct sk_buff *skb, u16 eth_type, int payload_checksum, int payload_length, int payload_offset)
{
	struct stmmac_avb_test_priv *tpriv;
	int ret = 0;

	tpriv = kzalloc(sizeof(*tpriv), GFP_KERNEL);
	if (!tpriv)
		return -ENOMEM;

	tpriv->ok = false;
	init_completion(&tpriv->comp);

	tpriv->pt.type = htons(eth_type);
	tpriv->pt.func = stmmac_avb_test_loopback_validate;
	tpriv->pt.dev = priv->dev;
	tpriv->pt.af_packet_priv = tpriv;
    tpriv->dst = priv->dev->dev_addr;
    tpriv->payload_checksum = payload_checksum;
    tpriv->payload_length = payload_length;
    tpriv->payload_offset = payload_offset;

    dev_add_pack(&tpriv->pt);

	ret = dev_direct_xmit(skb, 0);
	if (ret) {
        netdev_err(priv->dev, "Failed to send loopback packet\n");
		goto cleanup;
    }

	wait_for_completion_timeout(&tpriv->comp, STMMAC_LB_TIMEOUT);
    if (!tpriv->ok) {
        netdev_err(priv->dev, "Loopback test TIMEOUT\n");
        ret = -ETIMEDOUT;
    }

cleanup:
	dev_remove_pack(&tpriv->pt);
	kfree(tpriv);
	return ret;
}

struct stmmac_packet_attrs {
	int vlan;
	int vlan_id_in;
	int vlan_id_out;
	const unsigned char *src;
	const unsigned char *dst;
	u32 ip_src;
	u32 ip_dst;
	int tcp;
	int sport;
	int dport;
	u32 exp_hash;
	int dont_wait;
	int timeout;
	int size;
	int max_size;
	int remove_sa;
	u8 id;
	int sarc;
	u16 queue_mapping;
	u64 timestamp;
};

struct sk_buff *stmmac_test_get_udp_skb(struct stmmac_priv *priv,
					       struct stmmac_packet_attrs *attr);

int stmmac_test_mac_loopback(struct stmmac_priv *priv);

static int stmmac_avb_test_udp_packet_as_skb(struct stmmac_priv *priv)
{
    struct stmmac_packet_attrs attr = {};
    struct sk_buff *skb;
    int ret;
    int payload_offset;
    /* 64-byte test payload */
    static const unsigned char test_payload[64] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
        0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
        0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
        0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01
    };

    attr.src = attr.dst = priv->dev->dev_addr;

    skb = stmmac_test_get_udp_skb(priv, &attr);
    if (!skb) {
        netdev_err(priv->dev, "Failed to get UDP skb\n");
        return -ENOMEM;
    }

    pr_info("stmmac_avb_test_udp_packet_as_skb\n");

    /* Add our test payload to the UDP packet */
    if (skb_tailroom(skb) >= sizeof(test_payload)) {
        int payload_checksum;
        
        /* The payload will be at the end of the received packet */
        /* Calculate where it will be: received packet length - payload length */
        
        memcpy(skb_put(skb, sizeof(test_payload)), test_payload, sizeof(test_payload));
        payload_checksum = calculate_payload_checksum(test_payload, sizeof(test_payload));
        
        /* The payload will be at the end of the received packet */
        payload_offset = skb->len - sizeof(test_payload);
        
        pr_info("Added %lu-byte test payload. Total packet length: %d, checksum: %d\n", 
                sizeof(test_payload), skb->len, payload_checksum);
        
        ret = stmmac_avb_test_mac_loopback(priv, skb, ETH_P_IP, payload_checksum, sizeof(test_payload), payload_offset);
    } else {
        pr_warn("Not enough tailroom for test payload\n");
        ret = stmmac_avb_test_mac_loopback(priv, skb, ETH_P_IP, 0, 0, 0);
    }
    if (0 == ret) {
        pr_info("stmmac_avb_test_udp_packet_as_skb passed\n");
    }
    else {
        pr_err("stmmac_avb_test_udp_packet_as_skb failed\n");
    }
    return ret;
}

static int stmmac_avb_test_avtp_packet_as_skb(struct stmmac_priv *priv)
{
    struct sk_buff *skb;
    int ret = 0;

    pr_info("stmmac_avb_test_avtp_packet_as_skb\n");

    skb = stmmac_avb_create_adp(priv);
    if (!skb) {
        pr_err("Failed to create ADP packet\n");
        return -ENOMEM;
    }

    ret = stmmac_avb_test_mac_loopback(priv, skb, ETH_P_TSN, 0, 0, 0);
    if (0 == ret) {
        pr_info("stmmac_avb_test_avtp_packet_as_skb passed\n");
    }
    else {
        pr_err("stmmac_avb_test_avtp_packet_as_skb failed\n");
    }

    return ret;
}
#endif  // STMMAC_AVB_TX_ROOT_CAUSE_TESTS

static int stmmac_send_avtp_packet(struct stmmac_priv *priv,  unsigned int queue_id)
{
    int ret = 0;
    //struct stmmac_avb_tx_queue *tx_q = &priv->dma_avb_conf->tx_queue;
    struct avb_tx_desc *desc;
    /*
    const uint8_t avtp_discovery[] = {
        0x91, 0xe0, 0xf0, 0x01, 0x00, 0x00, 0x00, 0x01, 0xf2, 0xff, 0x3b, 0x06, 0x22, 0xf0, 0xfa, 0x02,
        0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
    */
    const uint8_t avtp_discovery[] = {
        // Destination MAC: 00:04:9f:08:54:33
        0x00, 0x04, 0x9f, 0x08, 0x54, 0x33,
        // Source MAC: 00:04:9f:08:54:33
        0x00, 0x04, 0x9f, 0x08, 0x54, 0x33,
        // EtherType
        0x22, 0xf0,
        // AVTPDU
        0xfa,  // AVTP_SUBTYPE_ADP
        0x02,  // Version, Sub-version
        0x00, 0x38, // Length
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };

    pr_info("[%d] %s\n", __LINE__, __func__);

    desc = priv->avb->alloc(priv->avb_data);
    if (!desc) {
        netdev_err(priv->dev, "Failed to alloc tx buffer\n");
        return -ENOMEM;
    }

    /* copy the avtp packet to the offset and update the length */
    memcpy((void*)desc + desc->common.offset, avtp_discovery, sizeof(avtp_discovery));
    desc->common.len = sizeof(avtp_discovery);

    /* print the device MAC address */
    pr_info("Destination MAC: %pM\n", priv->dev->dev_addr);

    ret = (queue_id == STMMAC_AVB_CHANNEL) ? stmmac_enet_start_xmit_avb(priv, desc)
            : stmmac_avb_xmit_avb_tx_desc(priv, queue_id, desc);
    if (ret < 0) {
        netdev_err(priv->dev, "Failed to start xmit\n");
        return ret;
    }

    return ret;
}

static int stmmac_avb_test_avtp_packet_as_avb_desc(struct stmmac_priv *priv)
{
    int ret;

    pr_info("Testing AVB packet ... queue 0\n");

    /* send an AVTP Discovery packet */
    ret = stmmac_send_avtp_packet(priv, 0);
    if (ret) {
        pr_err("Failed to send AVTP packet\n");
    }

    pr_info("stmmac_avb_test_avtp_packet_as_avb_desc ... STMMAC_AVB_CHANNEL\n");

    /* send an AVTP Discovery packet */
    ret = stmmac_send_avtp_packet(priv, STMMAC_AVB_CHANNEL);
    if (ret) {
        pr_err("Failed to send AVTP packet\n");
    }

    return ret;
}
#if 0
int stmmac_avb_test_rxp(struct stmmac_priv *priv)
{
    int ret = 0;
    u16 eth_types[] = {ETH_P_TSN};  // avtp ether types

    stmmac_avb_test_in_progress = true;
    pr_info("stmmac_avb_test_rxp() Started...\n");

    if (stmmac_rxp_setup(priv, eth_types, ARRAY_SIZE(eth_types))) {
        pr_err("Failed to add AVB filter\n");
        ret = -1;
        goto error;
    }

    if (stmmac_set_mac_loopback(priv, priv->ioaddr, true)) {
        pr_err("Failed to set MAC loopback\n");
        ret = -1;
        goto error_clear_rxp;
    }

#if STMMAC_AVB_TX_ROOT_CAUSE_TESTS
    ret |= stmmac_avb_test_udp_packet_as_skb(priv);
    ret |= stmmac_avb_test_avtp_packet_as_skb(priv);
#endif
    ret |= stmmac_avb_test_avtp_packet_as_avb_desc(priv);

	/* Sleep to allow loopback to show packets */
	msleep(500);

    if (stmmac_set_mac_loopback(priv, priv->ioaddr, false)) {
        pr_err("Failed to clear MAC loopback\n");
    }

error_clear_rxp:
    if (stmmac_rxp_clear(priv)) {
        pr_err("Failed to delete AVB filter\n");
    }

error:
    pr_info("stmmac_avb_test_rxp() ...Ended\n");
    stmmac_avb_test_in_progress = false;
    return ret;
}
#endif
int stmmac_avb_test_rxp(struct stmmac_priv *priv)
{
	int ret = 0;
	u16 eth_types[] = { ETH_P_TSN };  // AVTP EtherType
	unsigned char addr[ETH_ALEN] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x00};
	struct tc_cls_u32_offload cls_u32 = { };
	struct stmmac_packet_attrs attr = { };
	struct tc_action **actions;
	struct tc_u32_sel *sel;
	struct tcf_gact *gact;
	struct tcf_exts *exts;
	int i, nk = 1;

	stmmac_avb_test_in_progress = true;
	pr_info("stmmac_avb_test_rxp() Started...\n");

	/* Dump FRP stats before the test */
	pr_info("stmmac_avb_test_rxp: Dumping FRP stats before test...\n");
	dwmac5_frp_dump_stats(priv->ioaddr);

	/* Set up RX Parser for AVTP type */
	if (stmmac_rxp_setup(priv, eth_types, ARRAY_SIZE(eth_types))) {
		pr_err("Failed to add AVB filter\n");
		ret = -1;
		goto error;
	}

	/* Enable MAC loopback */
	if (stmmac_set_mac_loopback(priv, priv->ioaddr, true)) {
		pr_err("Failed to set MAC loopback\n");
		ret = -1;
		goto error_clear_rxp;
	}

#if STMMAC_AVB_TX_ROOT_CAUSE_TESTS
	ret |= stmmac_avb_test_udp_packet_as_skb(priv);
	ret |= stmmac_avb_test_avtp_packet_as_skb(priv);
#endif
	ret |= stmmac_avb_test_avtp_packet_as_avb_desc(priv);

	/* Install TC rule to drop packets from a specific MAC address */
	sel = kzalloc(struct_size(sel, keys, nk), GFP_KERNEL);
	if (!sel) {
		pr_err("Failed to allocate sel\n");
		ret = -ENOMEM;
		goto loopback_clear;
	}

	exts = kzalloc(sizeof(*exts), GFP_KERNEL);
	if (!exts) {
		pr_err("Failed to allocate exts\n");
		ret = -ENOMEM;
		goto cleanup_sel;
	}

	actions = kcalloc(nk, sizeof(*actions), GFP_KERNEL);
	if (!actions) {
		pr_err("Failed to allocate actions\n");
		ret = -ENOMEM;
		goto cleanup_exts;
	}

	gact = kcalloc(nk, sizeof(*gact), GFP_KERNEL);
	if (!gact) {
		pr_err("Failed to allocate gact\n");
		ret = -ENOMEM;
		goto cleanup_actions;
	}

	pr_info("Installing TC rule to drop packets from 0xdeadbeef\n");

	cls_u32.command = TC_CLSU32_NEW_KNODE;
	cls_u32.common.chain_index = 0;
	cls_u32.common.protocol = htons(ETH_P_ALL);
	cls_u32.knode.exts = exts;
	cls_u32.knode.sel = sel;
	cls_u32.knode.handle = 0x123;

	exts->nr_actions = nk;
	exts->actions = actions;
	for (i = 0; i < nk; i++) {
		actions[i] = (struct tc_action *)&gact[i];
		gact->tcf_action = TC_ACT_SHOT;
	}

	sel->nkeys = nk;
	sel->offshift = 0;
	sel->keys[0].off = 6;
	sel->keys[0].val = htonl(0xdeadbeef);
	sel->keys[0].mask = ~0x0;

	ret = stmmac_tc_setup_cls_u32(priv, priv, &cls_u32);
	if (ret) {
		pr_err("Failed to install TC rule, ret=%d\n", ret);
		goto cleanup_act;
	}

	/* Send test packet */
	attr.dst = priv->dev->dev_addr;
	attr.src = addr;

	pr_info("Sending test packet to verify drop rule\n");
	ret = stmmac_test_mac_loopback(priv);
	if (ret)
		pr_info("Packet dropped as expected (PASS)\n");
	else
		pr_warn("Packet received (FAIL)\n");

	ret = ret ? 0 : -EINVAL;  // Fail if packet was received

	/* Dump FRP stats after test */
	pr_info("Dumping FRP stats after test...\n");
	dwmac5_frp_dump_stats(priv->ioaddr);

	/* Delete TC rule */
	pr_info("Cleaning up TC rule\n");
	cls_u32.command = TC_CLSU32_DELETE_KNODE;
	stmmac_tc_setup_cls_u32(priv, priv, &cls_u32);

cleanup_act:
	kfree(gact);
cleanup_actions:
	kfree(actions);
cleanup_exts:
	kfree(exts);
cleanup_sel:
	kfree(sel);

loopback_clear:
	/* Sleep to allow loopback to show packets */
	msleep(500);

	/* Disable loopback */
	if (stmmac_set_mac_loopback(priv, priv->ioaddr, false)) {
		pr_err("Failed to clear MAC loopback\n");
	}

error_clear_rxp:
	if (stmmac_rxp_clear(priv)) {
		pr_err("Failed to delete AVB filter\n");
	}

error:
	pr_info("stmmac_avb_test_rxp() ...Ended\n");
	stmmac_avb_test_in_progress = false;
	return ret;
}

#endif  // CONFIG_STMMAC_AVB
