// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"
#include "obj.h"

#include "../src/obj_link.h"
#include "../src/obj_neigh.h"
#include "../src/obj_route.h"
#include "../src/obj_target.h"
#include "../src/obj_rule.h"
#include "../src/nl_queue.h"

const uint8_t lladdr_a[ETH_ALEN] = { 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf };
const uint8_t lladdr_b[ETH_ALEN] = { 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf };
const uint8_t lladdr_c[ETH_ALEN] = { 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf };
const uint8_t lladdr_d[ETH_ALEN] = { 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf };
const uint8_t lladdr_e[ETH_ALEN] = { 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef };
const uint8_t lladdr_f[ETH_ALEN] = { 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff };
const uint8_t lladdr_n[ETH_ALEN] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

struct af_addr addr_a;
struct af_addr addr_b;
struct af_addr addr_c;
struct af_addr addr_d;
struct af_addr addr_e;
struct af_addr addr_f;

static void prepare_addresses(void)
{
	build_af_addr2(&addr_a, AF_INET, "192.0.2.1", 0);
	build_af_addr2(&addr_b, AF_INET, "192.0.2.2", 0);
	build_af_addr2(&addr_c, AF_INET, "192.0.2.3", 0);
	build_af_addr2(&addr_d, AF_INET, "192.0.2.4", 0); /* XXX: 0 != 32 mask */
	build_af_addr2(&addr_e, AF_INET6, "2001:db8:3::", 0);
	build_af_addr2(&addr_f, AF_INET6, "2001:db8:4::", 0);
}

static void add_link1(void)
{
	obj_link_netlink_update(RTM_NEWLINK, 2, &lladdr_a, 1, 123, 1500, "foo");
}

static void rem_link1(void)
{
	obj_link_netlink_update(RTM_DELLINK, 2, &lladdr_a, 1, 123, 1500, "foo");
}

static void add_link2(void)
{
	obj_link_netlink_update(RTM_NEWLINK, 3, &lladdr_b, 1, 234, 1500, "bar");
}

static void add_link2_mac_c(void)
{
	obj_link_netlink_update(RTM_NEWLINK, 3, &lladdr_c, 1, 234, 1500, "bar");
}

static void rem_link2(void)
{
	obj_link_netlink_update(RTM_DELLINK, 3, &lladdr_b, 1, 234, 1500, "bar");
}

static void update_neigh(const uint16_t nlmsg_type, const uint32_t ifidx, struct af_addr *addr, const uint8_t (*lladdr)[ETH_ALEN])
{
	obj_neigh_netlink_update(nlmsg_type, ifidx, addr->af, &addr->in, lladdr);
}

static void add_neigh1(void)
{
	update_neigh(RTM_NEWNEIGH, 2, &addr_a, &lladdr_c);
}

static void add_neigh2(void)
{
	update_neigh(RTM_NEWNEIGH, 3, &addr_b, &lladdr_d);
}

static void rem_neigh1(void)
{
	update_neigh(RTM_DELNEIGH, 2, &addr_a, &lladdr_c);
}

static struct obj_target *add_target1(void)
{
	struct obj_link *l;
	struct obj_neigh *n;
	struct obj_target *t;

	l = obj_link_lookup(2);
	ck_assert_ptr_nonnull(l);

	n = obj_neigh_fdb_lookup(l, &addr_a);
	ck_assert_ptr_nonnull(n);

	t = obj_target_get_unipath(n);
	ck_assert_ptr_nonnull(t);

	return t;
}

static struct obj_target *add_target2(void)
{
	struct obj_link *l;
	struct obj_neigh *n;
	struct obj_target *t;

	l = obj_link_lookup(3);
	ck_assert_ptr_nonnull(l);

	n = obj_neigh_fdb_lookup(l, &addr_b);
	ck_assert_ptr_nonnull(n);

	t = obj_target_get_unipath(n);
	ck_assert_ptr_nonnull(t);

	return t;
}

static struct obj_target *add_target3(void)
{
	struct obj_link *l;
	struct obj_neigh *n;
	struct obj_target *t;

	l = obj_link_lookup(2);
	ck_assert_ptr_nonnull(l);

	n = obj_neigh_fdb_lookup(l, &addr_e);
	ck_assert_ptr_nonnull(n);

	t = obj_target_get_unipath(n);
	ck_assert_ptr_nonnull(t);

	return t;
}

START_TEST(obj_basics)
{
	struct obj_link link;
	struct obj_neigh neigh;
	struct obj_route route;
	struct obj_target target;
	struct obj_route rule;

	/* check that struct obj_core is the first member */
	ck_assert_ptr_eq(&link, &link.obj);
	ck_assert_ptr_eq(&neigh, &neigh.obj);
	ck_assert_ptr_eq(&route, &route.obj);
	ck_assert_ptr_eq(&target, &target.obj);
	ck_assert_ptr_eq(&rule, &rule.obj);
}
END_TEST

