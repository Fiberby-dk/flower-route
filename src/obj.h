/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef FLOWER_ROUTE_OBJ_H
#define FLOWER_ROUTE_OBJ_H

#include "rbtree.h"
#include "common.h"

enum obj_operating_mode {
	OBJ_MODE_NORMAL,
	OBJ_MODE_TEARDOWN,
};

enum obj_state {
	OBJ_STATE_PRESENT,   /* not in kernel */
	OBJ_STATE_INSTALLED, /* installed in kernel */
	OBJ_STATE_ZOMBIE,    /* going to get reaped */
};

/* canaries */
enum obj_kind {
	OBJ_KIND_UNKNOWN,
	OBJ_KIND_LINK   = 0x01234567,
	OBJ_KIND_NEIGH  = 0x12345678,
	OBJ_KIND_ROUTE  = 0x23456789,
	OBJ_KIND_TARGET = 0x3456789a,
	OBJ_KIND_RULE   = 0x456789ab,
};

/* canary macros */
#define obj_assert_kind(someobj, somekind) \
	AN((someobj)->obj.kind == OBJ_KIND_ ## somekind)
#define obj_set_kind(someobj, somekind) do { \
	obj_assert_kind(someobj, UNKNOWN); \
	(someobj)->obj.kind = OBJ_KIND_ ## somekind; \
} while (0)

struct obj_core {
	enum obj_kind kind;
	unsigned int refcnt;
	unsigned int weak_refcnt;
	enum obj_state state;
};

struct obj_link {
	struct obj_core obj;
	struct rb_node node;
	int ifindex;
	int lower_ifindex;
	uint16_t vlan_id;
	uint32_t mtu;
	uint8_t lladdr[ETH_ALEN];
	char *ifname;
	struct rb_root fdb;
};

struct obj_target;

struct obj_neigh {
	struct obj_core obj;
	struct obj_link *link;
	struct rb_node node;
	struct af_addr addr;
	uint8_t lladdr[ETH_ALEN];
	struct obj_target *targets; /* TODO */
};

struct obj_nexthop {
	struct obj_neigh *neigh;
	struct obj_nexthop *next;
};

struct obj_rule;

struct obj_target {
	struct obj_core obj;
	unsigned int nexthop_cnt;
	struct obj_nexthop *nexthop;
	struct obj_route *first_route;
	struct obj_route *last_route;
	struct obj_target *n_next_target; /* used in obj_neigh->targets linked list */
	struct obj_rule *rule;
};

struct obj_route {
	struct obj_core obj;
	struct af_addr dst;
	struct rb_node node;
	struct obj_target *target;
	struct obj_route *t_next_route;
	struct obj_rule *target_rule;
	struct obj_rule *rule;
};

enum obj_rule_state {
	OBJ_RULE_STATE_NEW,     /* have  = NULL, want  = NULL */
	OBJ_RULE_STATE_ALIEN,   /* have != NULL, want  = NULL */
	OBJ_RULE_STATE_WANT,    /* have  = NUlL, want != NULL */
	OBJ_RULE_STATE_QUEUED,  /* queued for installation */
	OBJ_RULE_STATE_PENDING, /* pending installation */
	OBJ_RULE_STATE_OK,      /* have != NULL, want == have */
	OBJ_RULE_STATE_ZOMBIE,  /* have  = NULL, want  = NULL */
};

enum obj_rule_type {
	OBJ_RULE_TYPE_NOT_SET, /* initial value */
	OBJ_RULE_TYPE_FOUND,   /* is in lost and found tree */
	OBJ_RULE_TYPE_STATIC,  /* is static, never uninstalled */
	OBJ_RULE_TYPE_DYNAMIC, /* is dynamic, uninstalled on unref */
};

