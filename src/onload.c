// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <limits.h>

#include "onload.h"

#define LINE_MAX_LEN 512

static struct config_prefix_list *lookup_prefix_list(const char *list_name, struct config_prefix_list ***tail)
{
	struct config_prefix_list *list, **next;

	next = &config->prefix_list_head;
	while (*next) {
		list = *next;
		if (strcmp(list->name, list_name) == 0)
			return list;
		next = &list->next;
	}

	if (tail)
		*tail = next;

	return NULL;
}

static struct config_prefix_list *get_or_create_prefix_list(const char *list_name)
{
	struct config_prefix_list *list, **tail;

	list = lookup_prefix_list(list_name, &tail);
	if (list)
		return list;

	/* allocate new list */
	list = fr_malloc(sizeof(struct config_prefix_list));
	list->name = strdup(list_name);
	*tail = list;
	return list;
}

struct config_prefix_list *onload_lookup_prefix_list(const char *list_name)
{
	return lookup_prefix_list(list_name, NULL);
}

static void onload_store_prefix(const char *list_name, const struct config_prefix_list_entry *pfx)
{
	struct config_prefix_list *list;
	struct config_prefix_list_entry *entry;

	list = get_or_create_prefix_list(list_name);
	AN(list);

	entry = fr_malloc(sizeof(struct config_prefix_list_entry));
	memcpy(entry, pfx, sizeof(struct config_prefix_list_entry));

	/* store prefix */
	if (list->tail != NULL)
		list->tail->next = entry;
	else
		list->head = entry;
	list->tail = entry;
}

void onload_print(void)
{
	fr_printf(DEBUG2, "onloaded prefixes:\n");
	// TODO actually loop over prefixes and print 'em
}

int onload_add_prefix(const char *list_name, const char *orig_addr)
{
	struct config_prefix_list_entry pfx;
	char *endptr;
	char *addr;
	int val;

	addr = strdup(orig_addr);
	AN(addr);
	memset(&pfx, '\0', sizeof(struct config_prefix_list_entry));

	// TODO check if prefix is already onloaded

	/* handle network part */
	char *ipstr = addr;
	char *slash = strchr(addr, '/');
	char *maskstr = NULL;
	int af_max_mask;
	int af;

	if (slash != NULL) {
		*slash = '\0';
		maskstr = slash + 1;
	}
	af = strchr(addr, ':') == NULL ? AF_INET : AF_INET6;
	if (inet_pton(af, ipstr, &pfx.addr) == 0)
		goto error_out;

	/* handle mask part */
	af_max_mask = af == AF_INET ? 32 : 128;
	if (maskstr == NULL) {
		pfx.addr.mask_len = af_max_mask;
	} else {
		val = strtol(maskstr, &endptr, 10);
		if (endptr[0] != '\0')
			goto error_out;
		if (val < 0 || val > af_max_mask)
			goto error_out;
		pfx.addr.mask_len = val;
	}

	pfx.addr.af = af;

	onload_store_prefix(list_name, &pfx);

	free(addr);
	return 0;
error_out:
	free(addr);
	return -EINVAL;
}

static int onload_fread_prefix(const char *list_name, FILE *fp)
{
	char buf[LINE_MAX_LEN];

	while (fgets(buf, sizeof(buf), fp)) {
		char *p_was_here;
		char end_orig;
		char *p = buf;
		char *begin;
		char *end;
		int ret;

		while (*p == ' ' || *p == '\t')
			p++;

		if (*p == '#' || *p == '\n' || *p == 0)
			continue;

		begin = p;

		do {
			p_was_here = p;
			while (*p >= '0' && *p <= '9')
				p++;
			while (*p >= 'a' && *p <= 'f')
				p++;
			while (*p >= 'A' && *p <= 'F')
				p++;
			while (*p == '.' || *p == ':' || *p == '/')
				p++;
		} while (p > p_was_here);

		end = p;

		while (*p == ' ' || *p == '\t')
			p++;

		if (!(*p == '#' || *p == '\n' || *p == 0))
			continue;

		if (*p == '\n')
			*p = '\0';

		end_orig = *end;
		*end = '\0';
		ret = onload_add_prefix(list_name, begin);
		if (ret != 0) {
			if (ret == -EINVAL) {
				*end = end_orig;
				fr_printf(ERROR, "unable to parse line: %s\n", buf);
			}
			return ret;
		}
	}
	return 0;
}

int onload_load_prefixes(const char *list_name, const char *file)
{
	FILE *fp;
	int ret = 0;

	fp = fopen(file, "r");
	if (!fp)
		return -errno;

	while ((ret = onload_fread_prefix(list_name, fp))) {
		if (ret != 0)
			break;
	}
	fclose(fp);
	return ret;
}

void onload_free(void)
{
	AN(config);
	for (struct config_prefix_list *list = config->prefix_list_head, *next_list; list; list = next_list) {
		next_list = list->next;
		for (struct config_prefix_list_entry *pfx = list->head, *next_pfx; pfx; pfx = next_pfx) {
			next_pfx = pfx->next;
			free(pfx);
		}
		free(list->name);
		free(list);
	}
}
