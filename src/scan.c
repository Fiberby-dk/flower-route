// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"
#include "nl_common.h"

#include "nl_conn.h"
#include "nl_dump.h"
#include "nl_filter.h"
#include "nl_queue.h"

#include "obj_rule.h"

#include "scan.h"

enum scan_state {
	SCAN_NEW,
	SCAN_RUN_HELPERS,
	SCAN_DUMP_CHAINS,
	SCAN_DUMP_EACH_CHAIN_INIT,
	SCAN_DUMP_EACH_CHAIN,
	SCAN_DONE,
	SCAN_WAIT,
};

struct scan {
	struct conn c;
	enum scan_state state;
	int helper_idx;
	struct rb_node *next_chain;
	uint32_t q_chain_no;
	ev_timer timer;
};

static void scan_links(EV_P_ void *data) { struct scan *s = data; nl_dump_link(EV_A_ &s->c); }
static void scan_neigh4(EV_P_ void *data) { struct scan *s = data; nl_dump_neigh(EV_A_ &s->c, AF_INET); }
static void scan_neigh6(EV_P_ void *data) { struct scan *s = data; nl_dump_neigh(EV_A_ &s->c, AF_INET6); }
static void scan_route4(EV_P_ void *data) { struct scan *s = data; nl_dump_route(EV_A_ &s->c, AF_INET); }
static void scan_route6(EV_P_ void *data) { struct scan *s = data; nl_dump_route(EV_A_ &s->c, AF_INET6); }
static void scan_chains(EV_P_ void *data) { struct scan *s = data; filter_dump_chains(EV_A_ &s->c); }

static void scan_break(EV_P_ void *data)
{
	fr_unused(data);
	ev_break(EV_A_ EVBREAK_ALL);
}

static void scan_chain(EV_P_ void *data)
{
	struct scan *s = data;

	filter_dump_chain(EV_A_ &s->c, s->q_chain_no);
}

static void scan_filters(EV_P_ void *data)
{
	struct scan *s = data;

	filter_dump(EV_A_ &s->c);
	s->state = SCAN_DUMP_CHAINS;
}

static void (*scan_helpers[])(EV_P_ void *data) = {
	scan_filters,
	scan_links,
	scan_neigh4,
	scan_neigh6,
	scan_route4,
	scan_route6,
	NULL
};

static void advance_scan(EV_P_ struct scan *s);

static void scan_timeout_cb(EV_P_ ev_timer *w, int revents)
{
	struct scan *s;

	fr_unused(revents);
	s = rb_container_of(w, struct scan, timer);
	ev_timer_stop(EV_A_ &s->timer);

	AN(s->state == SCAN_WAIT);
	s->state = SCAN_RUN_HELPERS;

	advance_scan(EV_A_ s);
}

static void advance_scan_cb(EV_P_ void *data, int nl_errno)
{
	fr_unused(nl_errno);
	struct scan *s = data;

	advance_scan(EV_A_ s);
}

static void advance_scan(EV_P_ struct scan *s)
{
	struct chain *ch;

	AN(s);
	while (1) {
		switch (s->state) {
		case SCAN_NEW:
			nl_conn_open(0, &s->c, "scan");
			queue_init(&s->c);
			s->state = SCAN_RUN_HELPERS;
			/* fall-through */
		case SCAN_RUN_HELPERS:
			fr_printf(DEBUG2, "SCAN_RUN_HELPERS\n");
			void (*helper)(EV_P_ void *data) = scan_helpers[s->helper_idx];

			if (helper != NULL) {
				queue_schedule(EV_A_ helper, advance_scan_cb, s);
				s->helper_idx++;
				return;
			}
			s->helper_idx = 0;
			s->state = SCAN_DONE;
			break;
		case SCAN_DUMP_CHAINS:
			fr_printf(DEBUG2, "SCAN_DUMP_CHAINS\n");
			s->state = SCAN_DUMP_EACH_CHAIN_INIT;
			queue_schedule(EV_A_ scan_chains, advance_scan_cb, s);
			return;
		case SCAN_DUMP_EACH_CHAIN_INIT:
			fr_printf(DEBUG2, "SCAN_DUMP_EACH_CHAIN_INIT\n");
			s->next_chain = rb_first(&chain_tree);
			s->state = s->next_chain == NULL ? SCAN_RUN_HELPERS : SCAN_DUMP_EACH_CHAIN;
			break;
		case SCAN_DUMP_EACH_CHAIN:
			fr_printf(DEBUG2, "SCAN_DUMP_EACH_CHAIN\n");
			AN(s->next_chain);
			ch = rb_container_of(s->next_chain, struct chain, node);
			fr_printf(DEBUG2, "dumping chain: %"PRIu32"\n", ch->chain_no);
			s->q_chain_no = ch->chain_no;
			queue_schedule(EV_A_ scan_chain, advance_scan_cb, s);

			s->next_chain = rb_next(s->next_chain);
			if (s->next_chain == NULL)
				s->state = SCAN_RUN_HELPERS;
			return;
		case SCAN_DONE:
			fr_printf(DEBUG2, "SCAN_DONE\n");
			obj_rule_remove_pin();
			obj_rule_print_all();
			s->state = SCAN_WAIT;
			break;
		case SCAN_WAIT:
			fr_printf(DEBUG2, "SCAN_WAIT\n");

			if (config->exit_after_first_sync)
				queue_schedule(EV_A_ scan_break, NULL, NULL);

			ev_timer_again(EV_A_ &s->timer);
			return;
		}
	}
}

void scan_init(EV_P)
{
	struct scan *s = fr_malloc(sizeof(struct scan));
	double scan_interval = config->scan_interval;

	ev_timer_init(&s->timer, scan_timeout_cb, 0., scan_interval);
	advance_scan(EV_A_ s);
}

void scan_fini(EV_P)
{
	struct conn *c = queue_get_conn();
	struct scan *s = rb_container_of(c, struct scan, c);

	queue_fini();
	if (s) {
		ev_timer_stop(EV_A_ &s->timer);
		nl_conn_close(EV_A_ c);
		free(s);
	}
}
