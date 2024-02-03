// SPDX-License-Identifier: GPL-2.0-or-later

#include "nl_common.h"
#include "tc_encode.h"

#include <linux/tc_act/tc_gact.h>
#include <linux/tc_act/tc_vlan.h>
#include <linux/tc_act/tc_pedit.h>
#include <linux/tc_act/tc_csum.h>
#include <linux/tc_act/tc_mirred.h>
#include <linux/if_ether.h>

static void tce_set_tcm(struct nlmsghdr *nlh, uint32_t info, int flags)
{
	struct tcmsg *tcm;

	tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct tcmsg));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = config->ifidx;
	tcm->tcm_handle = (flags & TCE_FLAG_LOOPBACK) != 0;
	tcm->tcm_parent = TC_H_MAKE(TC_H_CLSACT, TC_H_MIN_INGRESS);
	tcm->tcm_info = info;
}

static void tce_flower_set_eth_type(struct nlmsghdr *nlh, uint8_t af, int flags)
{
	uint16_t vlan_eth_type;

	switch (af) {
	case AF_INET:
		vlan_eth_type = ETH_P_IP;
		break;
	case AF_INET6:
		vlan_eth_type = ETH_P_IPV6;
		break;
	default:
		AN(false);
		break;
	}
	mnl_attr_put_u16(nlh, TCA_FLOWER_KEY_VLAN_ETH_TYPE, htons(vlan_eth_type));

	/*
	 * For normal operation, we set this as ETH_P_8021Q, but it is dumped back as IPv4/6
	 * For loopback operation (aka. testing), we encode it as we would get it from the kernel
	 */
	uint16_t eth_type = flags & TCE_FLAG_LOOPBACK ? vlan_eth_type : ETH_P_8021Q;

	mnl_attr_put_u16(nlh, TCA_FLOWER_KEY_ETH_TYPE, htons(eth_type));
}

static struct nlattr *tce_new_flower_rule(struct nlmsghdr *nlh, const struct tc_rule *tcr, int flags)
{
	uint32_t flower_flags = tcr->flower_flags;
	struct nlattr *flower;

	mnl_attr_put_strz(nlh, TCA_KIND, "flower");
	flower = mnl_attr_nest_start(nlh, TCA_OPTIONS);
	AN(flower);

	if ((flags & TCE_FLAG_LOOPBACK) == 0)
		flower_flags &= ~TCA_CLS_FLAGS_IN_HW;
	mnl_attr_put_u32(nlh, TCA_FLOWER_FLAGS, flower_flags);

	tce_flower_set_eth_type(nlh, tcr->af_addr.af, flags);
	return flower;
}

static void tce_action_start(struct nlmsghdr *nlh, struct nlattr **act, struct nlattr **act_opts, uint16_t act_no, const char *kind)
{
	*act = mnl_attr_nest_start(nlh, act_no);
	AN(*act);
	mnl_attr_put_strz(nlh, TCA_ACT_KIND, kind);
	//mnl_attr_put_u32(nlh, TCA_ACT_HW_STATS, TCA_ACT_HW_STATS_DELAYED);
	*act_opts = mnl_attr_nest_start(nlh, TCA_ACT_OPTIONS);
	AN(*act_opts);
}

static void tce_action_end(struct nlmsghdr *nlh, struct nlattr *act, struct nlattr *act_opts)
{
	mnl_attr_nest_end(nlh, act_opts);
	mnl_attr_nest_end(nlh, act);
}

static void tce_simple_gact(struct nlmsghdr *nlh, int action)
{
	struct nlattr *acts, *act, *act_opts;

	acts = mnl_attr_nest_start(nlh, TCA_FLOWER_ACT);
	AN(acts);
	tce_action_start(nlh, &act, &act_opts, 1, "gact");
	struct tc_gact p;

	memset(&p, '\0', sizeof(struct tc_gact));
	p.action = action;
	mnl_attr_put(nlh, TCA_GACT_PARMS, sizeof(struct tc_gact), &p);
	tce_action_end(nlh, act, act_opts);
	mnl_attr_nest_end(nlh, acts);
}

static void tce_add_tll_check_rule(struct nlmsghdr *nlh)
{
	mnl_attr_put_u8(nlh, TCA_FLOWER_KEY_IP_TTL, 0x01);
	mnl_attr_put_u8(nlh, TCA_FLOWER_KEY_IP_TTL_MASK, 0xff);
	tce_simple_gact(nlh, TC_ACT_TRAP);
}

static void tce_build_ipv6_mask(struct in6_addr *dst, const uint8_t mask_len)
{
	uint8_t rem = 128 - mask_len;

	for (int i = 15; i >= 0; i--) {
		if (rem >= 8) {
			dst->s6_addr[i] = 0x00;
			rem -= 8;
		} else {
			dst->s6_addr[i] = 0xff << rem;
			rem = 0;
		}
	}
}

