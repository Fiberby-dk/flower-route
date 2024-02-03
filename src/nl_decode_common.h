/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "nl_common.h"

struct type_map {
	const enum mnl_attr_data_type type;
	const char *attr;
};

#define TYPE_MAP(ATTR, TYPE) \
	[ATTR] = { \
		.type = MNL_TYPE_ ## TYPE, \
		.attr = #ATTR, \
	}

#define decode_nlattr_cb(type_name, max, print_undef) \
	static int decode_nlattr_ ## type_name ## _cb(const struct nlattr *attr, void *data) \
	{ \
		int type = mnl_attr_get_type(attr); \
		if (type < max) { \
			uint16_t attr_len = mnl_attr_get_payload_len(attr); \
			if (type_name ## _attr_types[type].type == MNL_TYPE_UNSPEC && print_undef) \
				fr_printf(DEBUG2, "undefined %s attr type: %d, len: %d\n", \
					#type_name, type, attr_len); \
		} \
		return decode_nlattr(attr, data, max, type_name ## _attr_types); \
	}

int decode_nlattr(const struct nlattr *attr,
	const struct nlattr **tb,
	uint16_t max,
	const struct type_map *typemap);