START_TEST(obj_link_cycle1)
{
	pre_test();

	add_link1();
	ck_assert_int_eq(obj_link_count(), 1);
	rem_link1();
	ck_assert_int_eq(obj_link_count(), 0);

	post_test();
}
END_TEST

START_TEST(obj_link_cycle2)
{
	pre_test();

	add_link1();
	ck_assert_int_eq(obj_link_count(), 1);
	add_link2();
	ck_assert_int_eq(obj_link_count(), 2);
	rem_link2();
	ck_assert_int_eq(obj_link_count(), 1);
	rem_link1();

	post_test();
}
END_TEST

START_TEST(obj_link_cycle3)
{
	struct obj_link *l;

	pre_test();

	add_link1();
	l = obj_link_ref(obj_link_lookup(2));
	ck_assert_ptr_nonnull(l);
	ck_assert_int_eq(obj_link_count(), 1);
	rem_link1();
	rem_link2();
	ck_assert_int_eq(obj_link_count(), 1);
	obj_link_unref(l);
	ck_assert_int_eq(obj_link_count(), 0);

	post_test();
}
END_TEST

START_TEST(obj_neigh_cycle1)
{
	pre_test();
	prepare_addresses();

	add_link1();
	ck_assert_int_eq(obj_link_count(), 1);

	add_neigh1();
	ck_assert_int_eq(obj_neigh_count(), 1);

	rem_neigh1();
	ck_assert_int_eq(obj_neigh_count(), 0);
	ck_assert_int_eq(obj_link_count(), 1);

	rem_link1();

	post_test();
}
END_TEST

START_TEST(obj_neigh_cycle2)
{
	pre_test();
	prepare_addresses();

	add_link1();
	ck_assert_int_eq(obj_link_count(), 1);

	add_neigh1();
	ck_assert_int_eq(obj_neigh_count(), 1);

	rem_link1(); /* removing the link, should also remove the neighbour */
	ck_assert_int_eq(obj_neigh_count(), 0);
	ck_assert_int_eq(obj_link_count(), 0);

	post_test();
}
END_TEST

START_TEST(obj_neigh_cycle3)
{
	struct af_addr addrs[256];

	pre_test();

	add_link1();
	ck_assert_int_eq(obj_link_count(), 1);

	for (int i = 0; i < 256; i++) {
		struct af_addr *addr = &addrs[i];
		char buf[INET6_ADDRSTRLEN];
		uint8_t af = i & 1 ? AF_INET : AF_INET6;
		int ret = 0;

		switch (af) {
		case AF_INET:
			ret = snprintf((char *) &buf, INET6_ADDRSTRLEN, "%d.%d.%d.%d", i, i, 255 - i, i);
			break;
		case AF_INET6:
			ret = snprintf((char *) &buf, INET6_ADDRSTRLEN, "2001:db8:%x:%x::%x:%x", i^12, i^34, i^45, i);
			break;
		}
		AN(ret > 0);

		build_af_addr2(addr, af, buf, 0);
	}

	/* add all neighbours */
	for (int i = 0; i < 256; i++) {
		struct af_addr *addr = &addrs[i^0x55];

		update_neigh(RTM_NEWNEIGH, 2, addr, &lladdr_c);
	}
	ck_assert_int_eq(obj_neigh_count(), 256);

	/* remove some neighbours */
	for (int i = 0; i < 256; i += 4) {
		struct af_addr *addr = &addrs[i^0x33];

		update_neigh(RTM_DELNEIGH, 2, addr, &lladdr_c);
	}
	ck_assert_int_eq(obj_neigh_count(), 192);

	/* add some neighbours */
	for (int i = 128; i < 256; i += 4) {
		struct af_addr *addr = &addrs[i^0x33];

		update_neigh(RTM_NEWNEIGH, 2, addr, &lladdr_c);
	}
	ck_assert_int_eq(obj_neigh_count(), 224);

	rem_link1(); /* removing the link, should also remove the neighbours */
	ck_assert_int_eq(obj_neigh_count(), 0);
	ck_assert_int_eq(obj_link_count(), 0);

	post_test();
}
END_TEST

START_TEST(obj_target_cycle1)
{
	struct obj_target *t;

	pre_test();
	prepare_addresses();
	add_link1();
	add_neigh1();

	t = add_target1();
	ck_assert_int_eq(obj_target_count(), 1);
	ck_assert_ptr_nonnull(t);

	rem_link1();
	ck_assert_int_eq(obj_target_count(), 0);

	post_test();
}
END_TEST

