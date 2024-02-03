# SPDX-License-Identifier: GPL-2.0-or-later
.PHONY: build clean clean-ish test test_nofork test_gdb valgrind scan-build lcov
.DEFAULT_GOAL=build
CC ?= clang
TARGET=flower-routed
TEST_TARGET=.objs/test
TARGETS=$(TARGET) $(TEST_TARGET)

MODS=common config options rt_names onload
MODS+=nl_common nl_conn nl_dump nl_decode nl_send rt_explain nl_filter
MODS+=tc_explain tc_decode nl_decode_common nl_queue tc_rule tc_encode
MODS+=obj obj_link obj_neigh obj_route obj_target obj_rule
MODS+=scan monitor rbtree hexdump nl_receive
MODS+=sched sched_basic tc_action

TESTS=main common
TESTS+=options queue scan obj sched

OBJS=$(patsubst %,.objs/%.o,$(MODS))
TESTS_OBJS=$(patsubst %,.objs/tests/%.o,$(TESTS))
OUTPUTS=$(TARGETS) $(OBJS) $(TESTS_OBJS) .version.h
LIBS+=-l ev $(shell pkg-config --libs libmnl)
CFLAGS=-g -Wall -Wextra -Werror=pedantic -pedantic-errors -std=c11 -O0 -fPIC
CFLAGS+= -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -fno-strict-aliasing
CFLAGS+= -march=native -fprofile-arcs -ftest-coverage
CFLAGS+=$(shell pkg-config --cflags libmnl)

build: $(TARGET)

src/.version.h: src/*.h src/*.c tests/*.c tests/*.h
	cd src && ./version.sh > .version.h
.objs/options.o: src/.version.h

.objs .objs/tests:
	mkdir -p $@

.objs/%.o: src/%.c | .objs
	$(COMPILE.c) $(OUTPUT_OPTION) $<

.objs/tests/%.o: tests/%.c | .objs/tests
	$(COMPILE.c) $(OUTPUT_OPTION) $<

# libcheck integration inspired by https://github.com/siriobalmelli/libcheck_example
$(TEST_TARGET): LIBS += $(shell pkg-config --cflags --libs check)
$(TEST_TARGET): $(TESTS_OBJS)

$(TARGET): .objs/main.o

$(TARGETS): $(OBJS)
	$(CC) -o $@ $(CFLAGS) $^ $(LIBS)

test: $(TEST_TARGET)
	$(TEST_TARGET)

test_nofork: $(TEST_TARGET)
	CK_FORK=no $(TEST_TARGET)

valgrind: $(TEST_TARGET)
	CK_FORK=no valgrind --leak-check=full --show-leak-kinds=all --show-error-list=yes $(TEST_TARGET)

test_gdb: $(TEST_TARGET)
	CK_FORK=no gdb -ex run --args $(TEST_TARGET)

lcov: clean $(TARGETS)
	lcov --capture --initial --directory . --build-directory .objs/ --output-file coverage_base.info
	$(MAKE) test
	lcov --capture --directory . --build-directory .objs/ --output-file coverage_test.info
	lcov -a coverage_base.info -a coverage_test.info -o coverage.info
	genhtml coverage.info --output-directory cov/

clean-ish:
	rm -f $(OUTPUTS) src/.version.h
	rm -f coverage.info coverage_base.info coverage_test.info
	rm -rf cov/ .objs/

clean: clean-ish
	rm -rf scan-build/

scan-build:
	mkdir -p scan-build
	scan-build-17 -o scan-build $(MAKE) clean-ish $(TARGETS)
