// SPDX-License-Identifier: GPL-2.0-or-later

#include "common.h"

void build_af_addr(struct af_addr *af_addr, const uint8_t af, const union some_in_addr *addr, const uint8_t mask_len)
{
	AN(af_addr);
	memset(af_addr, '\0', sizeof(struct af_addr));
	af_addr->mask_len = mask_len;
	af_addr->af = af;
	if (addr != NULL) {
		switch (af) {
		case AF_INET:
			memcpy(&af_addr->in.v4, &addr->v4, sizeof(struct in_addr));
			break;
		case AF_INET6:
			memcpy(&af_addr->in.v6, &addr->v6, sizeof(struct in6_addr));
			break;
		default:
			AN(false);
		}
	}
}

void build_af_addr2(struct af_addr *af_addr, const uint8_t af, const char *addrstr, const uint8_t mask_len)
{
	union some_in_addr addr;

	AN(inet_pton(af, addrstr, &addr) == 1);
	build_af_addr(af_addr, af, &addr, mask_len);
}

void print_af_addr(const struct af_addr *af_addr)
{
	char out[INET6_ADDRSTRLEN];

	AN(af_addr);
	if (inet_ntop(af_addr->af, &af_addr->in, out, sizeof(out)))
		fr_printf(INFO, "%s/%d\n", out, af_addr->mask_len);
}
