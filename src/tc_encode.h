/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "nl_common.h"
#include "nl_filter.h"
#include "tc_rule.h"

enum {
	NO_TCE_FLAGS      = 0,
	TCE_FLAG_LOOPBACK = 1<<0,
};

void tc_encode_drop_chain(struct nlmsghdr *nlh, const uint32_t chain_no, int flags);
void tc_encode_rule(struct nlmsghdr *nlh, const uint32_t chain_no, const uint16_t prio, const struct tc_rule *tcr, int flags);
