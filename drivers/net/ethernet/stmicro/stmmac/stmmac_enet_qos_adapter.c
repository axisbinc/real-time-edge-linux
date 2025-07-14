/*
 Copyright (c) 2025 AxisB Inc.
*/
#include <linux/module.h>               // For kernel module macros
#include <linux/netdevice.h>            // For net_device
#include <linux/skbuff.h>               // For sk_buff
#include <linux/etherdevice.h>          // For Ethernet helpers
#include <linux/slab.h>                 // For kzalloc/kfree
#include <linux/debugfs.h>              // For debugfs interface
#include "stmmac_enet_qos_adapter.h"    // QOS Adapter header
#include "stmmac.h"
#include "stmmac_frp.h"
#include "stmmac_rxp.h"

// AVTP EtherType definition
#define ETH_P_AVTP 0x22F0

// Adapter context structure
struct stmmac_enet_qos_ctx {
	struct net_device *dev;          // Interface being monitored

#ifdef CONFIG_STMMAC_ENET_QOS_RELAY
	struct net_device *relay_dev;    // Optional forwarding interface (e.g., eth1)
#endif
};

// Global debugfs root directory
static struct dentry *qos_debugfs_root;
static bool stmmac_qos_debugfs_ready = false;
static struct stmmac_enet_qos_ctx *qos_ctx;

/**
 * qos_adapter_run_selftests - Simple placeholder self-test logic
 */
static void stmmac_enet_qos_run_selftests(void)
{
	pr_info("STMMAC_ENET_QOS: Running self-tests...\n");
	pr_info("STMMAC_ENET_QOS: Self-tests completed.\n");
}

/**
 * run_selftest_write - DebugFS callback: triggers self-tests on write
 */
static ssize_t run_selftest_write(struct file *file, const char __user *buf,
                                  size_t count, loff_t *ppos)
{
	char input;
	struct stmmac_priv *priv;

	if (!qos_ctx || !qos_ctx->dev)
		return -ENODEV;

	priv = netdev_priv(qos_ctx->dev);

	if (copy_from_user(&input, buf, 1))
		return -EFAULT;

	switch (input) {
	case 'D':
		pr_info("Dump FRP stats...\n");
		dwmac5_frp_dump_stats(priv->hw->pcsr);
		break;

	case 'T':
		pr_info("Running AVB RXP test...\n");
		stmmac_avb_test_rxp(priv);
		break;

	default:
		stmmac_enet_qos_run_selftests();
		break;
	}

	return count;
}

// File ops for debugfs "run_selftest"
static const struct file_operations run_selftest_fops = {
	.owner = THIS_MODULE,
	.write = run_selftest_write,
};

/**
 * qos_adapter_debugfs_init - Create debugfs entries
 */
void stmmac_enet_qos_debugfs_init(void)
{
	qos_debugfs_root = debugfs_create_dir("stmmac_enet_qos", NULL);
	if (!qos_debugfs_root || IS_ERR(qos_debugfs_root)) {
		pr_warn("STMMAC_ENET_QOS: Failed to create debugfs directory\n");
		return;
	}

	debugfs_create_file("run_selftest", 0200, qos_debugfs_root,
	                    NULL, &run_selftest_fops);
}

// Register the adapter on the specified net_device
struct stmmac_enet_qos_ctx *stmmac_enet_qos_register(struct net_device *dev)
{
	struct stmmac_enet_qos_ctx *ctx;

	if (!dev) {
		pr_err("STMMAC_ENET_QOS: Invalid device\n");
		return NULL;
	}

	// Allocate and zero memory for the context
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		pr_err("STMMAC_ENET_QOS: Memory allocation failed\n");
		return NULL;
	}

	ctx->dev = dev;

#ifdef CONFIG_STMMAC_ENET_QOS_RELAY
	// Try to bind a secondary relay device (e.g., eth1) if available
	ctx->relay_dev = dev_get_by_name(&init_net, "eth1");
	if (!ctx->relay_dev)
		pr_warn("STMMAC_ENET_QOS: relay device 'eth1' not found\n");
#endif

	qos_ctx = ctx;
	pr_info("STMMAC_ENET_QOS: registered on %s\n", dev->name);

	if (!stmmac_qos_debugfs_ready) {
		stmmac_enet_qos_debugfs_init();
		stmmac_qos_debugfs_ready = true;
	}

	return ctx;
}
EXPORT_SYMBOL(stmmac_enet_qos_register);

