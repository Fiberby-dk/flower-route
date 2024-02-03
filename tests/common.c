// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"

#include "../src/obj_link.h"
#include "../src/obj_neigh.h"
#include "../src/obj_route.h"
#include "../src/obj_target.h"
#include "../src/obj_rule.h"
#include "../src/tc_action.h"
#include "../src/tc_decode.h"
#include "../src/tc_encode.h"
#include "../src/nl_filter.h"
#include "../src/nl_decode.h"
#include "../src/sched.h"

void assert_all_counts_are_zero(void)
{
	ck_assert_int_eq(obj_link_count(), 0);
	ck_assert_int_eq(obj_neigh_count(), 0);
	ck_assert_int_eq(obj_target_count(), 0);
	ck_assert_int_eq(obj_route_count(), 0);
	ck_assert_int_eq(obj_rule_count(), 0);
}

static void tc_install_handler(EV_P_ const uint32_t chain_no, const uint16_t prio, struct tc_rule *tcr)
{
	/*
	 * here we act as if the rule got installed,
	 * and picked up by the monitor
	 *
	 * 1. Encode the new rule into a netlink message
	 * 2. Decode the netlink message back into the new rule
	 * 3. Verify that the rule is identical
	 * 4. Pass the netlink message, as if we got it from the kernel
	 */

	char buf[MNL_SOCKET_DUMP_SIZE];
	struct nlmsghdr *nlh = mnl_nlmsg_put_header(buf);

	tc_encode_rule(nlh, chain_no, prio, tcr, TCE_FLAG_LOOPBACK);

	/* verify that the encode & decode have preserved the rule */
	if (tcr) {
		struct tc_decoded_rule *tdr = decode_filter2(nlh);

		ck_assert_ptr_nonnull(tdr);
		ck_assert_int_eq(tdr->is_done, true);
		ck_assert_int_eq(tdr->chain_no, chain_no);
		ck_assert_int_eq(tdr->prio, prio);
		ck_assert_mem_eq(&tdr->tcr, tcr, sizeof(struct tc_rule));
		free(tdr);
	} else {
		ck_assert_int_eq(nlh->nlmsg_type, RTM_DELTFILTER);
	}

	/* act as if we got the netlink message from the kernel */
	decode_nlmsg_cb(nlh, NULL);
}

void pre_test(void)
{
	struct tc_action_callbacks *tacb;

	config_init("test");
	assert_all_counts_are_zero();
	obj_set_mode(OBJ_MODE_NORMAL);
	sched_setup();

	obj_rule_init();
	tacb = tc_action_get_callbacks();
	tacb->install = tc_install_handler;
}

void post_test(void)
{
	filter_clear_chains();
	ev_loop_destroy(EV_DEFAULT);
	assert_all_counts_are_zero();
	config_free();
}
