// SPDX-License-Identifier: GPL-2.0-or-later

#include "tc_rule.h"

static unsigned int type_traits[TC_RULE_TYPE_MAX] = {
	[TC_RULE_TYPE_FORWARD] = TC_RULE_HAVE_AF | TC_RULE_HAVE_LLADDR | TC_RULE_HAVE_TTL_DEC | TC_RULE_HAVE_VLAN_MOD,
	[TC_RULE_TYPE_TTL_CHECK] = TC_RULE_HAVE_AF | TC_RULE_HAVE_TTL_CHECK | TC_RULE_HAVE_TRAP,
	[TC_RULE_TYPE_ROUTE_GOTO] = TC_RULE_HAVE_AF_IP | TC_RULE_HAVE_GOTO,
	[TC_RULE_TYPE_ROUTE_DFT_GOTO] = TC_RULE_HAVE_AF | TC_RULE_HAVE_GOTO,
	[TC_RULE_TYPE_ROUTE_TRAP] = TC_RULE_HAVE_AF_IP | TC_RULE_HAVE_TRAP,
};

void tc_rule_init(struct tc_rule *tcr)
{
	tcr->flower_flags = config->flower_flags;
}

void tc_rule_set_type(struct tc_rule *rule, const enum tc_rule_types type)
{
	AN(type < TC_RULE_TYPE_MAX);
	if (rule->type == TC_RULE_TYPE_ALIEN)
		return;
	AN(rule->type == TC_RULE_TYPE_UNSPEC);
	rule->type = type;
}

void tc_rule_set_type_and_traits(struct tc_rule *rule, const enum tc_rule_types type)
{
	tc_rule_set_type(rule, type);
	if (rule->traits == 0)
		rule->traits = type_traits[type];
	AN(rule->traits != 0);
}

enum tc_rule_types tc_rule_detect(struct tc_rule *rule)
{
	unsigned int traits = rule->traits;

	for (int i = 0; i < TC_RULE_TYPE_MAX; i++) {
		unsigned int expected_traits = type_traits[i];

		if (expected_traits > 0 && expected_traits == traits) {
			if (i == TC_RULE_TYPE_ROUTE_DFT_GOTO) {
				rule->traits = type_traits[TC_RULE_TYPE_ROUTE_GOTO];
				return TC_RULE_TYPE_ROUTE_GOTO;
			}
			return i;
		}
	}
	fr_printf(DEBUG2, "Unrecognized TC rule, with traits %08x\n", traits);
	return TC_RULE_TYPE_ALIEN;
}

static int tc_rule_validate(struct tc_rule *rule)
{
	unsigned int detected = tc_rule_detect(rule);

	if (rule->type != detected) {
		rule->type = TC_RULE_TYPE_ALIEN;
		return false;
	}
	return true;
}



const char *tc_rule_state_str(enum tc_rule_types type)
{
	switch (type) {
	case TC_RULE_TYPE_UNSPEC:
		return "unspec";
	case TC_RULE_TYPE_ALIEN:
		return "alien";
	case TC_RULE_TYPE_FORWARD:
		return "forward";
	case TC_RULE_TYPE_ROUTE_TRAP:
		return "route_trap";
	case TC_RULE_TYPE_ROUTE_GOTO:
	case TC_RULE_TYPE_ROUTE_DFT_GOTO:
		return "route_goto";
	case TC_RULE_TYPE_TTL_CHECK:
		return "ttl_check";
	case TC_RULE_TYPE_MAX:
		break;
	}
	return "???";
}

int tc_rule_mark_alien(struct tc_rule *rule)
{
	tc_rule_set_type(rule, TC_RULE_TYPE_ALIEN);
	printf("alien ALIEN alien\n");
	return MNL_CB_OK;
}

int tc_rule_set_dst(struct tc_rule *tcr, const char *ipstr, const uint8_t mask_len)
{
	uint8_t af = strchr(ipstr, ':') == NULL ? AF_INET : AF_INET6;

	if (inet_pton(af, ipstr, &tcr->af_addr.in) == 0)
		return false;

	tcr->af_addr.af = af;
	tcr->af_addr.mask_len = mask_len;
	return true;
}

void tc_rule_print(const struct tc_rule *rule)
{
	fr_printf(DEBUG1, "rule %s\n", tc_rule_state_str(rule->type));
}
