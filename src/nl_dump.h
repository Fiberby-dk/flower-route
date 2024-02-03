/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "common.h"

void nl_dump_link(EV_P_ struct conn *c);
void nl_dump_neigh(EV_P_ struct conn *c, uint8_t af);
void nl_dump_route(EV_P_ struct conn *c, uint8_t af);
