/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "nl_common.h"

struct conn *nl_conn_open(int groups, struct conn *reuse_conn, const char *name);
void nl_conn_close(EV_P_ struct conn *c);
const char *nl_conn_get_name(struct conn *c);
