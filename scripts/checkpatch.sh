#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

prepare_file(){
	local file="$1"
	# checkpatch.pl doesn't like libev's macros
	cat "$file" |
		sed -e 's/EV_P_/EV_P,/g' -e 's/EV_A_/EV_A,/g' |
		sed -e 's/EV_A/loop/g' -e 's/EV_P/struct ev_loop *loop/g' |
		sed -e 's/Suite/struct Suite/g'

}

CHECKPATCH=""
checkpatch(){
	local file="$1"
	local fixed_file=".checkpatch/$file"
	mkdir -p "$(dirname "$fixed_file")"
	prepare_file "$file" > "$fixed_file"
	"$CHECKPATCH" --no-tree --terse --summary -f "$fixed_file"
}

bail(){
	echo "$@" >&2
	exit 1
}

test_checkpath_location(){
	local candidate_path="$1/scripts/checkpatch.pl"
	if [ -z "$CHECKPATCH" ] && [ -x "$candidate_path" ] ; then
		CHECKPATCH="$candidate_path"
	fi
}

find_checkpatch(){
	test_checkpath_location /usr/src/linux
	test_checkpath_location $HOME/linux
	if [ -z "$CHECKPATCH" ] ; then
		bail "unable to find kernel tree"
	fi
}

find_targets(){
	if [ $# -eq 0 ] ; then
		find src/ tests/ scripts/ -name '*.h' -or -name '*.c' -or -iname '*.sh' | sort
	else
		echo "$@"
	fi
}

main(){
	find_checkpatch

	rm -rf .checkpatch

	for file in $(find_targets "$@"); do
		checkpatch "$file"
	done

	rm -rf .checkpatch
}

main "$@"
