// SPDX-License-Identifier: GPL-2.0-or-later

#include "nl_common.h"

#include <libmnl/libmnl.h>
#include <errno.h>
#include <stdio.h>
#include <linux/rtnetlink.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "nl_send.h"
#include "nl_receive.h"
#include "nl_decode.h"
#include "nl_decode_common.h"

int nl_send_req(EV_P_ struct conn *c, struct nlmsghdr *nlh)
{
	common_netlink_set_seq(nlh, c);

	if (mnl_socket_sendto(c->nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_sendto");
		return -1;
	}

	if (c->on_send_req)
		c->on_send_req(c);

	c->w.cb = nl_receive_cb;
	ev_io_start(EV_A_ &c->w);

	return 0;
}
