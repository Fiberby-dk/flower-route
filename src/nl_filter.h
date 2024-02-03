/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef FLOWER_ROUTE_NL_FILTER_H
#define FLOWER_ROUTE_NL_FILTER_H
#include "nl_common.h"
#include "rbtree.h"

void filter_dump(EV_P_ struct conn *c);
void filter_dump_chains(EV_P_ struct conn *c);
void filter_dump_chain(EV_P_ struct conn *c, uint32_t chain_no);

void filter_got_qdisc(void);
void filter_got_chain(uint32_t chain_no);

extern struct rb_root chain_tree;

enum chain_state {
	CHAIN_FLAG_UNKNOWN,
	CHAIN_STATE_PRESENT,
	CHAIN_STATE_RESERVED,
};

struct chain {
	uint32_t chain_no;
	uint8_t state;
	struct rb_node node;
};

struct chain *chain_lookup(const uint32_t chain_no);
uint32_t filter_find_available_chain_no(const uint32_t min_chain_no);
void filter_clear_chains(void);

#endif
