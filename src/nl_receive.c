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

#include "nl_receive.h"
#include "nl_decode.h"
#include "nl_decode_common.h"

/* error handling inspired by Linux's tools/net/ynl/lib/ynl.c */

static const struct type_map nlmsgerr_attr_types[NLMSGERR_ATTR_MAX+1] = {
	TYPE_MAP(NLMSGERR_ATTR_MSG,       NUL_STRING),
	TYPE_MAP(NLMSGERR_ATTR_OFFS,      U32),
	TYPE_MAP(NLMSGERR_ATTR_MISS_TYPE, U32),
	TYPE_MAP(NLMSGERR_ATTR_MISS_NEST, U32),
};
decode_nlattr_cb(nlmsgerr, NLMSGERR_ATTR_MAX, true)

static int my_ext_ack_check(unsigned int code, const struct nlmsghdr *nlh, unsigned int hlen)
{
	struct nlattr *tb[NLMSGERR_ATTR_MAX+1] = {0};
	int ret;

	fr_printf(ERROR, "Netlink error: %d (%s)\n", code, strerror(code));

	if (!(nlh->nlmsg_flags & NLM_F_ACK_TLVS))
		return MNL_CB_OK;

	ret = mnl_attr_parse(nlh, hlen, decode_nlattr_nlmsgerr_cb, tb);
	if (ret != MNL_CB_OK)
		return ret;

	if (tb[NLMSGERR_ATTR_MSG])
		fr_printf(ERROR, "Netlink error message: %s\n", mnl_attr_get_str(tb[NLMSGERR_ATTR_MSG]));

	if (tb[NLMSGERR_ATTR_OFFS] || tb[NLMSGERR_ATTR_MISS_TYPE] || tb[NLMSGERR_ATTR_MISS_NEST])
		fr_printf(DEBUG2, "Netlink error: got additional info\n");

	return MNL_CB_OK;
}

static int my_mnl_cb_noop(const struct nlmsghdr *nlh, void *data)
{
	fr_unused(nlh);
	fr_unused(data);
	return MNL_CB_OK;
}

static int my_mnl_cb_done(const struct nlmsghdr *nlh, void *data)
{
	fr_unused(data);
	int err = *(int *)NLMSG_DATA(nlh);

	if (err < 0) {
		my_ext_ack_check(-err, nlh, sizeof(int));
		return MNL_CB_ERROR;
	}
	return MNL_CB_STOP;
}

static int my_mnl_cb_error(const struct nlmsghdr *nlh, void *data)
{
	fr_unused(data);
	const struct nlmsgerr *err = mnl_nlmsg_get_payload(nlh);
	int code;

	if (nlh->nlmsg_len < mnl_nlmsg_size(sizeof(struct nlmsgerr))) {
		errno = EBADMSG;
		return MNL_CB_ERROR;
	}
	code = err->error >= 0 ? err->error : -err->error;

	AN(nlh->nlmsg_flags & NLM_F_CAPPED);

	if (code != 0)
		my_ext_ack_check(code, nlh, sizeof(*err));

	errno = code;

	return code ? MNL_CB_ERROR : MNL_CB_STOP;
}

static const mnl_cb_t my_mnl_cb_array[] = {
	[NLMSG_NOOP]    = my_mnl_cb_noop,
	[NLMSG_ERROR]   = my_mnl_cb_error,
	[NLMSG_DONE]    = my_mnl_cb_done,
	[NLMSG_OVERRUN] = my_mnl_cb_noop,
};

void nl_receive_cb(EV_P_ struct ev_io *w, int revents)
{
	fr_unused(revents);
	struct conn *c = (struct conn *) w;
	char buf[MNL_SOCKET_DUMP_SIZE];
	struct mnl_socket *nl = c->nl;
	int has_completed = false;
	int retval = 0;
	int ret;
	int len;

	AN(c->on_complete);
	len = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	while (len > 0) {
		ret = mnl_cb_run2(buf, len, c->seq, c->portid, decode_nlmsg_cb, c, my_mnl_cb_array, MNL_ARRAY_SIZE(my_mnl_cb_array));
		if (ret == MNL_CB_OK) {
			fr_printf(DEBUG2, "mnl_cb_run: %d (MNL_CB_OK)\n", ret);
		} else if (ret == MNL_CB_STOP) {
			fr_printf(DEBUG2, "mnl_cb_run: %d (MNL_CB_STOP)\n", ret);
			has_completed = true;
			break;
		} else if (ret == MNL_CB_ERROR) {
			retval = errno;
			fr_printf(DEBUG2, "mnl_cb_run: %d (%s)\n", errno, strerror(errno));
			has_completed = true;
			break;
		} else if (ret < 0) {
			fr_printf(DEBUG2, "mnl_cb_run: %d\n", ret);
			break;
		}
		len = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	}
	if (len == -1 && errno == EAGAIN)
		return;

	ev_io_stop(EV_A_ &c->w);

	if (has_completed)
		c->on_complete(EV_A_ c, retval);
}
