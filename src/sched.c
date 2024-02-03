// SPDX-License-Identifier: GPL-2.0-or-later

#include "sched.h"
#include "sched_basic.h"

static struct sched_ops *ops;

void sched_setup(void)
{
	ops = (struct sched_ops *) sched_basic_setup();

	/* check ops */
	AN(ops);
	AN(ops->init);
	AN(ops->place);
}

void sched_init(void)
{
	AN(ops);
	ops->init();
}

int sched_place(const struct tc_rule *tcr, uint32_t *chain_no, uint16_t *prio)
{
	AN(ops);
	return ops->place(tcr, chain_no, prio);
}
