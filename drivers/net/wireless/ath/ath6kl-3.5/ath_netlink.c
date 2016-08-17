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
#include "linux/if.h"
#include "linux/socket.h"
#include "linux/netlink.h"
#include <net/sock.h>

#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/cache.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include "ath_netlink.h"
#include "rttapi.h"
#include "core.h"
#include "debug.h"
#define MAX_PAYLOAD 1024

static struct sock *ath_nl_sock;
static u32 gpid;

void ath_netlink_send(char *event_data, u32 event_datalen)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh;

	skb = nlmsg_new(NLMSG_SPACE(event_datalen), GFP_ATOMIC);
	if (!skb) {
		ath6kl_err("%s: No memory,\n", __func__);
		return;
	}

	nlh = nlmsg_put(skb, gpid, 0, 0 , NLMSG_SPACE(event_datalen), 0);
	if (!nlh) {
		ath6kl_err("%s: nlmsg_put() failed\n", __func__);
		return;
	}

	memcpy(NLMSG_DATA(nlh), event_data, event_datalen);

#ifdef ATH6KL_SUPPORT_NETLINK_KERNEL3_7
	NETLINK_CB(skb).portid = 0;        /* from kernel */
#else
	NETLINK_CB(skb).pid = 0;        /* from kernel */
#endif
	NETLINK_CB(skb).dst_group = 0;  /* unicast */
	netlink_unicast(ath_nl_sock, skb, gpid, MSG_DONTWAIT);
}

/* --adhoc_netlink_data_ready--
 * Stub function that does nothing */
void ath_netlink_reply(int pid, int seq, void *payload)
{
	return;
}

/* Receive messages from netlink socket. */
#ifdef CE_OLD_KERNEL_SUPPORT_2_6_23
static void ath_netlink_receive(struct sk_buff *__skb, int len)
#else
static void ath_netlink_receive(struct sk_buff *__skb)
#endif
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	u_int8_t *data = NULL;
	u_int32_t uid, pid, seq;

	skb = skb_get(__skb);
	if (skb) {
		/* process netlink message pointed by skb->data */
		nlh = (struct nlmsghdr *)skb->data;
		pid = NETLINK_CREDS(skb)->pid;
		pid = nlh->nlmsg_pid;
		uid = NETLINK_CREDS(skb)->uid;
		seq = nlh->nlmsg_seq;
		data = NLMSG_DATA(nlh);

		gpid = pid;
		ath_netlink_reply(pid, seq, data);
		rttm_issue_request(data, nlmsg_len(nlh));
		kfree_skb(skb);
	}
	return ;
}

int ath_netlink_init()
{
#ifdef ATH6KL_SUPPORT_NETLINK_KERNEL3_7
	struct netlink_kernel_cfg netlink_cfg;

	if (ath_nl_sock == NULL) {
		netlink_cfg.groups = 1;
		netlink_cfg.input = ath_netlink_receive;
		netlink_cfg.cb_mutex = NULL;
		netlink_cfg.bind = NULL;
		netlink_cfg.flags = 0;

		ath_nl_sock = (struct sock *)netlink_kernel_create(
					&init_net, NETLINK_ATH_EVENT,
					&netlink_cfg);
		if (ath_nl_sock == NULL) {
			ath6kl_err("%s NetLink Create Failed\n", __func__);
			return -ENODEV;
		}
	}
#elif defined(ATH6KL_SUPPORT_NETLINK_KERNEL3_6)
	struct netlink_kernel_cfg netlink_cfg;

	if (ath_nl_sock == NULL) {
		netlink_cfg.groups = 1;
		netlink_cfg.input = ath_netlink_receive;
		netlink_cfg.cb_mutex = NULL;
		netlink_cfg.bind = NULL;

		ath_nl_sock = (struct sock *)netlink_kernel_create(
					&init_net, NETLINK_ATH_EVENT,
					THIS_MODULE,
					&netlink_cfg);
		if (ath_nl_sock == NULL) {
			ath6kl_err("%s NetLink Create Failed\n", __func__);
			return -ENODEV;
		}
	}
#else
	if (ath_nl_sock == NULL) {
#ifdef CE_OLD_KERNEL_SUPPORT_2_6_23
		ath_nl_sock = (struct sock *)netlink_kernel_create(
					NETLINK_ATH_EVENT,
					1, &ath_netlink_receive, NULL,
					THIS_MODULE);
#else
		ath_nl_sock = (struct sock *)netlink_kernel_create(
					&init_net, NETLINK_ATH_EVENT,
					1, &ath_netlink_receive, NULL,
					THIS_MODULE);
#endif
		if (ath_nl_sock == NULL) {
			ath6kl_err("%s NetLink Create Failed\n", __func__);
			return -ENODEV;
		}
	}
#endif

	return 0;
}

