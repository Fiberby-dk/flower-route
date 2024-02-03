/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "obj.h"

void obj_neigh_netlink_update(const uint16_t nlmsg_type, const int ifindex, const uint8_t af, const union some_in_addr *addr, const uint8_t (*lladdr)[ETH_ALEN]);
struct obj_neigh *obj_neigh_netlink_get(const int ifindex, uint8_t af, const union some_in_addr *addr);
void obj_neigh_link_update(struct obj_neigh *n);
void obj_neigh_print(const struct obj_neigh *n);

struct obj_neigh *obj_neigh_ref(struct obj_neigh *n);
void obj_neigh_unref(struct obj_neigh *n);
struct obj_neigh *obj_neigh_weak_ref(struct obj_neigh *n);
void obj_neigh_weak_unref(struct obj_neigh *n);
void obj_neigh_link_gone(struct obj_neigh *n);
int obj_neigh_count(void);

struct obj_neigh *obj_neigh_fdb_lookup(const struct obj_link *l, const struct af_addr *addr);
struct obj_neigh *obj_neigh_fdb_lookup2(const struct obj_link *l, const uint8_t af, const union some_in_addr *addr);
