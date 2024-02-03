// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "hexdump.h"

#define bytes_per_row 16
#define bytes_per_group 2
#define byte_offset ((2 * bytes_per_row) + (bytes_per_row / bytes_per_group) + 1)
#define buf_len (byte_offset + bytes_per_row + 1)
#define hex_pos(j) ((j * 2) + (j / bytes_per_group))

static char hexdigit(int n)
{
	return n < 10 ? '0'+n : 'a'+(n-10);
}

void hexdumpf(FILE *out, const void *data, size_t len)
{
	uint8_t buf[buf_len];
	int i, j, rows;
	size_t row_offset, pos;
	const uint8_t *bytes = data;

	fprintf(out, "hexdumping %zu byte%s\n", len, (len != 1 ? "s" : ""));
	rows = len / bytes_per_row + (len % bytes_per_row > 0);
	for (i = 0, row_offset = 0; i < rows; i++, row_offset += bytes_per_row) {
		memset(&buf, ' ', buf_len-1);
		buf[buf_len-1] = '\0';
		for (j = 0, pos = row_offset; j < bytes_per_row && pos < len; j++, pos++) {
			buf[hex_pos(j)]   = hexdigit(bytes[pos]/16);
			buf[hex_pos(j)+1] = hexdigit(bytes[pos]%16);
			if (bytes[pos] >= 0x20 && bytes[pos] < 0x7f)
				buf[byte_offset+j] = bytes[pos];
			else
				buf[byte_offset+j] = '.';
		}
		fprintf(out, "%08zx: %s\n", row_offset, buf);
	}
}
