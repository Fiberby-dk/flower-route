/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef FLOWER_ROUTE_OBJ_ROUTE_H
#define FLOWER_ROUTE_OBJ_ROUTE_H

#include "obj.h"

void obj_route_netlink_update(const uint16_t nlmsg_type, struct obj_target *t, const struct af_addr *af_dst);
void obj_route_install(struct obj_route *r);
int obj_route_count(void);
struct obj_route *obj_route_ref(struct obj_route *r);
void obj_route_unref(struct obj_route *r);

#endif
