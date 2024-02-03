// SPDX-License-Identifier: GPL-2.0-or-later

#include "nl_common.h"

#include <libmnl/libmnl.h>
#include <errno.h>
#include <stdio.h>
#include <linux/rtnetlink.h>
#include <time.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "nl_send.h"
#include "nl_conn.h"

static void nl_conn_set_name(struct conn *c, const char *name)
{
	AZ(c->name);
	c->name = strdup(name);
	AN(c->name);
}

struct conn *nl_conn_open(int groups, struct conn *reuse_conn, const char *name)
{
	int fd;
	int val = 1;
	int rcvbuf = 0x1000000; // 16 MiB
	struct conn *c = reuse_conn;

	if (c == NULL)
		c = fr_malloc(sizeof(struct conn));

	nl_conn_set_name(c, name);

	AZ(c->nl);
	c->nl = mnl_socket_open(NETLINK_ROUTE);
	if (c->nl == NULL) {
		perror("mnl_socket_open");
		goto error_out;
	}

	fd = mnl_socket_get_fd(c->nl);

	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) == -1) {
		perror("fcntl: SO_RCVBUF");
		goto error_out;
	}

	if (mnl_socket_setsockopt(c->nl, NETLINK_GET_STRICT_CHK, &val, sizeof(val)) == -1) {
		perror("mnl_socket_setsockopt: NETLINK_GET_STRICT_CHK");
		goto error_out;
	}

	if (mnl_socket_setsockopt(c->nl, NETLINK_EXT_ACK, &val, sizeof(val)) == -1) {
		perror("mnl_socket_setsockopt: NETLINK_EXT_ACK");
		goto error_out;
	}

	if (mnl_socket_setsockopt(c->nl, NETLINK_CAP_ACK, &val, sizeof(val)) == -1) {
		perror("mnl_socket_setsockopt: NETLINK_CAP_ACK");
		goto error_out;
	}

	if (mnl_socket_bind(c->nl, groups, MNL_SOCKET_AUTOPID) == -1) {
		perror("mnl_socket_bind");
		goto error_out;
	}

	if (fcntl(fd, F_SETFL, SOCK_NONBLOCK, &val) == -1) {
		perror("fcntl: SOCK_NONBLOCK");
		goto error_out;
	}

	ev_io_init(&c->w, NULL, fd, EV_READ);
	if (groups == 0) {
		c->portid = mnl_socket_get_portid(c->nl);
		c->seq = time(NULL) ^ c->portid;
	}

	return c;
error_out:
	if (reuse_conn == NULL)
		free(c);
	return NULL;
}

void nl_conn_close(EV_P_ struct conn *c)
{
	ev_io_stop(EV_A_ &c->w);
	mnl_socket_close(c->nl);
	c->nl = NULL;

	AN(c->name);
	free(c->name);
	c->name = NULL;
}

const char *nl_conn_get_name(struct conn *c)
{
	return c->name;
}
