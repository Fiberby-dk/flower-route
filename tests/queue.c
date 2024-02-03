// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"
#include "queue.h"

#include "../src/nl_conn.h"
#include "../src/nl_queue.h"
#include "../src/nl_dump.h"

static int foo_data = 0x42;
static int bar_data = 0x42541;
static int foobar_data = 42541;
static char *last_called;

static void check_and_set_last_called(const char *expected, char *new)
{
	ck_assert_pstr_eq(last_called, expected);
	last_called = new;
}

static void foo_execute(EV_P_ void *data)
{
	const int *val = data;
	struct conn *c = queue_get_conn();

	ck_assert_int_eq(*val, foo_data);
	check_and_set_last_called(NULL, "foo_execute");
	nl_dump_neigh(EV_A_ c, AF_INET);
}

static void foo_completed(EV_P_ void *data, int nl_errno)
{
	const int *val;

	fr_ev_unused();
	val = data;
	ck_assert_int_eq(*val, foo_data);
	ck_assert_int_eq(nl_errno, 0);
	check_and_set_last_called("foo_execute", "foo_completed");
}

static void bar_execute(EV_P_ void *data)
{
	const int *val = data;
	struct conn *c = queue_get_conn();

	ck_assert_int_eq(*val, bar_data);
	check_and_set_last_called("foo_completed", "bar_execute");
	nl_dump_neigh(EV_A_ c, AF_INET6);
}

static void bar_completed(EV_P_ void *data, int nl_errno)
{
	fr_ev_unused();
	const int *val = data;

	ck_assert_int_eq(*val, bar_data);
	ck_assert_int_eq(nl_errno, 0);
	check_and_set_last_called("bar_execute", "bar_completed");
}

START_TEST(queue1)
{
	struct conn c = {0};
	struct ev_loop *loop = EV_DEFAULT;

	last_called = NULL;

	pre_test();

	nl_conn_open(0, &c, "queue_test");
	queue_init(&c);

	queue_schedule(EV_A_ foo_execute, foo_completed, &foo_data);
	queue_schedule(EV_A_ bar_execute, bar_completed, &bar_data);

	ev_run(EV_A_ 0);
	queue_fini();
	nl_conn_close(EV_A_ &c);
	ck_assert_str_eq(last_called, "bar_completed");

	post_test();
}
END_TEST

static void foobar_execute(EV_P_ void *data)
{
	const int *val = data;
	struct conn *c = queue_get_conn();

	ck_assert_int_eq(*val, foobar_data);
	check_and_set_last_called(NULL, "foobar_execute");
	nl_dump_neigh(EV_A_ c, AF_INET6);
}

static void foobar_completed(EV_P_ void *data, int nl_errno)
{
	fr_ev_unused();
	const int *val = data;

	ck_assert_int_eq(*val, foobar_data);
	ck_assert_int_eq(nl_errno, 0);
	check_and_set_last_called("foobar_execute", "foobar_completed");
}

START_TEST(queue2)
{
	struct conn c = {0};
	struct ev_loop *loop = EV_DEFAULT;

	last_called = NULL;

	pre_test();

	nl_conn_open(0, &c, "queue_test");
	queue_init(&c);

	queue_schedule(EV_A_ foobar_execute, foobar_completed, &foobar_data);

	ev_run(EV_A_ 0);
	queue_fini();
	nl_conn_close(EV_A_ &c);
	ck_assert_str_eq(last_called, "foobar_completed");

	post_test();
}
END_TEST

static void tcase_queue(Suite *s)
{
	TCase *tc;

	tc = tcase_create("queue");
	tcase_add_test(tc, queue1);
	tcase_add_test(tc, queue2);

	suite_add_tcase(s, tc);
}

Suite *suite_queue(void)
{
	Suite *s;

	s = suite_create("queue");

	tcase_queue(s);

	return s;
}
