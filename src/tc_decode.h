/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "nl_common.h"
#include "tc_rule.h"

struct tc_decoded_rule {
	int is_done;
	uint32_t chain_no;
	uint16_t prio;
	struct tc_rule tcr;
};

int decode_qdisc(const struct nlmsghdr *nlh, struct conn *c);
int decode_chain(const struct nlmsghdr *nlh, struct conn *c);
int decode_filter(const struct nlmsghdr *nlh, struct conn *c);
struct tc_decoded_rule *decode_filter2(const struct nlmsghdr *nlh);
