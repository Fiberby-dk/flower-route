// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"
#include "options.h"

#include "../src/options.h"
#include "../src/rt_names.h"

static const char * const opts_a_args[] = {"test", "-t", "main", "-i", "lo", "-v", "--verbose"};

START_TEST(opts_a)
{
	size_t len;

	pre_test();
	rt_names_init();

	len = sizeof(opts_a_args) / sizeof(char *);
	ck_assert_int_eq(len, 7);
	options_parse(len, (char **) &opts_a_args);

	ck_assert_int_eq(config->verbosity, VERBOSITY_LEVEL_DEBUG1);
	ck_assert_int_eq(config->table_id, RT_TABLE_MAIN);
	ck_assert_pstr_eq(config->ifname, "lo");

	rt_names_free();
	post_test();
}
END_TEST

static const char * const opts_b_args[] = {"test", "-i", "lo", "-t", "local", "-v"};

START_TEST(opts_b)
{
	size_t len;

	pre_test();
	rt_names_init();

	len = sizeof(opts_b_args) / sizeof(char *);
	ck_assert_int_eq(len, 6);
	options_parse(len, (char **) &opts_b_args);

	ck_assert_int_eq(config->verbosity, VERBOSITY_LEVEL_INFO);
	ck_assert_int_eq(config->table_id, RT_TABLE_LOCAL);
	ck_assert_pstr_eq(config->ifname, "lo");

	rt_names_free();
	post_test();
}
END_TEST

static const char * const opts_c_args[] = {
	"test", "-i", "lo", "-t", "local", "-v",
	"-p", "onload", "192.0.2.0/24", "-p", "onload", "2001:db8::/48"
};

START_TEST(opts_c)
{
	struct config_prefix_list *list;
	size_t len;
	int i = 0;

	pre_test();
	rt_names_init();

	len = sizeof(opts_c_args) / sizeof(char *);
	options_parse(len, (char **) &opts_c_args);

	ck_assert_int_eq(config->verbosity, VERBOSITY_LEVEL_INFO);
	ck_assert_int_eq(config->table_id, RT_TABLE_LOCAL);
	ck_assert_pstr_eq(config->ifname, "lo");

	list = config->prefix_list_head;
	ck_assert_ptr_nonnull(list);
	ck_assert_ptr_null(list->next);
	ck_assert_pstr_eq(list->name, "onload");

	for (struct config_prefix_list_entry *pfx = list->head; pfx; pfx = pfx->next) {
		switch (i) {
		case 0:
			ck_assert_int_eq(pfx->addr.af, AF_INET);
			ck_assert_int_eq(pfx->addr.mask_len, 24);
			ck_assert_ptr_nonnull(pfx->next);
			break;
		case 1:
			ck_assert_int_eq(pfx->addr.af, AF_INET6);
			ck_assert_int_eq(pfx->addr.mask_len, 48);
			ck_assert_ptr_null(pfx->next);
			ck_assert_ptr_eq(list->tail, pfx);
			break;
		}
		i++;
	}

	rt_names_free();
	post_test();
}
END_TEST

static void tcase_options(Suite *s)
{
	TCase *tc;

	tc = tcase_create("options");
	tcase_add_test(tc, opts_a);
	tcase_add_test(tc, opts_b);
	tcase_add_test(tc, opts_c);

	suite_add_tcase(s, tc);
}

Suite *suite_options(void)
{
	Suite *s;

	s = suite_create("options");

	tcase_options(s);

	return s;
}
