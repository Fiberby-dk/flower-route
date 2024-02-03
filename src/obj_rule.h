/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "obj.h"
#include "tc_rule.h"

void obj_rule_netlink_found(const uint16_t nlmsg_type, const uint32_t chain_no, const uint16_t prio, struct tc_rule *tcr);
void obj_rule_static_want(const uint32_t chain_no, const uint16_t prio, const struct tc_rule *tcr);
void obj_rule_print_all(void);
struct obj_rule *obj_rule_prime_request(const struct tc_rule *tcr);
void obj_rule_queue_request(struct obj_rule *r);
struct obj_rule *obj_rule_request(const struct tc_rule *tcr);
struct obj_rule *obj_rule_ref(struct obj_rule *r);
void obj_rule_unref(struct obj_rule *r);
void obj_rule_remove_pin(void);
uint16_t obj_rule_find_available_prio(const uint32_t chain_no, const uint16_t min_prio);
void obj_rule_set_target(struct obj_rule *r, struct obj_target *t);
void obj_rule_unset_target(struct obj_rule *r);
void obj_rule_uninstall(struct obj_rule *r);
int obj_rule_count(void);
void obj_rule_init(void);
void obj_rule_reset_pin(void);
void obj_rule_clear_all(void);
struct obj_rule *obj_rule_pos_lookup(const uint32_t chain_no, const uint16_t prio);
