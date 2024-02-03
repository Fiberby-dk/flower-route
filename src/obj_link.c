// SPDX-License-Identifier: GPL-2.0-or-later

#include "obj_link.h"
#include "obj_neigh.h"

static struct rb_root obj_link_tree = RB_ROOT;
static int obj_link_cnt;

int obj_link_count(void)
{
	return obj_link_cnt;
}

static void obj_link_reap(struct obj_link *l)
{
	AN(l->obj.refcnt == 0);
	l->obj.state = OBJ_STATE_ZOMBIE;
	if (l->ifname != NULL)
		free(l->ifname);

	/* notify neighs */
	for (struct rb_node *n = rb_first(&l->fdb), *n_next; n; n = n_next) {
		n_next = rb_next(n);
		struct obj_neigh *neigh = rb_container_of(n, struct obj_neigh, node);

		obj_neigh_link_gone(neigh);
	}

	AN(l->obj.weak_refcnt == 0);
	rb_erase(&l->node, &obj_link_tree);
	obj_free(l);
	AN(obj_link_cnt--);
}

/* define helpers, after obj_link_reap */
obj_generic_ref(link, LINK)
obj_generic_unref(link, LINK)
obj_generic_weak_ref(link, LINK)
obj_generic_weak_unref(link, LINK)


struct obj_link *obj_link_lookup(const int ifindex)
{
	struct rb_node *node = obj_link_tree.rb_node;

	while (node) {
		struct obj_link *this = rb_container_of(node, struct obj_link, node);
		int ret = ifindex - this->ifindex;

		if (ret < 0)
			node = node->rb_left;
		else if (ret > 0)
			node = node->rb_right;
		else
			return this;
	}
	return NULL;
}

static int obj_link_insert(struct obj_link *l)
{
	obj_assert_kind(l, LINK);
	struct rb_node **new = &(obj_link_tree.rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		struct obj_link *this = rb_container_of(*new, struct obj_link, node);
		int ret = l->ifindex - this->ifindex;

		parent = *new;
		if (ret < 0)
			new = &((*new)->rb_left);
		else if (ret > 0)
			new = &((*new)->rb_right);
		else
			return 0;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&l->node, parent, new);
	rb_insert_color(&l->node, &obj_link_tree);

	return 1;
}

static void obj_link_new(struct obj_link *l)
{
	obj_set_state(link, l, INSTALLED);
	obj_link_insert(l);
	obj_link_print(l);
}

static void obj_link_update(struct obj_link *l)
{
	for (struct rb_node *n = rb_first(&l->fdb); n; n = rb_next(n)) {
		struct obj_neigh *neigh = rb_container_of(n, struct obj_neigh, node);

		AN(neigh->link == l);
		obj_neigh_link_update(neigh);
	}
}

static void obj_link_delete(struct obj_link *l)
{
	obj_set_state(link, l, PRESENT);
	obj_consider_reaping(link, l);
}

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

static int ifname_set(char **dst, const char *src)
{
	if (src == NULL) {
		if (*dst == NULL)
			return 0;
		free(*dst);
		*dst = NULL;
		return 1;
	}

	if (*dst != NULL) {
		if (strcmp(*dst, src) == 0)
			return 0;
		free(*dst);
	}

	*dst = strdup(src);
	AN(*dst);
	return 1;
}

void obj_link_netlink_update(const uint16_t nlmsg_type, const int ifindex, const uint8_t (*lladdr)[ETH_ALEN], const int lower_ifindex, const uint16_t vlan_id, const uint32_t mtu, const char *ifname)
{
	struct obj_link *l = obj_link_lookup(ifindex);

	if (l)
		obj_assert_kind(l, LINK);
	int is_new = l == NULL;

	if (nlmsg_type == RTM_DELLINK) {
		if (l)
			obj_link_delete(l);
		return;
	}
	if (is_new) {
		l = fr_malloc(sizeof(struct obj_link));
		obj_set_kind(l, LINK);
		obj_link_cnt++;
		l->ifindex = ifindex;
	} else {
		AN(l->ifindex == ifindex);
	}

	int changes = 0;

	changes += lladdr_set(&l->lladdr, lladdr);
	changes += ifname_set(&l->ifname, ifname);

	if (l->lower_ifindex != lower_ifindex) {
		l->lower_ifindex = lower_ifindex;
		changes++;
	}

	if (l->mtu != mtu) {
		l->mtu = mtu;
		changes++;
	}

	if (l->vlan_id != vlan_id) {
		l->vlan_id = vlan_id;
		changes++;
	}

	if (is_new)
		obj_link_new(l);
	else if (changes > 0)
		obj_link_update(l);
	else
		fr_printf(ERROR, "link: no changes\n");
}

void obj_link_print(struct obj_link *l)
{
	obj_assert_kind(l, LINK);
	fr_printf(INFO, "link: %d", l->ifindex);
	fr_printf(INFO, " %s", l->ifname);
	if (l->lower_ifindex != 0)
		fr_printf(INFO, ", lower: %d", l->lower_ifindex);
	fr_printf(INFO, ", lladdr: %02x:%02x:%02x:%02x:%02x:%02x",
		  l->lladdr[0], l->lladdr[1], l->lladdr[2],
		  l->lladdr[3], l->lladdr[4], l->lladdr[5]);
	fr_printf(INFO, ", mtu: %d", l->mtu);
	fr_printf(INFO, ", vlan: %d", l->vlan_id);
	fr_printf(INFO, "\n");
}
