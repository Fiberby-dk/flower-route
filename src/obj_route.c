// SPDX-License-Identifier: GPL-2.0-or-later

#include "obj_route.h"
#include "obj_target.h"
#include "obj_rule.h"
#include "tc_rule.h"

static struct rb_root obj_route_tree = RB_ROOT;
static int obj_route_cnt;

int obj_route_count(void)
{
	return obj_route_cnt;
}

static void obj_route_reap(struct obj_route *r)
{
	AN(r->obj.refcnt == 0);
	r->obj.state = OBJ_STATE_ZOMBIE;
	if (r->target) {
		obj_target_unlink_route(r->target, r);
		r->target = NULL;
	}
	if (r->rule) {
		obj_rule_unref(r->rule);
		r->rule = NULL;
	}
	if (r->target_rule) {
		obj_rule_unref(r->target_rule);
		r->target_rule = NULL;
	}
	rb_erase(&r->node, &obj_route_tree);
	obj_free(r);
	AN(obj_route_cnt--);
}

/* define helpers, after obj_route_reap */
obj_generic_ref(route, ROUTE)
obj_generic_unref(route, ROUTE)

static struct obj_route *obj_route_lookup(const struct af_addr *dst)
{
	struct rb_node *node = obj_route_tree.rb_node;
	struct obj_route *this;
	int ret;

	while (node) {
		this = rb_container_of(node, struct obj_route, node);
		ret = memcmp(dst, &this->dst, sizeof(struct af_addr));
		if (ret < 0)
			node = node->rb_left;
		else if (ret > 0)
			node = node->rb_right;
		else
			return this;
	}
	return NULL;
}

static int obj_route_insert(struct obj_route *r)
{
	struct rb_node **new = &(obj_route_tree.rb_node), *parent = NULL;
	struct obj_route *this;
	int ret;

	obj_assert_kind(r, ROUTE);

	/* Figure out where to put new node */
	while (*new) {
		this = rb_container_of(*new, struct obj_route, node);
		ret = memcmp(&r->dst, &this->dst, sizeof(struct af_addr));

		parent = *new;
		if (ret < 0)
			new = &((*new)->rb_left);
		else if (ret > 0)
			new = &((*new)->rb_right);
		else
			return 0;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&r->node, parent, new);
	rb_insert_color(&r->node, &obj_route_tree);

	return 1;
}

static void obj_route_new(struct obj_route *r)
{
	obj_set_state(route, r, INSTALLED);
	obj_route_insert(r);
}

static void obj_route_update(struct obj_route *r)
{
	obj_set_state(route, r, INSTALLED);
}

static void obj_route_delete(struct obj_route *r)
{
	obj_route_ref(r);
	obj_set_state(route, r, PRESENT);
	if (r->rule && r->rule->have)
		obj_rule_uninstall(r->rule);
	if (r->target)
		obj_target_unlink_route(r->target, r);
	obj_route_unref(r);
}

void obj_route_netlink_update(const uint16_t nlmsg_type, struct obj_target *t, const struct af_addr *dst)
{
	struct obj_route *r;
	int changes = 0;
	int is_new;

	AN(dst);
	AN(t);
	r = obj_route_lookup(dst);

	if (nlmsg_type == RTM_DELROUTE) {
		if (r)
			obj_route_delete(r);
		return;
	}

	is_new = r == NULL;
	if (is_new) {
		r = fr_malloc(sizeof(struct obj_route));
		obj_set_kind(r, ROUTE);
		obj_route_cnt++;
		memcpy(&r->dst, dst, sizeof(struct af_addr));
	}

	if (r->target != t) {
		if (r->target)
			obj_target_unlink_route(r->target, r);
		obj_target_link_route(t, r);
		changes++;
	}

	if (t->rule && t->rule->state == OBJ_RULE_STATE_OK && !r->rule)
		obj_route_install(r);

	if (is_new)
		obj_route_new(r);
	else if (changes > 0)
		obj_route_update(r);
}

static int obj_route_prepare_rule(struct obj_route *r, struct obj_rule *target_rule, struct tc_rule *tcr)
{
	AN(r);
	AN(target_rule);
	AN(target_rule->state == OBJ_RULE_STATE_OK);
	tc_rule_init(tcr);
	memcpy(&tcr->af_addr, &r->dst, sizeof(struct af_addr));
	tcr->goto_target = target_rule->chain_no;
	tc_rule_set_type_and_traits(tcr, TC_RULE_TYPE_ROUTE_GOTO);
	return true;
}

void obj_route_install(struct obj_route *r)
{
	obj_assert_kind(r, ROUTE);
	AN(r->target);
	struct obj_rule *target_rule = r->target->rule;
	struct tc_rule new_tcr = {0};
	int ret = obj_route_prepare_rule(r, target_rule, &new_tcr);

	if (ret) {
		if (r->rule) {
			struct tc_rule *current_tcr = r->rule->want;

			if (current_tcr && memcmp(current_tcr, &new_tcr, sizeof(struct tc_rule)) != 0) {
				AN(r->target_rule);
				struct obj_rule *old_rule = obj_rule_ref(r->rule);
				struct obj_rule *old_target_rule = obj_rule_ref(r->target_rule);

				if (r->target_rule != target_rule) {
					obj_rule_unref(r->target_rule);
					r->target_rule = obj_rule_ref(target_rule);
				}
				obj_rule_unref(r->rule);
				r->rule = obj_rule_request(&new_tcr);
				obj_rule_unref(old_target_rule);
				obj_rule_unref(old_rule);
			}
		} else {
			r->target_rule = obj_rule_ref(target_rule);
			r->rule = obj_rule_request(&new_tcr);
		}
	} else {
		if (r->target_rule) {
			obj_rule_unref(r->target_rule);
			r->target_rule = NULL;
		}
		if (r->rule)
			obj_rule_unref(r->rule);
	}
}
