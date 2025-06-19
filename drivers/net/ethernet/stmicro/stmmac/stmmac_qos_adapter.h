#ifndef __QOS_ADAPTER_H__
#define __QOS_ADAPTER_H__

#include <stdbool.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>

/* Forward declaration of context structure */
struct qos_adapter_context;

/**
 * qos_adapter_register - Initialize QOS adapter for a given net_device
 * @dev: Pointer to the network device (e.g., eth0)
 * Return: Pointer to adapter context, or NULL on failure
 */
struct qos_adapter_context *qos_adapter_register(struct net_device *dev);

/**
 * qos_adapter_unregister - Clean up QOS adapter
 * @ctx: Pointer to adapter context
 */
void qos_adapter_unregister(struct qos_adapter_context *ctx);

/**
 * qos_adapter_is_avtp - Check if skb is an AVTP packet (EtherType 0x22F0)
 * @skb: Pointer to the socket buffer
 * Return: true if it's an AVTP packet, false otherwise
 */
bool qos_adapter_is_avtp(struct sk_buff *skb);

/**
 * qos_adapter_handle_tx - Handle or relay AVTP TX packets
 * @ctx: Pointer to QOS adapter context
 * @skb: Packet to handle (will be freed here)
 */
void qos_adapter_handle_tx(struct qos_adapter_context *ctx, struct sk_buff *skb);

#endif /* __QOS_ADAPTER_H__ */
