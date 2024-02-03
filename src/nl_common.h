/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef FLOWER_ROUTE_NL_COMMON_H
#define FLOWER_ROUTE_NL_COMMON_H

#include "common.h"
#include <ev.h>
#include <libmnl/libmnl.h>

#ifndef MNL_SOCKET_DUMP_SIZE
#define MNL_SOCKET_DUMP_SIZE    32768
#endif

struct conn {
	struct ev_io w;
	struct mnl_socket *nl;
	unsigned int portid;
	unsigned int seq;
	void (*on_complete)(EV_P_ struct conn *c, int nl_errno);
	void (*on_send_req)(struct conn *c);
	int busy;
	int queue_pos;
	char *name;
};

void common_netlink_set_seq(struct nlmsghdr *nlh, struct conn *c);

#endif
