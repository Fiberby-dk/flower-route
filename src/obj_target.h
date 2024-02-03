/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "obj.h"

struct obj_target *obj_target_find(const union some_in_addr *nh, const int oif);
struct obj_target *obj_target_netlink_find(const int oif, uint8_t af, const union some_in_addr *nh);
struct obj_target *obj_target_get_unipath(struct obj_neigh *n);
struct obj_target *obj_target_ref(struct obj_target *t);
void obj_target_unref(struct obj_target *n);
struct obj_target *obj_target_weak_ref(struct obj_target *t);
void obj_target_weak_unref(struct obj_target *n);
void obj_target_link_route(struct obj_target *t, struct obj_route *r);
void obj_target_unlink_route(struct obj_target *t, struct obj_route *r);
void obj_target_print(struct obj_target *t);
void obj_target_neigh_update(struct obj_target *t);
void obj_target_notify_routes(struct obj_target *t);
int obj_target_count(void);