static void tce_match_prefix(struct nlmsghdr *nlh, const struct tc_rule *tcr, int flags)
{
	const struct af_addr *pfx = &tcr->af_addr;
	const uint8_t mask_len = pfx->mask_len;

	switch (pfx->af) {
	case AF_INET:
		if ((flags & TCE_FLAG_LOOPBACK) && mask_len == 0)
			break;
		mnl_attr_put_u32(nlh, TCA_FLOWER_KEY_IPV4_DST, pfx->in.v4.s_addr);
		uint32_t ip4_mask = 0;

		if (mask_len > 0)
			ip4_mask = htonl(0xffffffff << (32-mask_len));
		mnl_attr_put_u32(nlh, TCA_FLOWER_KEY_IPV4_DST_MASK, ip4_mask);
		break;
	case AF_INET6:
		mnl_attr_put(nlh, TCA_FLOWER_KEY_IPV6_DST, sizeof(struct in6_addr), &pfx->in.v6);
		struct in6_addr ip6_mask;

		tce_build_ipv6_mask(&ip6_mask, mask_len);
		mnl_attr_put(nlh, TCA_FLOWER_KEY_IPV6_DST_MASK, sizeof(struct in6_addr), &ip6_mask);
		break;
	}
}

static void tce_add_ip_gact_rule(struct nlmsghdr *nlh, const struct tc_rule *tcr, int flags, int action)
{
	tce_match_prefix(nlh, tcr, flags);
	tce_simple_gact(nlh, action);
}

static void tce_modify_vlan_action(struct nlmsghdr *nlh, uint16_t act_no, uint32_t vlan_id)
{
	struct nlattr *act, *act_opts;

	tce_action_start(nlh, &act, &act_opts, act_no, "vlan");
	struct tc_vlan p;

	memset(&p, '\0', sizeof(struct tc_vlan));
	p.v_action = TCA_VLAN_ACT_MODIFY;
	p.action = TC_ACT_PIPE;
	mnl_attr_put(nlh, TCA_VLAN_PARMS, sizeof(struct tc_vlan), &p);
	mnl_attr_put_u16(nlh, TCA_VLAN_PUSH_VLAN_ID, vlan_id);
	tce_action_end(nlh, act, act_opts);
}

static void tce_pedit_action(struct nlmsghdr *nlh, uint16_t act_no, const struct tc_rule *tcr)
{
	struct nlattr *act, *act_opts, *ex_keys, *ex_key;

	tce_action_start(nlh, &act, &act_opts, act_no, "pedit");

	const size_t nkeys = 4;
	const size_t sel_sz = sizeof(struct tc_pedit_sel) + nkeys * sizeof(struct tc_pedit_key);
	struct tc_pedit_sel *sel = fr_malloc(sel_sz);

	sel->action = TC_ACT_PIPE;
	sel->nkeys = nkeys;
	ex_keys = mnl_attr_nest_start(nlh, TCA_PEDIT_KEYS_EX);
	AN(ex_keys);

	/* Set MAC addresses */
	for (int i = 0; i < 3; i++) {
		ex_key = mnl_attr_nest_start(nlh, TCA_PEDIT_KEY_EX);
		AN(ex_key);
		mnl_attr_put_u16(nlh, TCA_PEDIT_KEY_EX_CMD, TCA_PEDIT_KEY_EX_CMD_SET);
		mnl_attr_put_u16(nlh, TCA_PEDIT_KEY_EX_HTYPE, TCA_PEDIT_KEY_EX_HDR_TYPE_ETH);
		sel->keys[i].val = tcr->lladdr.raw[i];
		sel->keys[i].mask = 0x00000000;
		sel->keys[i].off = i<<2;
		mnl_attr_nest_end(nlh, ex_key);
	}

	/* Decrement TTL / hoplimit */
	ex_key = mnl_attr_nest_start(nlh, TCA_PEDIT_KEY_EX);
	AN(ex_key);
	mnl_attr_put_u16(nlh, TCA_PEDIT_KEY_EX_CMD, TCA_PEDIT_KEY_EX_CMD_ADD);
	switch (tcr->af_addr.af) {
	case AF_INET:
		mnl_attr_put_u16(nlh, TCA_PEDIT_KEY_EX_HTYPE, TCA_PEDIT_KEY_EX_HDR_TYPE_IP4);
		sel->keys[3].val = htonl(0xff000000);
		sel->keys[3].mask = htonl(0x00ffffff);
		sel->keys[3].off = 8;
		break;
	case AF_INET6:
		mnl_attr_put_u16(nlh, TCA_PEDIT_KEY_EX_HTYPE, TCA_PEDIT_KEY_EX_HDR_TYPE_IP6);
		sel->keys[3].val = htonl(0x000000ff);
		sel->keys[3].mask = htonl(0xffffff00);
		sel->keys[3].off = 4;
		break;
	}
	mnl_attr_nest_end(nlh, ex_key);

	mnl_attr_nest_end(nlh, ex_keys);
	mnl_attr_put(nlh, TCA_PEDIT_PARMS_EX, sel_sz, sel);
	tce_action_end(nlh, act, act_opts);
	free(sel);
}

