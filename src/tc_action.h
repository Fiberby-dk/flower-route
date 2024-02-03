/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "common.h"
#include "tc_rule.h"

struct tc_action_callbacks {
	void (*install)(EV_P_ const uint32_t chain_no, const uint16_t prio, struct tc_rule *tcr);
	void (*pre_install)(void *data);
	void (*post_install)(void *data);
	void (*done)(void *data, const int nl_errno);
};

struct tc_action_callbacks *tc_action_get_callbacks(void);

void tc_action_install(const uint32_t chain_no, const uint16_t prio, struct tc_rule *tcr, void *data);
