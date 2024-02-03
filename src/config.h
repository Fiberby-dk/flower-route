/* SPDX-License-Identifier: GPL-2.0-or-later */

/* only included from common.h */

struct config_prefix_list_entry {
	struct config_prefix_list_entry *next;
	struct af_addr addr;
};

struct config_prefix_list {
	char *name;
	struct config_prefix_list_entry *head;
	struct config_prefix_list_entry *tail;
	struct config_prefix_list *next;
};

struct config {
	uint32_t table_id;
	unsigned int ifidx;
	unsigned int scan_interval;
	unsigned int timeout;
	char *ifname;
	char *prog_name;
	struct config_prefix_list *prefix_list_head;
	uint8_t dry_run;
	uint32_t flower_flags;
	uint8_t verbosity;
	int exit_after_first_sync;
};

extern struct config *config;

void config_init(const char *prog_name);
void config_free(void);
