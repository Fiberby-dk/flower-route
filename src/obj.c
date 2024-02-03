// SPDX-License-Identifier: GPL-2.0-or-later

#include "obj.h"

static enum obj_operating_mode obj_current_mode = OBJ_MODE_NORMAL;

void obj_set_mode(enum obj_operating_mode new_mode)
{
	obj_current_mode = new_mode;
}

unsigned int obj_is_reapable(struct obj_core *c)
{
	switch (obj_current_mode) {
	case OBJ_MODE_NORMAL:
		return c->refcnt == 0 && c->state != OBJ_STATE_ZOMBIE && c->state != OBJ_STATE_INSTALLED;
	case OBJ_MODE_TEARDOWN:
		return c->refcnt == 0 && c->state != OBJ_STATE_ZOMBIE;
	default:
		AN(false);
	}
}

unsigned int obj_get_operating_mode(void)
{
	return obj_current_mode;
}
