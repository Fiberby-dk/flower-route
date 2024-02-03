// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"
#include "nl_common.h"

#include <libmnl/libmnl.h>
#include <errno.h>

void common_netlink_set_seq(struct nlmsghdr *nlh, struct conn *c)
{
	AN(c->seq > 0);

	nlh->nlmsg_seq = ++c->seq;
}
