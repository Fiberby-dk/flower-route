#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

get_version(){
	git describe --always --dirty
}

main(){
	local version="$(get_version)"
	echo '/* SPDX-''License''-Identifier: GPL-2.0-or-later */'
	printf "#define VERSION_GIT \"%s\"\n" "$version"
}

main
