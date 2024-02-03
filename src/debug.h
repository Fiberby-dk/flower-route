/* SPDX-License-Identifier: GPL-2.0-or-later */

/* only included from common.h */

enum fr_debug_verbosity_level {
	VERBOSITY_LEVEL_ERROR,
	VERBOSITY_LEVEL_INFO,
	VERBOSITY_LEVEL_DEBUG1,
	VERBOSITY_LEVEL_DEBUG2,
	VERBOSITY_LEVEL_DEBUG3,
};

#define DBG_LEVEL(prio) \
	(config->verbosity >= VERBOSITY_LEVEL_##prio)

#define fr_printf(prio, ...) \
	do { \
		if (DBG_LEVEL(prio)) { \
			if (DBG_LEVEL(DEBUG3)) \
				fprintf(stderr, "%s:%d:%s(): ", __FILE__, __LINE__, __func__); \
			fprintf(stderr, __VA_ARGS__); \
		} \
	} while (0)
