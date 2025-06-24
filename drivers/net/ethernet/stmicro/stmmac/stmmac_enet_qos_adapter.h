#ifndef STMMAC_ENET_QOS_ADAPTER_H
#define STMMAC_ENET_QOS_ADAPTER_H

#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>

/* Forward declaration of context structure */
struct stmmac_enet_qos_ctx;

/**
 * stmmac_enet_qos_register - Initialize QOS adapter for a given net_device
 * @dev: Pointer to the network device (e.g., eth0)
 * Return: Pointer to adapter context, or NULL on failure
 */
struct stmmac_enet_qos_ctx *stmmac_enet_qos_register(struct net_device *dev);

/**
 * stmmac_enet_qos_unregister - Clean up QOS adapter
 * @ctx: Pointer to adapter context
 */
void stmmac_enet_qos_unregister(struct stmmac_enet_qos_ctx *ctx);

/**
 * qos_adapter_is_avtp - Check if skb is an AVTP packet (EtherType 0x22F0)
 * @skb: Pointer to the socket buffer
 * Return: true if it's an AVTP packet, false otherwise
 */
bool stmmac_enet_qos_is_avtp(struct sk_buff *skb);

/**
 * stmmac_enet_qos_handle_tx - Handle or relay AVTP TX packets
 * @ctx: Pointer to QOS adapter context
 * @skb: Packet to handle (will be freed here)
 */
void stmmac_enet_qos_handle_tx(struct stmmac_enet_qos_ctx *ctx, struct sk_buff *skb);

/**
 * stmmac_enet_qos_handle_rx - Handle or relay AVTP RX packets
 * @ctx: Pointer to QOS adapter context
 * @skb: Received packet to process (will be freed here)
 */
void stmmac_enet_qos_handle_rx(struct stmmac_enet_qos_ctx *ctx, struct sk_buff *skb);

/**
 * stmmac_enet_qos_debugfs_init - Create DebugFS interface (called internally)
 */
void stmmac_enet_qos_debugfs_init(void);

#endif /* __STMMAC_QOS_ADAPTER_H__ */
