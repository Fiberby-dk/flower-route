// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"

#include "options.h"
#include "rt_names.h"

#include "scan.h"
#include "monitor.h"
#include "obj_rule.h"
#include "sched_basic.h"

ev_timer timeout_watcher;

static void timeout_cb(EV_P_ ev_timer *w, int revents)
{
	fr_unused(w);
	fr_unused(revents);
	printf("timeout\n");
	ev_break(EV_A_ EVBREAK_ALL);
}

static void timeout_init(EV_P)
{
	double timeout = config->timeout;

	if (timeout > 0) {
		ev_timer_init(&timeout_watcher, timeout_cb, timeout, 0.);
		ev_timer_start(EV_A_ &timeout_watcher);
	}
}

int main(int argc, char **argv)
{
	struct ev_loop *loop = EV_DEFAULT;

	/* parse options */
	rt_names_init();
	config_init(argv[0]);
	options_parse(argc, argv);

	/* kickstart event loop */
	timeout_init(EV_A);
	monitor_init(EV_A);
	sched_setup();
	sched_init();
	scan_init(EV_A);
	obj_rule_init();

	ev_run(EV_A_ 0);

	scan_fini(EV_A);

	ev_loop_destroy(EV_A);

	fr_printf(INFO, "Oops, we exited the event loop!\n");
	return EXIT_SUCCESS;
}
