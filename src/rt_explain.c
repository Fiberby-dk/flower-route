// SPDX-License-Identifier: GPL-2.0-or-later

#include <stddef.h>
#include <linux/rtnetlink.h>

#include "rt_explain.h"

const char *rt_explain_nud_state(unsigned char nud_state)
{
	switch (nud_state) {
	case NUD_INCOMPLETE:
		return "incomplete";
	case NUD_REACHABLE:
		return "reachable";
	case NUD_STALE:
		return "stale";
	case NUD_DELAY:
		return "delay";
	case NUD_PROBE:
		return "probe";
	case NUD_FAILED:
		return "failed";
	case NUD_NOARP:
		return "noarp";
	case NUD_PERMANENT:
		return "permanent";
	case NUD_NONE:
		return "none";
	default:
		return NULL;
	}
}

const char *rt_explain_rtm_type(unsigned char rtm_type)
{
	switch (rtm_type) {
	case RTN_UNICAST:
		return "unicast";
	case RTN_LOCAL:
		return "local";
	case RTN_BROADCAST:
		return "broadcast";
	case RTN_ANYCAST:
		return "anycast";
	case RTN_MULTICAST:
		return "multicast";
	case RTN_BLACKHOLE:
		return "blackhole";
	case RTN_UNREACHABLE:
		return "unreachable";
	case RTN_PROHIBIT:
		return "prohibit";
	case RTN_THROW:
		return "throw";
	case RTN_NAT:
		return "nat";
	case RTN_XRESOLVE:
		return "xresolve ";
	default:
		return NULL;
	}
}

const char *rt_explain_rtm_scope(unsigned char rtm_scope)
{
	switch (rtm_scope) {
	case RT_SCOPE_UNIVERSE:
		return "universe";
	case RT_SCOPE_SITE:
		return "site";
	case RT_SCOPE_LINK:
		return "link";
	case RT_SCOPE_HOST:
		return "bost";
	case RT_SCOPE_NOWHERE:
		return "nowhere";
	default:
		return NULL;
	}
}
