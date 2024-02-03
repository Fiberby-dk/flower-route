// SPDX-License-Identifier: GPL-2.0-or-later

#include "nl_common.h"

#include <libmnl/libmnl.h>
#include <errno.h>
#include <stdio.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "nl_send.h"
#include "nl_filter.h"

struct rb_root chain_tree = RB_ROOT;

void filter_dump(EV_P_ struct conn *c)
{
	struct nlmsghdr *nlh;
	char buf[MNL_SOCKET_DUMP_SIZE];
	struct tcmsg *tcm;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_GETQDISC;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct tcmsg));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = config->ifidx; /* this is ignored by kernel */
	tcm->tcm_handle = 0;
	tcm->tcm_parent = 0;

	nl_send_req(EV_A_ c, nlh);
}

void filter_dump_chains(EV_P_ struct conn *c)
{
	struct nlmsghdr *nlh;
	char buf[MNL_SOCKET_DUMP_SIZE];
	struct tcmsg *tcm;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_GETCHAIN;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct tcmsg));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = config->ifidx;
	tcm->tcm_handle = 0;
	tcm->tcm_parent = TC_H_MAKE(TC_H_CLSACT, TC_H_MIN_INGRESS);

	nl_send_req(EV_A_ c, nlh);
}

void filter_dump_chain(EV_P_ struct conn *c, uint32_t chain_no)
{
	struct nlmsghdr *nlh;
	char buf[MNL_SOCKET_DUMP_SIZE];
	struct tcmsg *tcm;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_GETTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct tcmsg));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = config->ifidx;
	tcm->tcm_handle = 0;
	tcm->tcm_parent = TC_H_MAKE(TC_H_CLSACT, TC_H_MIN_INGRESS);

	mnl_attr_put_u32(nlh, TCA_CHAIN, chain_no);

	nl_send_req(EV_A_ c, nlh);
}

static int u32cmp(const uint32_t a, const uint32_t b)
{
	return ((int) a) - b;
}

struct chain *chain_lookup(const uint32_t chain_no)
{
	struct rb_node *node = chain_tree.rb_node;
	struct chain *this;
	int ret;

	while (node) {
		this = rb_container_of(node, struct chain, node);
		ret = u32cmp(chain_no, this->chain_no);
		if (ret < 0)
			node = node->rb_left;
		else if (ret > 0)
			node = node->rb_right;
		else
			return this;
	}
	return NULL;
}

static int chain_insert(struct chain *ch)
{
	struct rb_node **new = &(chain_tree.rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct chain *this = rb_container_of(*new, struct chain, node);
		int ret = u32cmp(ch->chain_no, this->chain_no);

		parent = *new;
		if (ret < 0)
			new = &((*new)->rb_left);
		else if (ret > 0)
			new = &((*new)->rb_right);
		else
			return 0;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&ch->node, parent, new);
	rb_insert_color(&ch->node, &chain_tree);

	return 1;
}

void filter_got_qdisc(void)
{
	fr_printf(DEBUG1, "got qdisc\n");
	/* TODO set flag, and test for it in the completion callback */
}

void filter_got_chain(uint32_t chain_no)
{
	struct chain *ch = chain_lookup(chain_no);
	int ret;

	if (ch == NULL) {
		ch = fr_malloc(sizeof(struct chain));
		ch->chain_no = chain_no;
		ret = chain_insert(ch);
		AN(ret == 1);
	}
	ch->state = CHAIN_STATE_PRESENT;
}

static void filter_reserve_chain(const uint32_t chain_no)
{
	struct chain *ch;

	filter_got_chain(chain_no);

	ch = chain_lookup(chain_no);
	AN(ch);
	ch->state = CHAIN_STATE_RESERVED;
	fr_printf(INFO, "chain: %d\tstate: %d\n", chain_no, ch->state);
}

uint32_t filter_find_available_chain_no(const uint32_t min_chain_no)
{
	uint32_t ret = 0;

	for (struct rb_node *n = rb_first(&chain_tree); n; n = rb_next(n)) {
		struct chain *ch = rb_container_of(n, struct chain, node);

		if (ch->chain_no == ret || ret < min_chain_no)
			ret = ch->chain_no + 1;
		else if (ret >= min_chain_no)
			break;
	}
	if (ret < min_chain_no)
		ret = min_chain_no;
	filter_reserve_chain(ret);
	return ret;
}

void filter_clear_chains(void)
{
	for (struct rb_node *n = rb_first(&chain_tree), *nn; n; n = nn) {
		struct chain *ch = rb_container_of(n, struct chain, node);

		nn = rb_next(n);
		rb_erase(&ch->node, &chain_tree);
		free(ch);
	}
}
