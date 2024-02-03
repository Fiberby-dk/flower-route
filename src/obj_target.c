// SPDX-License-Identifier: GPL-2.0-or-later

#include "obj_target.h"
#include "obj_neigh.h"
#include "obj_route.h"
#include "obj_rule.h"
#include "tc_rule.h"

static int obj_target_cnt;

int obj_target_count(void)
{
	return obj_target_cnt;
}

static void obj_target_reap(struct obj_target *t)
{
	//struct obj_link *l = t->link;
	t->obj.state = OBJ_STATE_ZOMBIE;
	AN(t->obj.refcnt == 0);
	/* TODO some state assert */
	/* TODO sanity check for leak */
	for (struct obj_nexthop *nh = t->nexthop, *next_nh; nh; nh = next_nh) {
		AN(nh);
		next_nh = nh->next;
		obj_neigh_weak_unref(nh->neigh);
		free(nh);
	}
	for (struct obj_route *r = t->first_route, *nr; r; r = nr) {
		nr = r->t_next_route;
		obj_route_unref(r);
	}
	if (t->rule) {
		obj_rule_unref(t->rule);
		t->rule = NULL;
	}
	AN(t->obj.weak_refcnt == 0);
	obj_free(t);
	AN(obj_target_cnt--);
	//obj_link_unref(l);
}

/* define helpers, after obj_target_reap */
obj_generic_ref(target, TARGET)
obj_generic_unref(target, TARGET)
obj_generic_weak_ref(target, TARGET)
obj_generic_weak_unref(target, TARGET)

static struct obj_target *obj_target_alloc(void)
{
	struct obj_target *t = fr_malloc(sizeof(struct obj_target));

	obj_set_kind(t, TARGET);
	obj_target_cnt++;
	return t;
}

struct obj_target *obj_target_get_unipath(struct obj_neigh *n)
{
	obj_assert_kind(n, NEIGH);
	struct obj_target *t = n->targets;

	if (t && t->nexthop_cnt == 1)
		return t;

	t = obj_target_alloc();
	t->nexthop_cnt = 1;
	t->nexthop = fr_malloc(sizeof(struct obj_nexthop));
	t->nexthop->neigh = obj_neigh_weak_ref(n);

	/* link target to the front of obj_neigh's linked list of targets */
	t->n_next_target = n->targets;
	n->targets = obj_target_ref(t);

	return t;
}

static inline bool is_lladdr_zero(const uint8_t *lladdr)
{
	return (lladdr[0] == 0 && lladdr[1] == 0 && lladdr[2] == 0 &&
			lladdr[3] == 0 && lladdr[4] == 0 && lladdr[5] == 0);
}

static int obj_target_prepare_rule(struct obj_target *t, struct tc_rule *tcr)
{
	struct obj_nexthop *nh = t->nexthop;

	AN(nh);
	struct obj_neigh *n = nh->neigh;

	AN(n);
	struct obj_link *l = n->link;

	/* XXX check NUD state */

	if (l->vlan_id == 0)
		return false;

	if (is_lladdr_zero(l->lladdr)) {
		fr_printf(DEBUG2, "skipping, link lladdr is zero\n");
		return false;
	}

	if (is_lladdr_zero(n->lladdr)) {
		fr_printf(DEBUG2, "skipping, neigh lladdr is zero\n");
		return false;
	}

	obj_target_print(t);
	tc_rule_init(tcr);
	tcr->vlan_id = l->vlan_id;
	memcpy(&tcr->lladdr.src, &l->lladdr, 6);
	memcpy(&tcr->lladdr.dst, &n->lladdr, 6);
	tcr->af_addr.af = n->addr.af;
	tc_rule_set_type_and_traits(tcr, TC_RULE_TYPE_FORWARD);
	return true;
}

static void obj_target_set_rule(struct obj_target *t, struct tc_rule *tcr)
{
	if (t->rule) {
		obj_rule_unset_target(t->rule);
		obj_rule_unref(t->rule);
		t->rule = NULL;
	}
	if (tcr) {
		t->rule = obj_rule_prime_request(tcr);
		if (t->rule) {
			obj_rule_set_target(t->rule, t);
			obj_rule_queue_request(t->rule);
		}
	} else {
		obj_target_notify_routes(t);
	}
}

static void obj_target_install(struct obj_target *t)
{
	obj_target_print(t);
	struct tc_rule tcr = {0};
	int ret = obj_target_prepare_rule(t, &tcr);

	if (!ret)
		return;
	tc_rule_print(&tcr);
	obj_target_set_rule(t, &tcr);
}

void obj_target_notify_routes(struct obj_target *t)
{
	obj_assert_kind(t, TARGET);
	if (t->rule && t->rule->state == OBJ_RULE_STATE_OK) {
		for (struct obj_route *r = t->first_route; r; r = r->t_next_route) {
			AN(r->target == t);
			obj_route_install(r);
		}
	}
}

void obj_target_neigh_update(struct obj_target *t)
{
	obj_assert_kind(t, TARGET);
	struct tc_rule new_tcr = {0};
	int ret = obj_target_prepare_rule(t, &new_tcr);

	if (ret) {
		if (t->rule) {
			struct tc_rule *current_tcr = t->rule->want;

			if (current_tcr && memcmp(current_tcr, &new_tcr, sizeof(struct tc_rule)) != 0)
				obj_target_set_rule(t, &new_tcr);
		}
	} else {
		obj_target_set_rule(t, NULL);
	}
}

void obj_target_link_route(struct obj_target *t, struct obj_route *r)
{
	obj_assert_kind(t, TARGET);
	obj_assert_kind(r, ROUTE);
	AZ(r->target);
	r->target = obj_target_weak_ref(t);
	obj_route_ref(r);
	if (t->last_route)
		t->last_route->t_next_route = r;
	else
		t->first_route = r;
	t->last_route = r;

	if (!t->rule)
		obj_target_install(t);
}

void obj_target_unlink_route(struct obj_target *t, struct obj_route *r)
{
	obj_assert_kind(t, TARGET);
	obj_assert_kind(r, ROUTE);
	obj_target_ref(t);
	obj_route_ref(r);
	AN(r->target == t);
	obj_target_weak_unref(r->target);
	r->target = NULL;
	struct obj_route **prev_ptr = &t->first_route;
	struct obj_route *prev = NULL;

	for (struct obj_route *rr = t->first_route; rr; rr = rr->t_next_route) {
		if (rr == r) {
			*prev_ptr = rr->t_next_route;
			if (t->last_route == r)
				t->last_route = prev;
			r->t_next_route = NULL;
			obj_route_unref(r);
		} else {
			prev = rr;
			prev_ptr = &rr->t_next_route;
		}
	}
	obj_route_unref(r);
	obj_target_unref(t);
}

void obj_target_print(struct obj_target *t)
{
	obj_assert_kind(t, TARGET);
	fr_printf(INFO, "obj_target\t%p\t%d\t%s\n", (void *) t,
			t->nexthop->neigh->link->vlan_id,
			t->nexthop->neigh->link->ifname);
	//print_lladdr(&t->nexthop->neigh->lladdr);
	print_af_addr(&t->nexthop->neigh->addr);
}
