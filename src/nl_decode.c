// SPDX-License-Identifier: GPL-2.0-or-later

#include "nl_common.h"
#include "rt_explain.h"

#include "nl_conn.h"
#include "nl_filter.h"
#include "nl_decode.h"
#include "tc_decode.h"
#include "nl_decode_common.h"
#include "obj_link.h"
#include "obj_neigh.h"
#include "obj_route.h"
#include "obj_target.h"

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

static const struct type_map link_attr_types[IFLA_MAX+1] = {
	TYPE_MAP(IFLA_ADDRESS,    BINARY),
	TYPE_MAP(IFLA_MTU,        U32),
	TYPE_MAP(IFLA_IFNAME,     NUL_STRING),
	TYPE_MAP(IFLA_LINK,       U32),
	TYPE_MAP(IFLA_LINKINFO,   NESTED),
	TYPE_MAP(IFLA_PROP_LIST,  NESTED),
};
decode_nlattr_cb(link, IFLA_MAX, false)

static const struct type_map link_info_attr_types[IFLA_INFO_MAX+1] = {
	TYPE_MAP(IFLA_INFO_KIND, NUL_STRING),
	TYPE_MAP(IFLA_INFO_DATA, NESTED),
};
decode_nlattr_cb(link_info, IFLA_INFO_MAX, true)

static const struct type_map link_vlan_attr_types[IFLA_VLAN_MAX+1] = {
	TYPE_MAP(IFLA_VLAN_ID,       U16),
	TYPE_MAP(IFLA_VLAN_FLAGS,    BINARY),
	TYPE_MAP(IFLA_VLAN_PROTOCOL, U16),
};
decode_nlattr_cb(link_vlan, IFLA_VLAN_MAX, true)

static const struct type_map neigh_attr_types[NDA_MAX+1] = {
	TYPE_MAP(NDA_DST,       BINARY),
	TYPE_MAP(NDA_LLADDR,    BINARY),
	TYPE_MAP(NDA_CACHEINFO, BINARY),
	TYPE_MAP(NDA_PROBES,    U32),
};
decode_nlattr_cb(neigh, NDA_MAX, true)

static const struct type_map route4_attr_types[RTAX_MAX+1] = {
	TYPE_MAP(RTA_TABLE,     U32),
	TYPE_MAP(RTA_OIF,       U32),
	TYPE_MAP(RTA_FLOW,      U32),
	TYPE_MAP(RTA_PRIORITY,  U32),
	TYPE_MAP(RTA_DST,       U32),
	TYPE_MAP(RTA_SRC,       U32),
	TYPE_MAP(RTA_PREFSRC,   U32),
	TYPE_MAP(RTA_GATEWAY,   U32),
	TYPE_MAP(RTA_METRICS,   NESTED),
	TYPE_MAP(RTA_MULTIPATH, BINARY),
};
decode_nlattr_cb(route4, RTAX_MAX, true)

static const struct type_map route6_attr_types[RTAX_MAX+1] = {
	TYPE_MAP(RTA_TABLE,     U32),
	TYPE_MAP(RTA_OIF,       U32),
	TYPE_MAP(RTA_FLOW,      U32),
	TYPE_MAP(RTA_PRIORITY,  U32),
	TYPE_MAP(RTA_DST,       BINARY),
	TYPE_MAP(RTA_SRC,       BINARY),
	TYPE_MAP(RTA_PREFSRC,   BINARY),
	TYPE_MAP(RTA_GATEWAY,   BINARY),
	TYPE_MAP(RTA_METRICS,   NESTED),
	TYPE_MAP(RTA_MULTIPATH, BINARY),
	TYPE_MAP(RTA_CACHEINFO, BINARY),
};
decode_nlattr_cb(route6, RTAX_MAX, true)

