// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"
#include "monitor.h"
#include "nl_conn.h"
#include "nl_receive.h"

struct monitor {
	struct conn c;
};

static void monitor_complete(EV_P_ struct conn *c, int nl_errno)
{
	fr_unused(c);
	fr_unused(nl_errno);
	fr_printf(ERROR, "Boo hun\n");
}

static void monitor_response_read_cb(EV_P_ struct ev_io *w, int revents)
{
	nl_receive_cb(EV_A_ w, revents);
	ev_io_start(EV_A_ w); /* wait for next event */
}

void monitor_init(EV_P)
{
	struct monitor *m = fr_malloc(sizeof(struct monitor));
	struct conn *c = &m->c;
	int groups = 0;

	groups |= RTNLGRP_LINK;
	groups |= RTNLGRP_NEIGH;
	groups |= RTNLGRP_TC;
	groups |= RTNLGRP_IPV4_ROUTE;
	groups |= RTNLGRP_IPV6_ROUTE;

	nl_conn_open(groups, c, "monitor");
	c->on_complete = monitor_complete;
	c->w.cb = monitor_response_read_cb;
	ev_io_start(EV_A_ &c->w);
}
