/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef FLOWER_ROUTE_TC_RULE_H
#define FLOWER_ROUTE_TC_RULE_H

#include "nl_common.h"
#include "nl_filter.h"

enum tc_rule_types {
	TC_RULE_TYPE_UNSPEC,
	TC_RULE_TYPE_ALIEN,
	TC_RULE_TYPE_FORWARD,
	TC_RULE_TYPE_ROUTE_TRAP,
	TC_RULE_TYPE_ROUTE_GOTO,
	TC_RULE_TYPE_ROUTE_DFT_GOTO,
	TC_RULE_TYPE_TTL_CHECK,
	TC_RULE_TYPE_MAX
};

enum tc_rule_traits {
	TC_RULE_HAVE_AF        = 1<<0,
	TC_RULE_HAVE_IP        = 1<<1,
	TC_RULE_HAVE_GOTO      = 1<<2,
	TC_RULE_HAVE_TRAP      = 1<<3,
	TC_RULE_HAVE_TTL_CHECK = 1<<4,
	TC_RULE_HAVE_TTL_DEC   = 1<<5,
	TC_RULE_HAVE_LLADDR    = 1<<6,
	TC_RULE_HAVE_VLAN_MOD  = 1<<7,
};
#define TC_RULE_HAVE_AF_IP (TC_RULE_HAVE_AF | TC_RULE_HAVE_IP)

struct tc_rule {
	//struct chain *chain;
	enum tc_rule_types type;
	//struct tc_chain *jump_to;
	//enum tc_state state;
	uint16_t vlan_id;
	uint32_t flower_flags;
	uint32_t goto_target;
	unsigned int traits;
	//const char *kind;
	struct af_addr af_addr;
	uint8_t ttl;
	union {
		struct {
			uint8_t dst[ETH_ALEN];
			uint8_t src[ETH_ALEN];
		};
		struct {
			uint32_t raw[3];
		};
	} lladdr;
};

void tc_rule_set_type(struct tc_rule *rule, const enum tc_rule_types type);
void tc_rule_set_type_and_traits(struct tc_rule *rule, const enum tc_rule_types type);
int tc_rule_mark_alien(struct tc_rule *rule);
void tc_rule_print(const struct tc_rule *rule);
enum tc_rule_types tc_rule_detect(struct tc_rule *rule);
const char *tc_rule_state_str(enum tc_rule_types type);
void tc_rule_init(struct tc_rule *tcr);
int tc_rule_set_dst(struct tc_rule *tcr, const char *ipstr, const uint8_t mask_len);

#endif
