/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef FLOWER_ROUTE_TESTS_COMMON_H
#define FLOWER_ROUTE_TESTS_COMMON_H
#include <check.h>

#include "../src/common.h"

void assert_all_counts_are_zero(void);
void pre_test(void);
void post_test(void);

#endif
