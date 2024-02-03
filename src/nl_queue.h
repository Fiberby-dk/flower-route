/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "nl_common.h"

void queue_schedule(EV_P_ void (*execute)(EV_P_ void *data), void (*completed)(EV_P_ void *data, int nl_errno), void *data); /* XXX make nl_errno const */
void queue_init(struct conn *c);
void queue_fini(void);
struct conn *queue_get_conn(void);
void nl_queue_status(void);
