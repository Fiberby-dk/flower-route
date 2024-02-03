// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"
#include "scan.h"

#include "../src/scan.h"
#include "../src/options.h"

static const char * const opts_args[] = {"test", "-i", "lo", "-1", "-t", "main"};

START_TEST(test_scan)
{
	size_t len;

	pre_test();

	len = sizeof(opts_args) / sizeof(char *);
	options_parse(len, (char **) &opts_args);

	struct ev_loop *loop = EV_DEFAULT;

	scan_init(EV_A);
	ev_run(EV_A_ 0);
	scan_fini(EV_A);

	post_test();
}
END_TEST

static void tcase_scan(Suite *s)
{
	TCase *tc;

	tc = tcase_create("scan");
	tcase_add_test(tc, test_scan);

	suite_add_tcase(s, tc);
}

Suite *suite_scan(void)
{
	Suite *s;

	s = suite_create("scan");

	tcase_scan(s);

	return s;
}
