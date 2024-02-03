// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"

#include "options.h"
#include "queue.h"
#include "scan.h"
#include "obj.h"
#include "sched.h"

static Suite *master_suite(void)
{
	return NULL;
}

int main(void)
{
	int failed;

	SRunner *sr = srunner_create(master_suite());

	srunner_add_suite(sr, suite_options());
	srunner_add_suite(sr, suite_queue());
	srunner_add_suite(sr, suite_scan());
	srunner_add_suite(sr, suite_obj());
	srunner_add_suite(sr, suite_sched());

	srunner_run_all(sr, CK_NORMAL);
	failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return failed;
}
