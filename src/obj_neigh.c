// SPDX-License-Identifier: GPL-2.0-or-later

#include "obj_neigh.h"
#include "obj_link.h"
#include "obj_target.h"

static int obj_neigh_cnt;

int obj_neigh_count(void)
{
	return obj_neigh_cnt;
}

static void obj_neigh_unlink(struct obj_neigh *n)
{
	struct obj_link *l = n->link;

	rb_erase(&n->node, &l->fdb);
	obj_link_weak_unref(l);
	n->link = NULL;
}

static void obj_neigh_reap(struct obj_neigh *n)
{
	struct obj_link *l;

	obj_assert_kind(n, NEIGH);
	AN(n->obj.refcnt == 0);
	n->obj.state = OBJ_STATE_ZOMBIE;
	for (struct obj_target *t = n->targets, *nt; t; t = nt) {
		nt = t->n_next_target;
		t->n_next_target = NULL;
		obj_target_unref(t);
		// TODO remove neigh reference from target completely
	}

	l = n->link;
	if (l)
		obj_neigh_unlink(n);

	obj_free(n);
	AN(obj_neigh_cnt--);
}

/* define helpers, after obj_target_reap */
obj_generic_ref(neigh, NEIGH)
obj_generic_unref(neigh, NEIGH)
obj_generic_weak_ref(neigh, NEIGH)
obj_generic_weak_unref(neigh, NEIGH)

static int lladdr_set(uint8_t (*dst)[ETH_ALEN], const uint8_t (*src)[ETH_ALEN])
{
	int changes = 0;

	if (src == NULL) {
		for (int i = 0; i < ETH_ALEN; i++) {
			if ((*dst)[i] != 0) {
				(*dst)[i] = 0;
				changes++;
			}
		}
	} else if (memcmp(dst, src, ETH_ALEN) != 0) {
		memcpy(dst, src, ETH_ALEN);
		changes++;
	}
	return changes;
}

struct obj_neigh *obj_neigh_fdb_lookup(const struct obj_link *l, const struct af_addr *addr)
{
	struct rb_node *node;
	struct obj_neigh *this;
	int ret;

	obj_assert_kind(l, LINK);
	node = l->fdb.rb_node;

	while (node) {
		this = rb_container_of(node, struct obj_neigh, node);
		ret = memcmp(addr, &this->addr, sizeof(struct af_addr));
		if (ret < 0)
			node = node->rb_left;
		else if (ret > 0)
			node = node->rb_right;
		else
			return this;
	}
	return NULL;
}

struct obj_neigh *obj_neigh_fdb_lookup2(const struct obj_link *l, const uint8_t af, const union some_in_addr *addr)
{
	struct af_addr af_addr;

	obj_assert_kind(l, LINK);
	build_af_addr(&af_addr, af, addr, 0);
	return obj_neigh_fdb_lookup(l, &af_addr);
}


static int obj_neigh_fdb_insert(struct obj_neigh *n)
{
	obj_assert_kind(n, NEIGH);
	struct obj_link *l = n->link;
	struct rb_node **new = &(l->fdb.rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct obj_neigh *this = rb_container_of(*new, struct obj_neigh, node);
		int ret = memcmp(&n->addr, &this->addr, sizeof(struct af_addr));

		parent = *new;
		if (ret < 0)
			new = &((*new)->rb_left);
		else if (ret > 0)
			new = &((*new)->rb_right);
		else
			return 0;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&n->node, parent, new);
	rb_insert_color(&n->node, &l->fdb);

	return 1;
}

void obj_neigh_print(const struct obj_neigh *n)
{
	obj_assert_kind(n, NEIGH);
	struct obj_link *l = n->link;
	char out[INET6_ADDRSTRLEN];

	fr_printf(INFO, "neigh: %d %s (vlan id: %d)", l->ifindex, l->ifname, l->vlan_id);
	if (n->addr.af > 0) {
		if (inet_ntop(n->addr.af, &n->addr.in, out, sizeof(out)))
			fr_printf(INFO, " %s", out);
	}
	fr_printf(INFO, ", lladdr: %02x:%02x:%02x:%02x:%02x:%02x",
			n->lladdr[0], n->lladdr[1], n->lladdr[2],
			n->lladdr[3], n->lladdr[4], n->lladdr[5]);
	fr_printf(INFO, "\n");
}

static void obj_neigh_notify_targets(struct obj_neigh *n)
{
	for (struct obj_target *t = n->targets; t; t = t->n_next_target)
		obj_target_neigh_update(t);
}

void obj_neigh_link_update(struct obj_neigh *n)
{
	obj_assert_kind(n, NEIGH);
	obj_neigh_notify_targets(n);
}

static void obj_neigh_new(struct obj_neigh *n)
{
	obj_neigh_ref(n);
	obj_set_state(neigh, n, INSTALLED);
	obj_neigh_fdb_insert(n);
	obj_neigh_print(n);
	obj_neigh_unref(n);
}

static void obj_neigh_update(struct obj_neigh *n)
{
	obj_neigh_ref(n);
	obj_set_state(neigh, n, INSTALLED);
	obj_neigh_notify_targets(n);
	obj_neigh_unref(n);
}

static void obj_neigh_delete(struct obj_neigh *n)
{
	obj_set_state(neigh, n, PRESENT);
	obj_consider_reaping(neigh, n);
}

void obj_neigh_link_gone(struct obj_neigh *n)
{
	obj_assert_kind(n, NEIGH);
	AN(n->link);
	obj_neigh_unlink(n);
	obj_neigh_delete(n);
}

static struct obj_neigh *obj_neigh_alloc(void)
{
	struct obj_neigh *n;

	n = fr_malloc(sizeof(struct obj_neigh));
	obj_set_kind(n, NEIGH);
	obj_neigh_cnt++;
	return n;
}

void obj_neigh_netlink_update(const uint16_t nlmsg_type, const int ifindex, const uint8_t af, const union some_in_addr *addr, const uint8_t (*lladdr)[ETH_ALEN])
{
	struct af_addr af_addr;
	struct obj_neigh *n;
	struct obj_link *l;
	int changes = 0;
	int is_new;

	l = obj_link_lookup(ifindex);
	if (l == NULL)
		return;

	build_af_addr(&af_addr, af, addr, 0);
	n = obj_neigh_fdb_lookup(l, &af_addr);

	if (nlmsg_type == RTM_DELNEIGH) {
		if (n)
			obj_neigh_delete(n);
		return;
	}

	is_new = n == NULL;
	if (is_new) {
		n = obj_neigh_alloc();
		memcpy(&n->addr, &af_addr, sizeof(struct af_addr));
		n->link = obj_link_weak_ref(l);
	}

	changes += lladdr_set(&n->lladdr, lladdr);

	if (is_new)
		obj_neigh_new(n);
	else if (changes > 0)
		obj_neigh_update(n);
}

struct obj_neigh *obj_neigh_netlink_get(const int ifindex, uint8_t af, const union some_in_addr *addr)
{
	struct af_addr af_addr;
	struct obj_link *l;
	struct obj_neigh *n;
	int is_new;

	l = obj_link_lookup(ifindex);
	if (l == NULL)
		return NULL;

	build_af_addr(&af_addr, af, addr, 0);
	n = obj_neigh_fdb_lookup(l, &af_addr);

	is_new = n == NULL;
	if (is_new) {
		n = obj_neigh_alloc();
		memcpy(&n->addr, &af_addr, sizeof(struct af_addr));
		n->link = obj_link_ref(l);
	}

	return n;
}
