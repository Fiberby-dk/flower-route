// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <net/if.h>

#include ".version.h"
#include "rt_names.h"
#include "onload.h"
#include "options.h"

enum run_modes {
	RUN_DAEMON,
	RUN_ONCE,
	SHOW_HELP,
	SHOW_VERSION,
};

static struct option long_options[] = {
	{"iface",          required_argument, 0, 'i' },
	{"table",          required_argument, 0, 't' },
	{"add-prefix",     required_argument, 0, 'p' },
	{"load-prefix",    required_argument, 0, 'P' },
	{"scan-interval",  required_argument, 0, 's' },
	{"timeout",        required_argument, 0, 'T' },
	{"verbose",        no_argument,       0, 'v' },
	{"help",           no_argument,       0, 'h' },
	{"one-off",        no_argument,       0, '1' },
	{"dry-run",        no_argument,       0,  1  },
	{"skip-hw",        no_argument,       0,  2  },
	{"skip_hw",        no_argument,       0,  2  },
	{"version",        no_argument,       0,  3  },
	{0,                0,                 0,  0  }
};
static const char short_options[] = "i:t:p:P:s:T:vh1";

static void show_help(FILE *f)
{
	fprintf(f, "usage: %s [OPTIONS]\n\n", config->prog_name);
	fprintf(f, "flower-route syncronizes a routing table to");
	fprintf(f, "a TC-flower offload-capable NIC\n");
	fprintf(f, "\n");
	fprintf(f, "The following NIC's are currently Known to work:\n");
	fprintf(f, "- Mellanox ConnectX-4 onwards (those that have \"ASAP²\")\n");
	fprintf(f, "     (Currently only tested on Connect-X 5 and 6 Dx)\n");
	fprintf(f, "\n");
	fprintf(f, "Options:\n");
	fprintf(f, "\t-i, --iface <iface>               install offload rules on interface\n");
	fprintf(f, "\t-t, --table <table>               routing table to syncronize with\n");
	fprintf(f, "\t-p, --add-prefix <list> <prefix>  add static prefix\n");
	fprintf(f, "\t-P, --load-prefix <list> <file>   load static prefixes from file\n");
	fprintf(f, "\t-s, --scan-interval <secs>        time between netlink scans (dft: 10s)\n");
	fprintf(f, "\t-T, --timeout <secs>              run for <n> seconds, and then exit\n");
	fprintf(f, "\t-1, --one-off                     just sync once, and then exit\n");
	fprintf(f, "\t    --skip-hw                     for testing without hardware\n");
	fprintf(f, "\t    --dry-run                     don't make any changes to TC\n");
	fprintf(f, "\t-v, --verbose                     increase verbosity\n");
	fprintf(f, "\t    --version                     show version\n");
	fprintf(f, "\t-h, --help                        show this help text\n");
}

static void show_version(void)
{
	printf("%s version %s\n", config->prog_name, VERSION_GIT);
	exit(EXIT_SUCCESS);
}

static void bail(const char *fmt, ...)
{
	va_list ap;

	if (fmt != NULL) {
		fprintf(stderr, "%s: ", config->prog_name);

		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);

		fprintf(stderr, "\n");
	}

	fprintf(stderr, "\n");
	show_help(stderr);
	exit(EXIT_FAILURE);
}

static void validate_options(void)
{
	if (config->table_id == 0)
		bail("missing --table argument");

	if (config->ifidx == 0)
		bail("missing --iface argument");
}

static char *get_second_argument(const int argc, char **argv)
{
	char *ret = NULL;

	if (optind < argc && *argv[optind] != '-') {
		ret = argv[optind];
		optind++;
	} else {
		bail("-p and -P requires TWO arguments");
	}
	return ret;
}

void options_parse(const int argc, char **argv)
{
	int c;
	char *endptr;
	long val;
	enum run_modes run_mode = RUN_DAEMON;

	/* reset getopt (needed for `make test_nofork` */
	optind = 1;

	while (1) {
		int option_index = 0;
		char *optarg2;

		c = getopt_long(argc, argv, short_options,
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 't':
			if (config->table_id != 0)
				bail("table should only be specified once");
			config->table_id = rt_names_lookup(optarg);

			if (config->table_id == 0) {
				val = strtol(optarg, &endptr, 10);
				if (endptr[0] == '\0' && val >= 0 && val <= UINT_MAX)
					config->table_id = val;
			}
			if (config->table_id == 0)
				bail("unknown routing table %s", optarg);
			break;
		case 'i':
			if (config->ifname != NULL)
				bail("interface should only be specified once");
			config->ifidx = if_nametoindex(optarg);
			if (config->ifidx != 0)
				config->ifname = strdup(optarg);
			else
				bail("invalid interface: %s", optarg);
			break;
		case 'p':
			optarg2 = get_second_argument(argc, argv);
			if (onload_add_prefix(optarg, optarg2) == -EINVAL)
				bail("failed to parse prefix: %s", optarg);
			break;
		case 'P':
			optarg2 = get_second_argument(argc, argv);
			if (onload_load_prefixes(optarg, optarg2) == -EINVAL)
				bail("failed to parse prefix file: %s", optarg);
			break;
		case 'T':
			val = strtol(optarg, &endptr, 10);
			if (endptr[0] != '\0')
				bail("invalid argument: '%s'", optarg);
			if (val <= 0 || val > UINT_MAX)
				bail("timeout: out of bounds");
			config->timeout = val;
			break;
		case 's':
			val = strtol(optarg, &endptr, 10);
			if (endptr[0] != '\0')
				bail("invalid argument: '%s'", optarg);
			if (val <= 0 || val > UINT_MAX)
				bail("scan-interval: out of bounds");
			config->scan_interval = val;
			break;
		case '1':
			run_mode = RUN_ONCE;
			break;
		case 'v':
			config->verbosity++;
			break;
		case 'h':
			run_mode = SHOW_HELP;
			break;
		case 1: /* dry-run */
			config->dry_run = true; // XXX make sure dry_run is followed
			break;
		case 2: /* skip hw */
			config->flower_flags = TCA_CLS_FLAGS_SKIP_HW;
			break;
		case 3: /* version */
			run_mode = SHOW_VERSION;
			break;
		default:
			bail(NULL);
		}
	}

	if (optind < argc)
		bail("Too many arguments");

	switch (run_mode) {
	case SHOW_HELP:
		show_help(stdout);
		exit(EXIT_SUCCESS);
	case SHOW_VERSION:
		show_version();
		exit(EXIT_SUCCESS);
	case RUN_ONCE:
		config->exit_after_first_sync = true;
		/* fall-through */
	case RUN_DAEMON:
		if (config->verbosity > 1)
			onload_print();
		validate_options();
		break;
	}
}