struct obj_rule {
	struct obj_core obj;
	enum obj_rule_type type;
	enum obj_rule_state state;
	uint32_t chain_no;
	uint16_t prio;
	uint8_t have_laf; /* TODO replace with OBJ_RULE_TYPE_FOUND */
	uint8_t have_pos; /* TODO replace with OBJ_RULE_TYPE_STATIC / OBJ_RULE_TYPE_DYNAMIC */
	struct rb_node laf_node;
	struct rb_node pos_node;
	struct obj_target *target;
	struct obj_route *route;
	struct tc_rule *have;
	struct tc_rule *want;
};

/* when neigh's lladdr changes it needs to notify all it's targets
 * when target has been installed it needs to notify all it's routes
 * when a link is deleted it needs to notify all it's neigbours
 * when a neighbour changes it needs to notify all it's targets
 *
 * when a target is ready, request target installation
 * when a target is installed, notify routes
 * when a route's target is ready, request route installation
 */

void obj_set_mode(enum obj_operating_mode new_mode);
unsigned int obj_is_reapable(struct obj_core *c);
unsigned int obj_get_operating_mode(void);

static inline void obj_ref(struct obj_core *c)
{
	if (c->state == OBJ_STATE_ZOMBIE && c->weak_refcnt == 0)
		return;
	c->refcnt++;
}

static inline unsigned int obj_unref(struct obj_core *c)
{
	if (c->state == OBJ_STATE_ZOMBIE && c->refcnt == 0)
		return 0;
	AN(c->refcnt > 0);
	return --c->refcnt;
}

static inline void obj_weak_ref(struct obj_core *c)
{
	if (c->state == OBJ_STATE_ZOMBIE && c->weak_refcnt == 0)
		return;
	c->weak_refcnt++;
}

static inline unsigned int obj_weak_unref(struct obj_core *c)
{
	if (c->state == OBJ_STATE_ZOMBIE && c->weak_refcnt == 0)
		return 0;
	AN(c->weak_refcnt > 0);
	return --c->weak_refcnt;
}

static inline void _obj_free(struct obj_core *c)
{
	AN(c->kind != OBJ_KIND_UNKNOWN);
	AN(c->state == OBJ_STATE_ZOMBIE);
	AN(c->refcnt == 0);
	AN(c->weak_refcnt == 0);
	c->kind = OBJ_KIND_UNKNOWN;
	free(c);
}
#define obj_free(someobj) _obj_free(&((someobj)->obj))

static inline int obj_is_ok(struct obj_core *c)
{
	return c->state == OBJ_STATE_PRESENT;
}

#define obj_set_state(somekind, someobj, somestate) \
	(someobj)->obj.state = OBJ_STATE_ ## somestate

#define obj_consider_reaping(somekind, someobj) \
	do { \
		if (obj_is_reapable(&(someobj)->obj)) \
			obj_ ## somekind ## _reap(someobj); \
	} while (0)

#define obj_generic_ref(lcase, ucase) \
	struct obj_ ## lcase *obj_ ## lcase ## _ref(struct obj_ ## lcase *o) \
	{ \
		if (o == NULL) \
			return NULL; \
		obj_assert_kind(o, ucase); \
		obj_ref(&o->obj); \
		return o; \
	}

#define obj_generic_unref(lcase, ucase) \
	void obj_ ## lcase ## _unref(struct obj_ ## lcase *o) \
	{ \
		obj_assert_kind(o, ucase); \
		obj_unref(&o->obj); \
		if (obj_is_reapable(&o->obj)) \
			obj_ ## lcase ## _reap(o); \
	}

#define obj_generic_weak_ref(lcase, ucase) \
	struct obj_ ## lcase *obj_ ## lcase ## _weak_ref(struct obj_ ## lcase *o) \
	{ \
		AN(o); \
		obj_assert_kind(o, ucase); \
		obj_weak_ref(&o->obj); \
		return o; \
	}

#define obj_generic_weak_unref(lcase, ucase) \
	void obj_ ## lcase  ## _weak_unref(struct obj_ ## lcase *o) \
	{ \
		obj_assert_kind(o, ucase); \
		AN(o); \
		obj_weak_unref(&o->obj); \
	}

#endif
