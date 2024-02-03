/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef FLOWER_ROUTE_SCHED_H
#define FLOWER_ROUTE_SCHED_H

#include "tc_rule.h"

struct sched_ops {
	int (*place)(const struct tc_rule *tcr, uint32_t *chain_no, uint16_t *prio);
	void (*init)(void);
};

void sched_setup(void);
void sched_init(void);
int sched_place(const struct tc_rule *tcr, uint32_t *chain_no, uint16_t *prio);

#endif