int ath_netlink_free(void)
{
	if (ath_nl_sock) {
#ifdef CE_OLD_KERNEL_SUPPORT_2_6_23
		sock_release(ath_nl_sock->sk_socket);
#else
		netlink_kernel_release(ath_nl_sock);
#endif
		ath_nl_sock = NULL;
	}

	return 0;
}

static struct sock *ath_nlapi_sock;
struct athnl_ap_status ath_ap_status;

int ath_nlapi_socket_init(void (*r)(struct sk_buff *skb))
{
#ifdef ATH6KL_SUPPORT_NETLINK_KERNEL3_7
        struct netlink_kernel_cfg netlink_cfg;

        if (ath_nlapi_sock == NULL) {
                netlink_cfg.groups = 1;
                netlink_cfg.input = *r;
                netlink_cfg.cb_mutex = NULL;
                netlink_cfg.bind = NULL;
                netlink_cfg.flags = 0;

                ath_nlapi_sock = (struct sock *)netlink_kernel_create(
                                        &init_net, NETLINK_ATH_API_EVENT,
                                        &netlink_cfg);
                if (ath_nlapi_sock == NULL) {
                        ath6kl_err("%s NetLink Create Failed\n", __func__);
                        return -ENODEV;
                }
        }
#elif defined(ATH6KL_SUPPORT_NETLINK_KERNEL3_6)
        struct netlink_kernel_cfg netlink_cfg;

        if (ath_nlapi_sock == NULL) {
                netlink_cfg.groups = 1;
                netlink_cfg.input = *r;
                netlink_cfg.cb_mutex = NULL;
                netlink_cfg.bind = NULL;

                ath_nlapi_sock = (struct sock *)netlink_kernel_create(
                                        &init_net, NETLINK_ATH_API_EVENT,
                                        THIS_MODULE,
                                        &netlink_cfg);
                if (ath_nlapi_sock == NULL) {
                        ath6kl_err("%s NetLink Create Failed\n", __func__);
                        return -ENODEV;
                }
        }
#else
	if (ath_nlapi_sock == NULL) {
#ifdef CE_OLD_KERNEL_SUPPORT_2_6_23
                ath_nlapi_sock = (struct sock *)netlink_kernel_create(
                                        NETLINK_ATH_API_EVENT,
                                        1, r, NULL,
                                        THIS_MODULE);
#else
		ath_nlapi_sock = (struct sock *)netlink_kernel_create(
					&init_net, NETLINK_ATH_API_EVENT,
					1, r, NULL,
					THIS_MODULE);
#endif
		if (ath_nlapi_sock == NULL) {
			ath6kl_err("%s NetLink Create Failed\n", __func__);
			return -ENODEV;
		}
	}
#endif
	return 0;
}

void ath_nlapi_socket_send(u32 pid, char *event_data, u32 event_datalen)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh;

	skb = nlmsg_new(NLMSG_SPACE(event_datalen), GFP_ATOMIC);
	if (!skb) {
		ath6kl_err("%s: No memory,\n", __func__);
		return;
	}

	nlh = nlmsg_put(skb, pid, 0, 0 , NLMSG_SPACE(event_datalen), 0);
	if (!nlh) {
		ath6kl_err("%s: nlmsg_put() failed\n", __func__);
		return;
	}

	memcpy(NLMSG_DATA(nlh), event_data, event_datalen);

