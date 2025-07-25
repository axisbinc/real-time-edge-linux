#include "stmmac.h"
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
};

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

	tpriv->ok = true;
	complete(&tpriv->comp);
out:
	kfree_skb(skb);
	return 0;
}

static int stmmac_avb_test_mac_loopback(struct stmmac_priv *priv,
        struct sk_buff *skb, u16 eth_type)
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

    attr.src = attr.dst = priv->dev->dev_addr;

    skb = stmmac_test_get_udp_skb(priv, &attr);
    if (!skb) {
        netdev_err(priv->dev, "Failed to get UDP skb\n");
        return -ENOMEM;
    }

    ret = stmmac_avb_test_mac_loopback(priv, skb, ETH_P_IP);
    if (0 == ret) {
        pr_info("UDP loopback test passed\n");
    }
    else {
        pr_err("UDP loopback test failed\n");
    }
    return ret;
}

static int stmmac_avb_test_avtp_packet_as_skb(struct stmmac_priv *priv)
{
    struct sk_buff *skb;
    int ret = 0;

    skb = stmmac_avb_create_adp(priv);
    if (!skb) {
        pr_err("Failed to create ADP packet\n");
        return -ENOMEM;
    }

    ret = stmmac_avb_test_mac_loopback(priv, skb, ETH_P_TSN);
    if (0 == ret) {
        pr_info("AVB loopback test passed\n");
    }
    else {
        pr_err("AVB loopback testing check the packet on avb_channel \n");
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

#if 0
    pr_info("Testing AVB packet ... queue 0\n");

    /* send an AVTP Discovery packet */
    ret = stmmac_send_avtp_packet(priv, 0);
    if (ret) {
        pr_err("Failed to send AVTP packet\n");
    }
#endif
    pr_info("Testing AVB packet ... avb channel\n");

    /* send an AVTP Discovery packet */
    ret = stmmac_send_avtp_packet(priv, 4);
    if (ret) {
        pr_err("Failed to send AVTP packet\n");
    }

    return ret;
}

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
#endif  // CONFIG_STMMAC_AVB