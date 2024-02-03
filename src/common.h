/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef FLOWER_ROUTE_COMMON_H
#define FLOWER_ROUTE_COMMON_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <inttypes.h>
#include <ev.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <linux/rtnetlink.h>
#include <linux/pkt_cls.h>
#include <linux/if_arp.h>

#define fr_unused(var) ((void)var)

#if EV_MULTIPLICITY
#define fr_ev_unused() fr_unused(EV_A)
#else
#define fr_ev_unused()
#endif

enum boolean {
	false = 0,
	true  = 1
};

union some_in_addr {
	struct in_addr v4;
	struct in6_addr v6;
};

struct af_addr {
	uint8_t af;
	uint8_t mask_len;
	union some_in_addr in;
};

void build_af_addr(struct af_addr *af_addr, const uint8_t af, const union some_in_addr *addr, const uint8_t mask_len);
void build_af_addr2(struct af_addr *af_addr, const uint8_t af, const char *addrstr, const uint8_t mask_len);
void print_af_addr(const struct af_addr *af_addr);

#include "config.h"
#include "debug.h"

static inline void *
fr_malloc(size_t size)
{
	void *ptr = malloc(size);

	if (ptr == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}
	memset(ptr, '\0', size);
	return ptr;
}

/* below inspired by PHK in Varnish Cache */
#ifdef WITHOUT_ASSERTS
#define assert(e)       ((void)(e))
#else /* WITH_ASSERTS */
#define assert(e) \
do { \
	if (!(e)) { \
		fr_printf(ERROR, "ASSERT_FAILED %s\t%s\t%d\t%s\n", \
			__func__, __FILE__, __LINE__, #e); \
		abort(); \
	} \
} while (0)
#endif

/* Assert zero */
#define AZ(foo)    assert((foo) == 0)

/* Assert non-zero */
#define AN(foo)    assert((foo) != 0)

#endif