START_TEST(obj_route_cycle1)
{
	struct obj_target *t;
	struct af_addr my_net = { .af = AF_INET, .mask_len = 25 };

	ck_assert_int_eq(inet_pton(AF_INET, "192.0.2.128", &my_net.in), 1);

	pre_test();
	prepare_addresses();
	//config->verbosity = VERBOSITY_LEVEL_DEBUG2;
	obj_rule_reset_pin();

	add_link1();
	add_neigh1();
	t = add_target1();
	ck_assert_int_eq(obj_target_count(), 1);
	obj_route_netlink_update(RTM_NEWROUTE, t, &my_net);
	ck_assert_int_eq(obj_route_count(), 1);
	ck_assert_int_eq(obj_rule_count(), 1);
	obj_rule_remove_pin();
	ck_assert_int_eq(obj_rule_count(), 2);

	obj_set_mode(OBJ_MODE_TEARDOWN);
	rem_link1(); /* this should clean up all the objects */

	post_test();
}
END_TEST

START_TEST(obj_route_cycle2)
{
	struct obj_target *t1, *t2, *t3;
	struct af_addr net1 = { .af = AF_INET, .mask_len = 25 };
	struct af_addr net2 = { .af = AF_INET, .mask_len = 24 };
	struct af_addr net3 = { .af = AF_INET, .mask_len = 24 };
	struct af_addr net4 = { .af = AF_INET6, .mask_len = 48 };

	ck_assert_int_eq(inet_pton(AF_INET, "192.0.2.128", &net1.in), 1);
	ck_assert_int_eq(inet_pton(AF_INET, "198.51.100.0", &net2.in), 1);
	ck_assert_int_eq(inet_pton(AF_INET, "203.0.113.0", &net3.in), 1);
	ck_assert_int_eq(inet_pton(AF_INET6, "2001:db8::", &net4.in), 1);

	pre_test();
	prepare_addresses();
	config->verbosity = VERBOSITY_LEVEL_ERROR;
	obj_rule_reset_pin();

	add_link1();
	add_neigh1();
	t1 = add_target1();
	ck_assert_int_eq(obj_target_count(), 1);
	obj_route_netlink_update(RTM_NEWROUTE, t1, &net1);
	obj_route_netlink_update(RTM_NEWROUTE, t1, &net2);
	obj_route_netlink_update(RTM_NEWROUTE, t1, &net3);
	ck_assert_int_eq(obj_route_count(), 3);
	ck_assert_int_eq(obj_rule_count(), 1);
	obj_rule_remove_pin();

	obj_rule_print_all();
	ck_assert_int_eq(obj_rule_count(), 4);
	obj_route_netlink_update(RTM_DELROUTE, t1, &net1);
	ck_assert_int_eq(obj_route_count(), 2);
	ck_assert_int_eq(obj_rule_count(), 3);
	obj_route_netlink_update(RTM_DELROUTE, t1, &net1);
	ck_assert_int_eq(obj_rule_count(), 3);

	obj_rule_print_all();

	add_link2();
	add_neigh2();
	t2 = add_target2();
	ck_assert_int_eq(obj_target_count(), 2);
	obj_route_netlink_update(RTM_NEWROUTE, t2, &net1);
	obj_route_netlink_update(RTM_NEWROUTE, t2, &net1);
	ck_assert_int_eq(obj_rule_count(), 5);
	obj_rule_print_all();

	/* test with a IPv6 route */
	update_neigh(RTM_NEWNEIGH, 2, &addr_e, &lladdr_e);
	t3 = add_target3();
	obj_route_netlink_update(RTM_NEWROUTE, t3, &net4);
	ck_assert_int_eq(obj_rule_count(), 7);
	obj_rule_print_all();

	/* verify chains and prios */
	ck_assert_int_eq(t1->rule->state, OBJ_RULE_STATE_OK);
	ck_assert_int_eq(t1->rule->chain_no, 5);
	ck_assert_int_eq(t1->rule->prio, 1);
	ck_assert_int_eq(t1->rule->have->type, TC_RULE_TYPE_FORWARD);
	ck_assert_mem_eq(&t1->rule->have->lladdr.src, &lladdr_a, ETH_ALEN);
	ck_assert_mem_eq(&t1->rule->have->lladdr.dst, &lladdr_c, ETH_ALEN);

	ck_assert_int_eq(t2->rule->state, OBJ_RULE_STATE_OK);
	ck_assert_int_eq(t2->rule->chain_no, 6);
	ck_assert_int_eq(t2->rule->prio, 1);
	ck_assert_int_eq(t2->rule->have->type, TC_RULE_TYPE_FORWARD);
	ck_assert_mem_eq(&t2->rule->have->lladdr.src, &lladdr_b, ETH_ALEN);
	ck_assert_mem_eq(&t2->rule->have->lladdr.dst, &lladdr_d, ETH_ALEN);

	ck_assert_int_eq(t3->rule->state, OBJ_RULE_STATE_OK);
	ck_assert_int_eq(t3->rule->chain_no, 7);
	ck_assert_int_eq(t3->rule->prio, 1);
	ck_assert_int_eq(t3->rule->have->type, TC_RULE_TYPE_FORWARD);
	ck_assert_mem_eq(&t3->rule->have->lladdr.src, &lladdr_a, ETH_ALEN);
	ck_assert_mem_eq(&t3->rule->have->lladdr.dst, &lladdr_e, ETH_ALEN);

	struct obj_rule *r;

	r = obj_rule_pos_lookup(1, 100);
	ck_assert_ptr_nonnull(r);
	ck_assert_int_eq(r->chain_no, 1);
	ck_assert_int_eq(r->prio, 100);
	ck_assert_int_eq(r->have->type, TC_RULE_TYPE_ROUTE_GOTO);
	ck_assert_int_eq(r->state, OBJ_RULE_STATE_OK);
	ck_assert_int_eq(r->have->af_addr.af, AF_INET);

	r = obj_rule_pos_lookup(1, 101);
	ck_assert_ptr_nonnull(r);
	ck_assert_int_eq(r->chain_no, 1);
	ck_assert_int_eq(r->prio, 101);
	ck_assert_int_eq(r->have->type, TC_RULE_TYPE_ROUTE_GOTO);
	ck_assert_int_eq(r->state, OBJ_RULE_STATE_OK);
	ck_assert_int_eq(r->have->af_addr.af, AF_INET);

	r = obj_rule_pos_lookup(1, 102);
	ck_assert_ptr_nonnull(r);
	ck_assert_int_eq(r->chain_no, 1);
	ck_assert_int_eq(r->prio, 102);
	ck_assert_int_eq(r->have->type, TC_RULE_TYPE_ROUTE_GOTO);
	ck_assert_int_eq(r->state, OBJ_RULE_STATE_OK);
	ck_assert_int_eq(r->have->af_addr.af, AF_INET);

	r = obj_rule_pos_lookup(2, 100);
	ck_assert_ptr_nonnull(r);
	ck_assert_int_eq(r->chain_no, 2);
	ck_assert_int_eq(r->prio, 100);
	ck_assert_int_eq(r->have->type, TC_RULE_TYPE_ROUTE_GOTO);
	ck_assert_int_eq(r->state, OBJ_RULE_STATE_OK);
	ck_assert_int_eq(r->have->af_addr.af, AF_INET6);

	/* change MAC on link2 */
	add_link2_mac_c();

	obj_rule_print_all();
	ck_assert_int_eq(obj_rule_count(), 7);

	/* TODO: verify that the rules updated to the new mac */
	/* TODO: verify the order that the rule was updated in */

	update_neigh(RTM_NEWNEIGH, 2, &addr_e, &lladdr_f);
	/* TODO: verify that the rules updated to the new mac */
	/* TODO: verify the order that the rule was updated in */


	/* TODO make neigh invalid */
	/* TODO confirm that routes are uninstalled */

	obj_set_mode(OBJ_MODE_TEARDOWN);
	rem_link2();
	rem_link1(); /* this should clean up all the objects */

	post_test();
}
END_TEST

