// SPDX-License-Identifier: GPL-2.0-or-later

#include "hack.h"
#include "nl_conn.h"
#include "nl_queue.h"
#include "nl_filter.h"
#include "tc_filter.h"

struct my_state {
	struct chain ch;
	uint16_t prio;
	struct conn conn;
	struct chain chain;
	int state;
};

static void increase_state(struct my_state *state)
{
	int old_state = state->state;
	int new_state = old_state + 1;

	state->state = new_state;
	fr_printf(DEBUG1, "change state: %d -> %d\n", old_state, new_state);
}

static void advance_state(EV_P_ void *data, int nl_errno);

static void prepare_state(EV_P_ void *data)
{
	advance_state(EV_A_ data, 0);
}



static void prepare_onload_rule(struct tc_rule *rule, const char *ipstr, uint8_t mask_len)
{
	uint8_t af = strchr(ipstr, ':') == NULL ? AF_INET : AF_INET6;

	rule->af_addr.af = af;
	if (inet_pton(af, ipstr, &rule->af_addr.in) == 0)
		return;
	rule->af_addr.mask_len = mask_len;
	rule->type = TC_RULE_TYPE_ROUTE_TRAP;
}

static void prepare_goto_rule(struct tc_rule *rule, const char *ipstr, const uint8_t mask_len, const uint32_t goto_target)
{
	prepare_onload_rule(rule, ipstr, mask_len);
	rule->goto_target = goto_target;
	rule->type = TC_RULE_TYPE_ROUTE_GOTO;
}

static void prepare_forward_rule(struct tc_rule *rule, uint16_t vid, uint16_t af, const char *dst, const char *src)
{
	rule->vlan_id = vid;
	memcpy(&rule->lladdr.dst, dst, 6);
	memcpy(&rule->lladdr.src, src, 6);
	rule->af_addr.af = af;
	rule->type = TC_RULE_TYPE_FORWARD;
}