// Unregister and free the adapter context
void stmmac_enet_qos_unregister(struct stmmac_enet_qos_ctx *ctx)
{
	if (!ctx)
		return;

#ifdef CONFIG_STMMAC_ENET_QOS_RELAY
	if (ctx->relay_dev)
		dev_put(ctx->relay_dev);  // Release relay device reference
#endif

	pr_info("STMMAC_ENET_QOS: unregistered from %s\n", ctx->dev->name);
	kfree(ctx);  // Free memory

	if (stmmac_qos_debugfs_ready) {
		debugfs_remove_recursive(qos_debugfs_root);
		stmmac_qos_debugfs_ready = false;
	}
}
EXPORT_SYMBOL(stmmac_enet_qos_unregister);

// Check if skb contains an AVTP packet
bool stmmac_enet_qos_is_avtp(struct sk_buff *skb)
{
	struct ethhdr *eth;

	if (!skb)
		return false;

	eth = eth_hdr(skb);	   // Extract Ethernet header
	if (!eth)
		return false;

	// Compare EtherType to AVTP
	return (ntohs(eth->h_proto) == ETH_P_AVTP);
}

// Handle AVTP packet - log or relay, and drop original
void stmmac_enet_qos_handle_tx(struct stmmac_enet_qos_ctx *ctx, struct sk_buff *skb)
{
	struct ethhdr *eth;

	if (!ctx || !skb)
		return;

	eth = eth_hdr(skb);

	// Log info about the AVTP packet
	pr_info("STMMAC_ENET_QOS: AVTP TX detected (len=%u, src=%pM, dst=%pM)\n",
	        skb->len, eth->h_source, eth->h_dest);

#ifdef CONFIG_STMMAC_ENET_QOS_RELAY
	// Clone and forward the packet to relay_dev if available and up
	if (ctx->relay_dev && netif_running(ctx->relay_dev)) {
		struct sk_buff *skb_clone = skb_copy(skb, GFP_ATOMIC);
		if (!skb_clone) {
			pr_err("STMMAC_ENET_QOS: TX relay clone failed\n");
			goto drop;
		}

		// Set the relay interface as the target
		skb_clone->dev = ctx->relay_dev;
		skb_clone->protocol = eth_type_trans(skb_clone, ctx->relay_dev);
		skb_reset_network_header(skb_clone);
		skb_reset_mac_header(skb_clone);

		if (dev_queue_xmit(skb_clone) != NET_XMIT_SUCCESS) {
			pr_err("STMMAC_ENET_QOS: TX relay failed\n");
			kfree_skb(skb_clone);
		}
        else {
			pr_info("STMMAC_ENET_QOS: relayed TX AVTP packet to %s\n",
			        ctx->relay_dev->name);
		}
	}
drop:
#endif

	// Drop the original TX skb as we already handled it
	kfree_skb(skb);
}

// Handle AVTP packets during reception (RX)
void stmmac_enet_qos_handle_rx(struct stmmac_enet_qos_ctx *ctx, struct sk_buff *skb)
{
	struct ethhdr *eth;

	if (!ctx || !skb)
		return;

	eth = eth_hdr(skb);

	// Log details about the AVTP RX packet
	pr_info("STMMAC_ENET_QOS: AVTP RX detected (len=%u, src=%pM, dst=%pM)\n",
	        skb->len, eth->h_source, eth->h_dest);

#ifdef CONFIG_STMMAC_ENET_QOS_RELAY
	// Forward AVTP RX packet to relay device if present and running
	if (ctx->relay_dev && netif_running(ctx->relay_dev)) {
		struct sk_buff *skb_clone = skb_copy(skb, GFP_ATOMIC);
		if (!skb_clone) {
			pr_err("STMMAC_ENET_QOS: RX relay clone failed\n");
			goto drop;
		}

		// Assign the relay device as the new target
		skb_clone->dev = ctx->relay_dev;
		skb_clone->protocol = eth_type_trans(skb_clone, ctx->relay_dev);
		skb_reset_network_header(skb_clone);
		skb_reset_mac_header(skb_clone);

		// Transmit the relayed packet
		if (dev_queue_xmit(skb_clone) != NET_XMIT_SUCCESS) {
			pr_err("STMMAC_ENET_QOS: RX relay failed\n");
			kfree_skb(skb_clone);
		} else {
			pr_info("STMMAC_ENET_QOS: relayed RX AVTP to %s\n",
			        ctx->relay_dev->name);
		}
	}
drop:
#endif

	// Drop the original RX skb
	kfree_skb(skb);
}
