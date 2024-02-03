// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"
#include "onload.h"

struct config *config;

void config_init(const char *prog_name)
{
	config = fr_malloc(sizeof(struct config));
	config->prog_name = strdup(prog_name);

	/* default values */
	config->scan_interval = 10;
	config->flower_flags = TCA_CLS_FLAGS_SKIP_SW | TCA_CLS_FLAGS_IN_HW;
}

void config_free(void)
{
	if (config->prog_name) {
		free(config->prog_name);
		config->prog_name = NULL;
	}
	if (config->ifname) {
		free(config->ifname);
		config->ifname = NULL;
	}
	onload_free();
	free(config);
	config = NULL;
}