static int decode_link(const struct nlmsghdr *nlh, struct conn *c)
{
	struct nlattr *tb[IFLA_MAX+1] = {0};
	struct nlattr *tb_info[IFLA_INFO_MAX+1] = {0};
	struct nlattr *tb_vlan[IFLA_VLAN_MAX+1] = {0};
	struct ifinfomsg *ifi = mnl_nlmsg_get_payload(nlh);
	int ret;
	int lower_ifindex;
	const char *ifname;
	const uint8_t (*lladdr)[ETH_ALEN];
	uint32_t mtu;
	int is_vlan;
	uint16_t vlan_id;

	if (ifi->ifi_type != ARPHRD_ETHER)
		return MNL_CB_OK;

	ret = mnl_attr_parse(nlh, sizeof(*ifi), decode_nlattr_link_cb, tb);
	if (ret != MNL_CB_OK)
		return ret;

	lower_ifindex = tb[IFLA_LINK] ? mnl_attr_get_u32(tb[IFLA_LINK]) : 0;
	if (lower_ifindex != config->ifidx)
		return MNL_CB_OK;

	ifname = tb[IFLA_IFNAME] ? mnl_attr_get_str(tb[IFLA_IFNAME]) : NULL;
	lladdr = tb[IFLA_ADDRESS] ? mnl_attr_get_payload(tb[IFLA_ADDRESS]) : NULL;
	mtu = tb[IFLA_MTU] ? mnl_attr_get_u32(tb[IFLA_MTU]) : 0;

	is_vlan = 0;
	if (tb[IFLA_LINKINFO]) {
		ret = mnl_attr_parse_nested(tb[IFLA_LINKINFO], decode_nlattr_link_info_cb, tb_info);
		if (ret != MNL_CB_OK)
			return ret;

		if (tb_info[IFLA_INFO_KIND]) {
			const char *kind = mnl_attr_get_str(tb_info[IFLA_INFO_KIND]);

			if (strcmp(kind, "vlan") == 0)
				is_vlan = 1;
		}
	}

	if (is_vlan) {
		struct nlattr *info_data = tb_info[IFLA_INFO_DATA];

		ret = mnl_attr_parse_nested(info_data, decode_nlattr_link_vlan_cb, tb_vlan);
		if (ret != MNL_CB_OK)
			return ret;
	}

	vlan_id = tb_vlan[IFLA_VLAN_ID] ? mnl_attr_get_u16(tb_vlan[IFLA_VLAN_ID]) : 0;

	obj_link_netlink_update(nlh->nlmsg_type, ifi->ifi_index, lladdr, lower_ifindex, vlan_id, mtu, ifname);
	fr_printf(INFO, "\n");

	return MNL_CB_OK;
}

static struct obj_target *decode_multipath(const struct nlattr *attr, const uint8_t af, int (*attr_cb)(const struct nlattr *, void *))
{
	size_t len = mnl_attr_get_payload_len(attr);
	struct rtnexthop *rtnh = mnl_attr_get_payload(attr);
	struct obj_nexthop *first, **last;

	/* TODO validate the assumtion: multipath is always returned in sort order from the kernel */

	while (rtnh && RTNH_OK(rtnh, len)) {
		struct nlattr *tb[RTAX_MAX+1] = {0};
		size_t attrs_len = rtnh->rtnh_len - sizeof(struct rtnexthop);
		int ret = mnl_attr_parse_payload(RTNH_DATA(rtnh), attrs_len, attr_cb, tb);

		if (ret != MNL_CB_OK)
			return NULL;

		int oif = rtnh->rtnh_ifindex;
		void *nh = mnl_attr_get_payload(tb[RTA_GATEWAY]);
		struct obj_neigh *n = obj_neigh_netlink_get(oif, af, nh);

		if (!n)
			return NULL; /* unknown link */

		if (n) {
			/* TODO don't just treat multipath as unipath */
			return obj_target_get_unipath(n);
		}

		/* prepare next round */
		len -= NLMSG_ALIGN(rtnh->rtnh_len);
		rtnh = len >= sizeof(struct rtnexthop) ? RTNH_NEXT(rtnh) : NULL;
	}

	return NULL;
}

