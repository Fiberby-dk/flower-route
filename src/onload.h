/* SPDX-License-Identifier: GPL-2.0-or-later */

struct config_prefix_list *onload_lookup_prefix_list(const char *list_name);
int onload_load_prefixes(const char *list_name, const char *file);
int onload_add_prefix(const char *list_name, const char *addr);
void onload_print(void);
void onload_free(void);
