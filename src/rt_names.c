// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"
#include <linux/rtnetlink.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include "rt_names.h"

#define CONF_ETC_DIR "/etc/iproute2"
#define CONF_USR_DIR "/usr/lin/iproute2"

#define NAME_MAX_LEN 512

/* This is a simplified version of iproute2's lib/rt_names */

struct rt_names_entry {
	struct rt_names_entry *next;
	uint32_t id;
	char *name;
	int is_static;
};

static struct rt_names_entry local_table_entry = {
	.id = RT_TABLE_LOCAL,
	.name = "local",
	.is_static = true,
};
static struct rt_names_entry main_table_entry  = {
	.id = RT_TABLE_MAIN,
	.name = "main",
	.next = &local_table_entry,
	.is_static = true,
};
static struct rt_names_entry dflt_table_entry  = {
	.id = RT_TABLE_DEFAULT,
	.name = "default",
	.next = &main_table_entry,
	.is_static = true,
};

static struct rt_names_entry *rt_names_root = &dflt_table_entry;
static struct rt_names_entry *rt_names_tail = &local_table_entry;

static void rt_names_add_entry(uint32_t id, const char *name)
{
	struct rt_names_entry *entry = fr_malloc(sizeof(struct rt_names_entry));

	entry->id = id;
	entry->name = strdup(name);
	AN(entry->name);
	rt_names_tail->next = entry;
	rt_names_tail = entry;
}

unsigned int rt_names_lookup(const char *name)
{
	for (struct rt_names_entry *entry = rt_names_root; entry; entry = entry->next)
		if (strcmp(entry->name, name) == 0)
			return entry->id;
	return 0;
}

/* borrowed from iproute2 */
static int rt_names_fread_id_name(FILE *fp, uint32_t *id, char *namebuf)
{
	char buf[NAME_MAX_LEN];
	int id2 = 0;

	while (fgets(buf, sizeof(buf), fp)) {
		char *p = buf;

		while (*p == ' ' || *p == '\t')
			p++;

		if (*p == '#' || *p == '\n' || *p == 0)
			continue;

		if (sscanf(p, "0x%x %s\n", id, namebuf) != 2 &&
				sscanf(p, "0x%x %s #", id, namebuf) != 2 &&
				sscanf(p, "%d %s\n", &id2, namebuf) != 2 &&
				sscanf(p, "%d %s #", &id2, namebuf) != 2) {
			stpncpy(namebuf, p, NAME_MAX_LEN);
			return -1;
		}
		if (id2 > 0)
			*id = id2;
		return 1;
	}
	return 0;
}

static int rt_names_read_file(const char *file)
{
	FILE *fp;
	unsigned int id;
	char namebuf[NAME_MAX_LEN] = {0};
	int ret;

	fp = fopen(file, "r");
	if (!fp)
		return -errno;

	while ((ret = rt_names_fread_id_name(fp, &id, &namebuf[0]))) {
		if (ret == -1) {
			fprintf(stderr, "rt_tables: Database %s is corrupted at %s\n",
					file, namebuf);
			fclose(fp);
			return -EINVAL;
		}

		/* do something with id and namebuf */
		rt_names_add_entry(id, namebuf);
	}
	fclose(fp);

	return 0;
}

static void rt_names_read_dir(const char *dirpath_base, const char *dirpath_overload)
{
	struct dirent *de;
	DIR *d;

	d = opendir(dirpath_base);
	if (d == NULL)
		return;
	while ((de = readdir(d)) != NULL) {
		char path[PATH_MAX];
		size_t len;
		struct stat sb;

		if (*de->d_name == '.')
			continue;

		/* only consider filenames ending in '.conf' */
		len = strlen(de->d_name);
		if (len <= 5)
			continue;
		if (strcmp(de->d_name + len - 5, ".conf"))
			continue;

		if (dirpath_overload) {
			/* only consider filenames not present in the overloading directory, e.g. /etc */
			snprintf(path, sizeof(path), "%s/%s", dirpath_overload, de->d_name);
			if (lstat(path, &sb) == 0)
				continue;
		}

		/* load the conf file in the base directory, e.g., /usr */
		snprintf(path, sizeof(path), "%s/%s", dirpath_base, de->d_name);
		rt_names_read_file(path);
	}
	closedir(d);
}

static void rt_names_load_names(void)
{
	if (rt_names_read_file(CONF_ETC_DIR "/rt_tables") == ENOENT)
		rt_names_read_file(CONF_USR_DIR "/rt_tables");

	/* load /usr/lib/iproute2/rt_tables.d/X conf files, unless /etc/iproute2/rt_tables.d/X exists */
	rt_names_read_dir(CONF_USR_DIR "/rt_tables.d", CONF_ETC_DIR "/rt_tables.d");
	rt_names_read_dir(CONF_ETC_DIR "/rt_tables.d", NULL);
}

void rt_names_init(void)
{
	rt_names_load_names();
}

void rt_names_free(void)
{
	for (struct rt_names_entry *entry = rt_names_root, *next; entry; entry = next) {
		next = entry->next;
		if (!entry->is_static) {
			if (entry->name)
				free(entry->name);
			free(entry);
		}
	}
	rt_names_root = &dflt_table_entry;
	rt_names_tail = &local_table_entry;
}