#ifdef ATH6KL_SUPPORT_NETLINK_KERNEL3_7
        NETLINK_CB(skb).portid = 0;        /* from kernel */
#else
        NETLINK_CB(skb).pid = 0;        /* from kernel */
#endif
	NETLINK_CB(skb).dst_group = 0;  /* unicast */
	netlink_unicast(ath_nlapi_sock, skb, pid, MSG_DONTWAIT);
}

int ath_nlapi_socket_free(void)
{
	if (ath_nlapi_sock) {
		netlink_kernel_release(ath_nlapi_sock);
		ath_nlapi_sock = NULL;
	}

	return 0;
}

static struct ath6kl *ath_nlapi_ar;
static u32 ath_nlapi_pid;
static struct ath6kl_vif *ath_nlapi_get_vif_by_name(char *vif_name)
{
	struct ath6kl_vif *vif, *vif_tmp;
	bool vif_found = false;

	spin_lock_bh(&ath_nlapi_ar->list_lock);

	if (list_empty(&ath_nlapi_ar->vif_list)) {
		spin_unlock_bh(&ath_nlapi_ar->list_lock);
		return NULL;
	}

	list_for_each_entry_safe(vif, vif_tmp, &ath_nlapi_ar->vif_list, list) {
		if (!strncmp(vif->ndev->name, vif_name, IFNAMSIZ)) {
			vif_found = true;
			break;
		}
	}

	spin_unlock_bh(&ath_nlapi_ar->list_lock);

	return (vif_found) ? vif : NULL;
}

/* Receive messages from netlink socket. */
static void ath_nlapi_receive(struct sk_buff *__skb)
{
	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh = NULL;
	u_int8_t *data = NULL;
	u_int32_t uid, pid, seq;
	struct athnl_parameters *param;
	struct ath6kl_vif *vif;
	struct target_stats *stats;

	if (__skb == NULL)
		return;

	skb = skb_get(__skb);

	/* process netlink message pointed by skb->data */
	nlh = (struct nlmsghdr *)skb->data;
	pid = NETLINK_CREDS(skb)->pid;
	pid = nlh->nlmsg_pid;
	uid = NETLINK_CREDS(skb)->uid;
	seq = nlh->nlmsg_seq;
	data = NLMSG_DATA(nlh);
	ath_nlapi_pid = pid;
	param = (struct athnl_parameters *)data;
	vif = ath_nlapi_get_vif_by_name(param->vif_name);

	if (vif == NULL) {
		ath6kl_warn("%s vif with name %s is not found\n", __func__, param->vif_name);
		kfree_skb(skb);
		return;
	}

	ath6kl_wmi_get_stats_cmd(vif->ar->wmi, vif->fw_vif_idx);
	stats = &vif->target_stats;
	switch (nlh->nlmsg_type) {
		case NETLINK_ATH_API_CMD_GET_TX_RATE:
			ath_nlapi_socket_send(ath_nlapi_pid, &ath_ap_status.tx_ucast_rate, sizeof(s32));
			break;
		case NETLINK_ATH_API_CMD_GET_TX_RETRY_RATIO:
			ath_nlapi_socket_send(ath_nlapi_pid, &ath_ap_status.tx_retry_ratio, sizeof(s16));
			break;
		case NETLINK_ATH_API_CMD_GET_FIFO_DEPTH:
			ath_nlapi_socket_send(ath_nlapi_pid, &ath_ap_status.tx_total_fifo_depth, sizeof(s16));
			break;
	}

	kfree_skb(skb);
	return;
}

int ath_nlapi_init(void *ar)
{
	ath_nlapi_ar = (struct ath6kl *)ar;
	return ath_nlapi_socket_init(ath_nlapi_receive);
}

int ath_nlapi_free(void)
{
	return ath_nlapi_socket_free();
}
