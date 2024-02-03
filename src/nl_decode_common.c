// SPDX-License-Identifier: GPL-2.0-or-later

#include "nl_decode_common.h"

int decode_nlattr(const struct nlattr *attr,
	const struct nlattr **tb,
	uint16_t max,
	const struct type_map *typemap)
{
	int type = mnl_attr_get_type(attr);

	/* skip unsupported attribute in user-space */
	if (mnl_attr_type_valid(attr, max) < 0)
		return MNL_CB_OK;

	const struct type_map *map = &typemap[type];
	int datatype = map->type;

	if (map->type > MNL_TYPE_UNSPEC) {
		if (mnl_attr_validate(attr, datatype) < 0) {
			fr_printf(ERROR, "mnl_attr_validate: %s: %s\n",
				map->attr,
				strerror(errno));
			return MNL_CB_ERROR;
		}
		tb[type] = attr;
	} else {
		//fr_printf(DEBUG2, "unknown attr: %d\n", type);
	}

	return MNL_CB_OK;
}
