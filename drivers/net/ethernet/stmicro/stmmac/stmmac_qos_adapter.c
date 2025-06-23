#include <linux/module.h>            // For kernel module macros
#include <linux/netdevice.h>         // For net_device
#include <linux/skbuff.h>            // For sk_buff
#include <linux/etherdevice.h>       // For Ethernet helpers
#include <linux/slab.h>              // For kzalloc/kfree
#include <linux/debugfs.h>         // For debugfs interface
#include "stmmac_qos_adapter.h"      // QOS Adapter header

// AVTP EtherType definition
#define ETH_P_AVTP 0x22F0

// Adapter context structure
struct qos_adapter_context {
	struct net_device *dev;          // Interface being monitored

#ifdef CONFIG_QOS_RELAY
	struct net_device *relay_dev;    // Optional forwarding interface (e.g., eth1)
#endif
};

// Global debugfs root directory
static struct dentry *qos_debugfs_root;
static bool debugfs_initialized = false;

// Register the adapter on the specified net_device
struct qos_adapter_context *qos_adapter_register(struct net_device *dev)
{
	struct qos_adapter_context *ctx;

	if (!dev) {
		pr_err("QOS_ADAPTER: Invalid device\n");
		return NULL;
	}

	// Allocate and zero memory for the context
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		pr_err("QOS_ADAPTER: Memory allocation failed\n");
		return NULL;
	}

	ctx->dev = dev;

#ifdef CONFIG_QOS_RELAY
	// Try to bind a secondary relay device (e.g., eth1) if available
	ctx->relay_dev = dev_get_by_name(&init_net, "eth1");
	if (!ctx->relay_dev)
		pr_warn("QOS_ADAPTER: relay device 'eth1' not found\n");
#endif

	pr_info("QOS_ADAPTER: registered on %s\n", dev->name);

	if (!debugfs_initialized) {
		qos_adapter_debugfs_init();
		debugfs_initialized = true;
	}

	return ctx;
}

// Unregister and free the adapter context
void qos_adapter_unregister(struct qos_adapter_context *ctx)
{
	if (!ctx)
		return;

#ifdef CONFIG_QOS_RELAY
	if (ctx->relay_dev)
		dev_put(ctx->relay_dev);  // Release relay device reference
#endif

	pr_info("QOS_ADAPTER: unregistered from %s\n", ctx->dev->name);
	kfree(ctx);  // Free memory

	if (debugfs_initialized) {
		debugfs_remove_recursive(qos_debugfs_root);
		debugfs_initialized = false;
	}
}

// Check if skb contains an AVTP packet
bool qos_adapter_is_avtp(struct sk_buff *skb)
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
void qos_adapter_handle_tx(struct qos_adapter_context *ctx, struct sk_buff *skb)
{
	struct ethhdr *eth;

	if (!ctx || !skb)
		return;

	eth = eth_hdr(skb);

	// Log info about the AVTP packet
	pr_info("QOS_ADAPTER: AVTP TX detected (len=%u, src=%pM, dst=%pM)\n",
	        skb->len, eth->h_source, eth->h_dest);

#ifdef CONFIG_QOS_RELAY
	// Clone and forward the packet to relay_dev if available and up
	if (ctx->relay_dev && netif_running(ctx->relay_dev)) {
		struct sk_buff *skb_clone = skb_copy(skb, GFP_ATOMIC);
		if (!skb_clone) {
			pr_err("QOS_ADAPTER: TX relay clone failed\n");
			goto drop;
		}

		// Set the relay interface as the target
		skb_clone->dev = ctx->relay_dev;
		skb_clone->protocol = eth_type_trans(skb_clone, ctx->relay_dev);
		skb_reset_network_header(skb_clone);
		skb_reset_mac_header(skb_clone);

		if (dev_queue_xmit(skb_clone) != NET_XMIT_SUCCESS) {
			pr_err("QOS_ADAPTER: TX relay failed\n");
			kfree_skb(skb_clone);
		}
        else {
			pr_info("QOS_ADAPTER: relayed TX AVTP packet to %s\n",
			        ctx->relay_dev->name);
		}
	}
drop:
#endif

	// Drop the original TX skb as we already handled it
	kfree_skb(skb);
}

// Handle AVTP packets during reception (RX)
void qos_adapter_handle_rx(struct qos_adapter_context *ctx, struct sk_buff *skb)
{
	struct ethhdr *eth;

	if (!ctx || !skb)
		return;

	eth = eth_hdr(skb);

	// Log details about the AVTP RX packet
	pr_info("QOS_ADAPTER: AVTP RX detected (len=%u, src=%pM, dst=%pM)\n",
	        skb->len, eth->h_source, eth->h_dest);

#ifdef CONFIG_QOS_RELAY
	// Forward AVTP RX packet to relay device if present and running
	if (ctx->relay_dev && netif_running(ctx->relay_dev)) {
		struct sk_buff *skb_clone = skb_copy(skb, GFP_ATOMIC);
		if (!skb_clone) {
			pr_err("QOS_ADAPTER: RX relay clone failed\n");
			goto drop;
		}

		// Assign the relay device as the new target
		skb_clone->dev = ctx->relay_dev;
		skb_clone->protocol = eth_type_trans(skb_clone, ctx->relay_dev);
		skb_reset_network_header(skb_clone);
		skb_reset_mac_header(skb_clone);

		// Transmit the relayed packet
		if (dev_queue_xmit(skb_clone) != NET_XMIT_SUCCESS) {
			pr_err("QOS_ADAPTER: RX relay failed\n");
			kfree_skb(skb_clone);
		} else {
			pr_info("QOS_ADAPTER: relayed RX AVTP to %s\n",
			        ctx->relay_dev->name);
		}
	}
drop:
#endif

	// Drop the original RX skb
	kfree_skb(skb);
}

/* ============================ */
/*        DebugFS support       */
/* ============================ */

/**
 * qos_adapter_run_selftests - Simple placeholder self-test logic
 */
static void qos_adapter_run_selftests(void)
{
	pr_info("QOS_ADAPTER: Running self-tests...\n");
	pr_info("QOS_ADAPTER: Self-tests completed.\n");
}

/**
 * run_selftest_write - DebugFS callback: triggers self-tests on write
 */
static ssize_t run_selftest_write(struct file *file, const char __user *buf,
                                  size_t count, loff_t *ppos)
{
	qos_adapter_run_selftests();
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
static void qos_adapter_debugfs_init(void)
{
	qos_debugfs_root = debugfs_create_dir("qos_adapter", NULL);
	if (!qos_debugfs_root || IS_ERR(qos_debugfs_root)) {
		pr_warn("QOS_ADAPTER: Failed to create debugfs directory\n");
		return;
	}

	debugfs_create_file("run_selftest", 0200, qos_debugfs_root,
	                    NULL, &run_selftest_fops);
}