#define NEW_HACK_STEP (2+(2*__COUNTER__))
static void advance_state(EV_P_ void *data, int nl_errno)
{
	struct tc_rule rule;
	struct my_state *state = data;
	struct conn *c = &state->conn;

	fr_printf(DEBUG1, "advance_hack %d: %d (%s)\n", state->state, nl_errno, strerror(nl_errno));
	increase_state(state);

	switch (state->state) {
	case 1:
		nl_conn_open(0, c, "hack");
		queue_init(c);
		break;
	case NEW_HACK_STEP:
		// delete chain 14 rule 1
		filter_drop_tc_rule(EV_A_ c, 14, 1);
		break;
	case NEW_HACK_STEP:
		// delete chain 14
		filter_drop_tc_chain(EV_A_ c, 14);
		break;
	case NEW_HACK_STEP:
		// delete chain 16
		filter_drop_tc_chain(EV_A_ c, 16);
		break;
	case NEW_HACK_STEP:
		// delete chain 24
		filter_drop_tc_chain(EV_A_ c, 24);
		break;
	case NEW_HACK_STEP:
		// delete chain 26
		filter_drop_tc_chain(EV_A_ c, 26);
		break;
	case NEW_HACK_STEP:
		// delete chain 34
		filter_drop_tc_chain(EV_A_ c, 34);
		break;
	case NEW_HACK_STEP:
		// delete chain 36
		state->chain.chain_no = 36;
		filter_drop_tc_chain(EV_A_ c, 36);
		break;
	case NEW_HACK_STEP:
		// delete chain 0
		filter_drop_tc_chain(EV_A_ c, 0);
		break;
	case NEW_HACK_STEP:
		// delete chain 0
		filter_drop_tc_chain(EV_A_ c, 0);
		break;
	case NEW_HACK_STEP:
		// 0,1 add ipv4 ttl check
		rule.af_addr.af = AF_INET;
		rule.type = TC_RULE_TYPE_TTL_CHECK;
		filter_add_tc_rule(EV_A_ c, 0, 1, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,2 add ipv6 ttl check
		rule.af_addr.af = AF_INET6;
		rule.type = TC_RULE_TYPE_TTL_CHECK;
		filter_add_tc_rule(EV_A_ c, 0, 2, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,10 add ip trap
		prepare_onload_rule(&rule, "192.0.2.0", 24);
		filter_add_tc_rule(EV_A_ c, 0, 10, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,11 add ip trap
		prepare_onload_rule(&rule, "2001:db8:ffff::", 48);
		filter_add_tc_rule(EV_A_ c, 0, 11, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,100 add ip jump
		prepare_goto_rule(&rule, "198.51.100.0", 25, 14);
		filter_add_tc_rule(EV_A_ c, 0, 100, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,101 add ip jump
		prepare_goto_rule(&rule, "2001:db8:1::", 48, 16);
		filter_add_tc_rule(EV_A_ c, 0, 101, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,102 add ip jump
		prepare_goto_rule(&rule, "198.51.100.128", 25, 24);
		filter_add_tc_rule(EV_A_ c, 0, 102, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,103 add ip jump
		prepare_goto_rule(&rule, "2001:db8:2::", 48, 26);
		filter_add_tc_rule(EV_A_ c, 0, 103, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,104 add ip jump
		prepare_goto_rule(&rule, "203.0.113.0", 25, 34);
		filter_add_tc_rule(EV_A_ c, 0, 104, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,105 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 48, 36);
		filter_add_tc_rule(EV_A_ c, 0, 105, &rule);
		break;

	case NEW_HACK_STEP:
		// 0,110 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 49, 36);
		filter_add_tc_rule(EV_A_ c, 0, 110, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,111 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 50, 36);
		filter_add_tc_rule(EV_A_ c, 0, 111, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,112 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 51, 36);
		filter_add_tc_rule(EV_A_ c, 0, 112, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,113 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 52, 36);
		filter_add_tc_rule(EV_A_ c, 0, 113, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,114 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 53, 36);
		filter_add_tc_rule(EV_A_ c, 0, 114, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,115 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 54, 36);
		filter_add_tc_rule(EV_A_ c, 0, 115, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,116 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 55, 36);
		filter_add_tc_rule(EV_A_ c, 0, 116, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,117 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 56, 36);
		filter_add_tc_rule(EV_A_ c, 0, 117, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,118 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 57, 36);
		filter_add_tc_rule(EV_A_ c, 0, 118, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,119 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 58, 36);
		filter_add_tc_rule(EV_A_ c, 0, 119, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,120 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 128, 36);
		filter_add_tc_rule(EV_A_ c, 0, 120, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,121 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 0, 36);
		filter_add_tc_rule(EV_A_ c, 0, 121, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,122 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 1, 36);
		filter_add_tc_rule(EV_A_ c, 0, 122, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,123 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 2, 36);
		filter_add_tc_rule(EV_A_ c, 0, 123, &rule);
		break;
	case NEW_HACK_STEP:
		// 0,124 add ip jump
		prepare_goto_rule(&rule, "2001:db8:3::", 127, 36);
		filter_add_tc_rule(EV_A_ c, 0, 124, &rule);
		break;

	case NEW_HACK_STEP:
		// 14,1 add ip jump
		prepare_forward_rule(&rule, 710, AF_INET,
				"\x0a\x12\x34\x56\x00\x12", "\x0a\x12\x34\x56\x00\x11");
		filter_add_tc_rule(EV_A_ c, 14, 1, &rule);
		break;
	case NEW_HACK_STEP:
		// 16,1 add ip jump
		prepare_forward_rule(&rule, 710, AF_INET6,
				"\x0a\x12\x34\x56\x00\x12", "\x0a\x12\x34\x56\x00\x11");
		filter_add_tc_rule(EV_A_ c, 16, 1, &rule);
		break;
	case NEW_HACK_STEP:
		// 24,1 add ip jump
		prepare_forward_rule(&rule, 720, AF_INET,
				"\x0a\x12\x34\x56\x00\x22", "\x0a\x12\x34\x56\x00\x21");
		filter_add_tc_rule(EV_A_ c, 24, 1, &rule);
		break;
	case NEW_HACK_STEP:
		// 26,1 add ip jump
		prepare_forward_rule(&rule, 720, AF_INET6,
				"\x0a\x12\x34\x56\x00\x22", "\x0a\x12\x34\x56\x00\x21");
		filter_add_tc_rule(EV_A_ c, 26, 1, &rule);
		break;
	case NEW_HACK_STEP:
		// 34,1 add ip jump
		prepare_forward_rule(&rule, 730, AF_INET,
				"\x0a\x12\x34\x56\x00\x32", "\x0a\x12\x34\x56\x00\x31");
		filter_add_tc_rule(EV_A_ c, 34, 1, &rule);
		break;
	case NEW_HACK_STEP:
		// 36,1 add ip jump
		prepare_forward_rule(&rule, 730, AF_INET6,
				"\x0a\x12\x34\x56\x00\x32", "\x0a\x12\x34\x56\x00\x31");
		filter_add_tc_rule(EV_A_ c, 36, 1, &rule);
		break;
	default:
		break;
	}
	if ((state->state & 1) && state->state < NEW_HACK_STEP)
		queue_schedule(EV_A_ prepare_state, advance_state, state);
}

void hack_static(EV_P)
{
	struct my_state *state = fr_malloc(sizeof(struct my_state));

	prepare_state(EV_A, state);
}