static void tce_redirect_action(struct nlmsghdr *nlh, uint16_t act_no)
{
	struct nlattr *act, *act_opts;

	tce_action_start(nlh, &act, &act_opts, act_no, "mirred");
	struct tc_mirred p;

	memset(&p, '\0', sizeof(struct tc_mirred));
	p.eaction = TCA_EGRESS_REDIR;
	p.action = TC_ACT_STOLEN;
	p.ifindex = config->ifidx;
	mnl_attr_put(nlh, TCA_MIRRED_PARMS, sizeof(struct tc_mirred), &p);
	tce_action_end(nlh, act, act_opts);
}

static void tce_csum_action(struct nlmsghdr *nlh, uint16_t act_no)
{
	struct nlattr *act, *act_opts;

	tce_action_start(nlh, &act, &act_opts, act_no, "csum");
	struct tc_csum p;

	memset(&p, '\0', sizeof(struct tc_csum));
	p.update_flags = TCA_CSUM_UPDATE_FLAG_IPV4HDR;
	p.action = TC_ACT_PIPE;
	mnl_attr_put(nlh, TCA_CSUM_PARMS, sizeof(struct tc_csum), &p);
	tce_action_end(nlh, act, act_opts);
}

static void tce_add_ip_forward_rule(struct nlmsghdr *nlh, const struct tc_rule *rule)
{
	struct nlattr *acts;

	acts = mnl_attr_nest_start(nlh, TCA_FLOWER_ACT);
	AN(acts);

	uint16_t act_no = 0;

	tce_modify_vlan_action(nlh, ++act_no, rule->vlan_id);
	tce_pedit_action(nlh, ++act_no, rule);
	if (rule->af_addr.af == AF_INET)
		tce_csum_action(nlh, ++act_no);
	tce_redirect_action(nlh, ++act_no);

	mnl_attr_nest_end(nlh, acts);
}

static void tc_encode_add_rule(struct nlmsghdr *nlh, const uint32_t chain_no, const uint16_t prio, const struct tc_rule *tcr, int flags)
{
	nlh->nlmsg_type = RTM_NEWTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_EXCL | NLM_F_CREATE;
	tce_set_tcm(nlh, TC_H_MAKE(prio << 16, htons(ETH_P_8021Q)), flags);
	mnl_attr_put_u32(nlh, TCA_CHAIN, chain_no);

	struct nlattr *flower = tce_new_flower_rule(nlh, tcr, flags);

	switch (tcr->type) {
	case TC_RULE_TYPE_FORWARD:
		tce_add_ip_forward_rule(nlh, tcr);
		break;
	case TC_RULE_TYPE_ROUTE_TRAP:
		tce_add_ip_gact_rule(nlh, tcr, flags, TC_ACT_TRAP);
		break;
	case TC_RULE_TYPE_ROUTE_GOTO:
		tce_add_ip_gact_rule(nlh, tcr, flags,
				TC_ACT_GOTO_CHAIN | tcr->goto_target);
		break;
	case TC_RULE_TYPE_TTL_CHECK:
		tce_add_tll_check_rule(nlh);
		break;
	default:
		AN(false);
		break;
	}

	mnl_attr_nest_end(nlh, flower);
}

void tc_encode_drop_chain(struct nlmsghdr *nlh, const uint32_t chain_no, int flags)
{
	nlh->nlmsg_type = RTM_DELTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	tce_set_tcm(nlh, 0, flags);
	mnl_attr_put_u32(nlh, TCA_CHAIN, chain_no);
}

static void tc_encode_drop_rule(struct nlmsghdr *nlh, const uint32_t chain_no, const uint16_t prio, int flags)
{
	nlh->nlmsg_type = RTM_DELTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	tce_set_tcm(nlh, TC_H_MAKE(prio << 16, 0), flags);
	mnl_attr_put_u32(nlh, TCA_CHAIN, chain_no);
}

void tc_encode_rule(struct nlmsghdr *nlh, const uint32_t chain_no, const uint16_t prio, const struct tc_rule *tcr, int flags)
{
	if (tcr)
		tc_encode_add_rule(nlh, chain_no, prio, tcr, flags);
	else
		tc_encode_drop_rule(nlh, chain_no, prio, flags);
}