static int decode_route(const struct nlmsghdr *nlh, struct conn *c)
{
	int (*attr_cb)(const struct nlattr *attr, void *data);
	struct rtmsg *rm = mnl_nlmsg_get_payload(nlh);
	struct nlattr *tb[RTAX_MAX+1] = {0};
	struct obj_target *t = NULL;
	int ret;

	switch (rm->rtm_family) {
	case AF_INET:
		attr_cb = decode_nlattr_route4_cb;
		break;
	case AF_INET6:
		attr_cb = decode_nlattr_route6_cb;
		break;
	default:
		return MNL_CB_OK;
	}

	ret = mnl_attr_parse(nlh, sizeof(*rm), attr_cb, tb);
	if (ret != MNL_CB_OK)
		return ret;

	AN(tb[RTA_TABLE]);
	if (mnl_attr_get_u32(tb[RTA_TABLE]) != config->table_id)
		return MNL_CB_OK;

	if (!tb[RTA_DST])
		return MNL_CB_OK;

	if (tb[RTA_MULTIPATH]) {
		/* multipath */
		t = decode_multipath(tb[RTA_MULTIPATH], rm->rtm_family, attr_cb);
	} else if (tb[RTA_OIF] && tb[RTA_GATEWAY]) {
		/* unipath */
		int oif = mnl_attr_get_u32(tb[RTA_OIF]);
		void *nh = mnl_attr_get_payload(tb[RTA_GATEWAY]);
		struct obj_neigh *n = obj_neigh_netlink_get(oif, rm->rtm_family, nh);

		if (!n)
			return MNL_CB_OK; /* unknown link */
		t = obj_target_get_unipath(n);
	} else {
		/* ??? */
		fr_printf(DEBUG2, "Recieved weird route from netlink\n");
	}

	if (t) {
		struct af_addr af_dst;
		void *dst = mnl_attr_get_payload(tb[RTA_DST]);

		build_af_addr(&af_dst, rm->rtm_family, dst, rm->rtm_dst_len);
		obj_route_netlink_update(nlh->nlmsg_type, t, &af_dst);
	}

	//fr_printf(DEBUG2, "%s: got route\n", nl_conn_get_name(c));
	//obj_route_netlink_update(nlh->nlmsg_type, rm, dst, nh, oif);
	//struct obj_target *target = obj_target_netlink_get(oif, rm->rtm_family, nh);

	return MNL_CB_OK;
}

static int decode_neigh(const struct nlmsghdr *nlh, struct conn *c)
{
	struct nlattr *tb[NDA_MAX+1] = {0};
	struct ndmsg *ndm = mnl_nlmsg_get_payload(nlh);
	int ret = mnl_attr_parse(nlh, sizeof(*ndm), decode_nlattr_neigh_cb, tb);
	const uint8_t (*lladdr)[ETH_ALEN];
	void *addr = NULL;

	if (ret != MNL_CB_OK)
		return ret;

	lladdr = tb[NDA_LLADDR] ? mnl_attr_get_payload(tb[NDA_LLADDR]) : NULL;

	if (tb[NDA_DST])
		addr = mnl_attr_get_payload(tb[NDA_DST]);

	obj_neigh_netlink_update(nlh->nlmsg_type, ndm->ndm_ifindex, ndm->ndm_family, addr, lladdr);

	return MNL_CB_OK;
}

int decode_nlmsg_cb(const struct nlmsghdr *nlh, void *data)
{
	struct conn *c = data;

	switch (nlh->nlmsg_type) {
	case RTM_NEWLINK:
	case RTM_DELLINK:
		return decode_link(nlh, c);

	case RTM_NEWROUTE:
	case RTM_DELROUTE:
		return decode_route(nlh, c);

	case RTM_NEWNEIGH:
	case RTM_DELNEIGH:
	case RTM_GETNEIGH:
		return decode_neigh(nlh, c);

	case RTM_NEWQDISC:
	case RTM_DELQDISC:
		return decode_qdisc(nlh, c);

	case RTM_NEWCHAIN:
	case RTM_DELCHAIN:
		return decode_chain(nlh, c);

	case RTM_NEWTFILTER:
	case RTM_DELTFILTER:
		return decode_filter(nlh, c);

	default:
		fr_printf(DEBUG1, "no handler for nlmsg_type %d\n", nlh->nlmsg_type);
		break;
	}

	return MNL_CB_OK;
}
