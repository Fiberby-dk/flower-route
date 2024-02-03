// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * this is a simple scheduler
 *
 * chain=0,offset=100
 *
 *
 */

#include "common.h"
#include "tc_rule.h"
#include "obj_rule.h"
#include "sched_basic.h"
#include "nl_filter.h"
#include "onload.h"

static void request_af_goto_rule(const uint32_t chain_no, const uint16_t prio, const uint8_t af, const uint32_t goto_target)
{
	struct tc_rule tcr = {0};

	tc_rule_init(&tcr);
	tcr.af_addr.af = af;
	if (af == AF_INET6) {
		/* mlx5_core can't match on ::/0, so matching on IPv6 unicast prefix */
		AN(tc_rule_set_dst(&tcr, "2000::", 3));
	}
	tcr.goto_target = goto_target;
	tc_rule_set_type_and_traits(&tcr, TC_RULE_TYPE_ROUTE_GOTO);
	obj_rule_static_want(chain_no, prio, &tcr);
}

static void request_ttl_check(const uint32_t chain_no, const uint16_t prio, const uint8_t af)
{
	struct tc_rule tcr = {0};

	tc_rule_init(&tcr);
	tcr.af_addr.af = af;
	tc_rule_set_type_and_traits(&tcr, TC_RULE_TYPE_TTL_CHECK);
	obj_rule_static_want(chain_no, prio, &tcr);
}

static void request_onload_rule(const uint32_t chain_no, const uint16_t prio, const struct af_addr *af_addr)
{
	struct tc_rule tcr = {0};

	tc_rule_init(&tcr);
	memcpy(&tcr.af_addr, af_addr, sizeof(struct af_addr));
	tc_rule_set_type_and_traits(&tcr, TC_RULE_TYPE_ROUTE_TRAP);
	obj_rule_static_want(chain_no, prio, &tcr);
}

static uint32_t get_af_chain(const uint8_t af)
{
	switch (af) {
	case AF_INET:
		return 1;
	case AF_INET6:
		return 2;
	default:
		AN(false);
		break;
	}
	//return 0;
}

static void sched_basic_place_prefix_list(const uint16_t base_prio, const char *list_name)
{
	struct config_prefix_list *list;

	list = onload_lookup_prefix_list(list_name);
	if (!list)
		return;

	for (struct config_prefix_list_entry *pfx = list->head; pfx; pfx = pfx->next) {
		uint32_t chain_no = get_af_chain(pfx->addr.af);
		uint16_t prio = obj_rule_find_available_prio(chain_no, base_prio);

		request_onload_rule(chain_no, prio, &pfx->addr);
	}
}

static void sched_basic_initial_requests(void)
{
	/* sort packets by address family */
	/* chain 0 must be kept small, as skip_sw currently slows the software path */
	request_af_goto_rule(0, 1, AF_INET, 1);
	request_af_goto_rule(0, 2, AF_INET6, 2);

	/* trap packets with expiring TTL */
	request_ttl_check(1, 1, AF_INET);
	request_ttl_check(2, 1, AF_INET6);

	sched_basic_place_prefix_list(10, "onload");
}

static int sched_basic_place(const struct tc_rule *tcr, uint32_t *chain_no, uint16_t *prio)
{
	switch (tcr->type) {
	case TC_RULE_TYPE_FORWARD:
		*prio = 1;
		*chain_no = filter_find_available_chain_no(5);
		fr_printf(INFO, "filter_find_available_chain_no: %d\n", *chain_no);
		return true;
	case TC_RULE_TYPE_ROUTE_GOTO:
		*chain_no = get_af_chain(tcr->af_addr.af);
		*prio = obj_rule_find_available_prio(*chain_no, 100);
		AN(*prio >= 100);
		fr_printf(INFO, "obj_rule_find_available_prio: %d\n", *prio);
		return true;
	default:
		fr_printf(INFO, "sched_basic: failed to place rule\n");
		break;
	}
	return false;
}

static void sched_basic_init(void)
{
	sched_basic_initial_requests();
}

static const struct sched_ops sched_basic_ops = {
	.init = sched_basic_init,
	.place = sched_basic_place,
};

const struct sched_ops *sched_basic_setup(void)
{
	return &sched_basic_ops;
}
