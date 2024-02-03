// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"
#include "tc_explain.h"
#include <linux/pkt_cls.h>

// action_n2a from iproute2 tc/tc_util.c
const char *tc_explain_action(int action)
{
	static char buf[64];

	if (TC_ACT_EXT_CMP(action, TC_ACT_GOTO_CHAIN))
		return "goto";
	if (TC_ACT_EXT_CMP(action, TC_ACT_JUMP))
		return "jump";
	switch (action) {
	case TC_ACT_UNSPEC:
		return "continue";
	case TC_ACT_OK:
		return "pass";
	case TC_ACT_SHOT:
		return "drop";
	case TC_ACT_RECLASSIFY:
		return "reclassify";
	case TC_ACT_PIPE:
		return "pipe";
	case TC_ACT_STOLEN:
		return "stolen";
	case TC_ACT_TRAP:
		return "trap";
	default:
		snprintf(buf, 64, "%d", action);
		return buf;
	}
}

