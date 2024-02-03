/* SPDX-License-Identifier: GPL-2.0-or-later */

void hexdumpf(FILE *out, const void *data, size_t len);
#define hexdump(data, len) hexdumpf(stdout, data, len)
