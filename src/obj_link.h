/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "obj.h"

void obj_link_netlink_update(const uint16_t nlmsg_type, const int ifindex, const uint8_t (*lladdr)[ETH_ALEN], const int lower_ifindex, const uint16_t vlan_id, const uint32_t mtu, const char *ifname);
void obj_link_print(struct obj_link *l);
struct obj_link *obj_link_ref(struct obj_link *l);
void obj_link_unref(struct obj_link *l);
struct obj_link *obj_link_weak_ref(struct obj_link *l);
void obj_link_weak_unref(struct obj_link *l);
struct obj_link *obj_link_lookup(const int ifindex);
int obj_link_count(void);
