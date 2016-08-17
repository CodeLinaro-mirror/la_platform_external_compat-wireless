
/*
 * Copyright (c) 2011 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef _ATH_NETLINK_H_
#define _ATH_NETLINK_H_
#include "linux/if.h"
#include "linux/socket.h"
#include "linux/netlink.h"
#define NETLINK_ATH_EVENT	(NETLINK_GENERIC + 4)
#define NETLINK_ATH_API_EVENT	(NETLINK_GENERIC + 6)

#define MAC_ADDR_LEN           6

#define NETLINK_ATH_API_CMD_GET_TX_RATE                 1
#define NETLINK_ATH_API_CMD_GET_TX_RETRY_RATIO          2
#define NETLINK_ATH_API_CMD_GET_FIFO_DEPTH              3

struct athnl_parameters {
	char vif_name[16];
};

struct athnl_ap_status {
	u32 tx_ucast_rate;
	u16 tx_retry_ratio;
	u16 tx_total_fifo_depth;
};

int ath_netlink_init(void);
int ath_netlink_delete(void);
void ath_netlink_send(char *event_data, u32 event_datalen);
int ath_netlink_free(void);

int ath_nlapi_socket_init(void (*recv)(struct sk_buff *skb));
void ath_nlapi_socket_send(u32 pid, char *event_data, u32 event_datalen);
int ath_nlapi_socket_free(void);
int ath_nlapi_init(void *ar);
int ath_nlapi_free(void);

#endif /* _ATH_NETLINK_H_ */