// TODO test rule placement more
// TODO test lost and found rules
// TODO test rule content

static void tcase_basics(Suite *s)
{
	TCase *tc;

	tc = tcase_create("link");
	tcase_add_test(tc, obj_basics);

	suite_add_tcase(s, tc);
}

static void tcase_link(Suite *s)
{
	TCase *tc;

	tc = tcase_create("link");
	tcase_add_test(tc, obj_link_cycle1);
	tcase_add_test(tc, obj_link_cycle2);
	tcase_add_test(tc, obj_link_cycle3);

	suite_add_tcase(s, tc);
}

static void tcase_neigh(Suite *s)
{
	TCase *tc;

	tc = tcase_create("neigh");
	tcase_add_test(tc, obj_neigh_cycle1);
	tcase_add_test(tc, obj_neigh_cycle2);
	tcase_add_test(tc, obj_neigh_cycle3);

	suite_add_tcase(s, tc);
}

static void tcase_target(Suite *s)
{
	TCase *tc;

	tc = tcase_create("target");
	tcase_add_test(tc, obj_target_cycle1);

	suite_add_tcase(s, tc);
}

static void tcase_route(Suite *s)
{
	TCase *tc;

	tc = tcase_create("route");
	tcase_add_test(tc, obj_route_cycle1);
	tcase_add_test(tc, obj_route_cycle2);

	suite_add_tcase(s, tc);
}

Suite *suite_obj(void)
{
	Suite *s;

	s = suite_create("obj");

	tcase_basics(s);
	tcase_link(s);
	tcase_neigh(s);
	tcase_target(s);
	tcase_route(s);

	return s;
}
