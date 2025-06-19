#include <linux/module.h>            // For kernel module macros
#include <linux/netdevice.h>         // For net_device
#include <linux/skbuff.h>            // For sk_buff
#include <linux/etherdevice.h>       // For Ethernet helpers
#include <linux/slab.h>              // For kzalloc/kfree
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

// Register the adapter for the given device
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
	// Try to find "eth1" for AVTP packet forwarding
	ctx->relay_dev = dev_get_by_name(&init_net, "eth1");
	if (!ctx->relay_dev)
		pr_warn("QOS_ADAPTER: relay device 'eth1' not found\n");
#endif

	pr_info("QOS_ADAPTER: registered on %s\n", dev->name);
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
}

// Check if skb contains an AVTP packet
bool qos_adapter_is_avtp(struct sk_buff *skb)
{
	struct ethhdr *eth;

	if (!skb)
		return false;

	eth = eth_hdr(skb);
	if (!eth)
		return false;

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
	// Relay to eth1 if available
	if (ctx->relay_dev && netif_running(ctx->relay_dev)) {
		struct sk_buff *skb_clone = skb_copy(skb, GFP_ATOMIC);
		if (!skb_clone) {
			pr_err("QOS_ADAPTER: failed to clone skb\n");
			goto drop;
		}

		skb_clone->dev = ctx->relay_dev;
		skb_clone->protocol = eth_type_trans(skb_clone, ctx->relay_dev);
		skb_reset_network_header(skb_clone);
		skb_reset_mac_header(skb_clone);

		if (dev_queue_xmit(skb_clone) != NET_XMIT_SUCCESS) {
			pr_err("QOS_ADAPTER: relay failed\n");
			kfree_skb(skb_clone);
		}
        else {
			pr_info("QOS_ADAPTER: relayed AVTP packet to %s\n",
			        ctx->relay_dev->name);
		}
	}
drop:
#endif

	// Drop the original packet (already handled)
	kfree_skb(skb);
}
