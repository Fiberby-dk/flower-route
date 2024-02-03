// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"
#include "sched.h"

#include "../src/obj_link.h"
#include "../src/obj_neigh.h"
#include "../src/obj_route.h"
#include "../src/obj_target.h"
#include "../src/obj_rule.h"
#include "../src/nl_queue.h"
#include "../src/sched.h"

START_TEST(obj_sched_basic1)
{
	pre_test();
	obj_rule_reset_pin();

	sched_init();

	obj_rule_remove_pin();
	ck_assert_int_eq(obj_rule_count(), 4);
	obj_rule_print_all();

	obj_set_mode(OBJ_MODE_TEARDOWN);
	obj_rule_clear_all();
	post_test();
}
END_TEST

static void tcase_sched(Suite *s)
{
	TCase *tc;

	tc = tcase_create("basic");
	tcase_add_test(tc, obj_sched_basic1);

	suite_add_tcase(s, tc);
}

Suite *suite_sched(void)
{
	Suite *s;

	s = suite_create("sched");

	tcase_sched(s);

	return s;
}
