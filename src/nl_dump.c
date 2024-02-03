// SPDX-License-Identifier: GPL-2.0-or-later

#include "nl_common.h"
#include "nl_dump.h"
#include "nl_send.h"

void nl_dump_link(EV_P_ struct conn *c)
{
	struct nlmsghdr *nlh;
	char buf[MNL_SOCKET_DUMP_SIZE];
	struct ifinfomsg *ifm;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

	ifm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct ifinfomsg));
	ifm->ifi_family = AF_UNSPEC;

	nl_send_req(EV_A_ c, nlh);
}

void nl_dump_neigh(EV_P_ struct conn *c, uint8_t af)
{
	struct nlmsghdr *nlh;
	char buf[MNL_SOCKET_DUMP_SIZE];
	struct ndmsg *ndm;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_GETNEIGH;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

	ndm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct ndmsg));
	ndm->ndm_family = af;

	nl_send_req(EV_A_ c, nlh);
}

void nl_dump_route(EV_P_ struct conn *c, uint8_t af)
{
	struct nlmsghdr *nlh;
	char buf[MNL_SOCKET_DUMP_SIZE];
	struct rtmsg *rtm;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_GETROUTE;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

	rtm = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtmsg));
	rtm->rtm_family = af;

	/* 32 bit routing table */
	rtm->rtm_table = RT_TABLE_UNSPEC;
	mnl_attr_put_u32(nlh, RTA_TABLE, config->table_id);

	nl_send_req(EV_A_ c, nlh);
}

