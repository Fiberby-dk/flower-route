#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later

##################################################
#  prepare a mlx5_core device for TC offloading  #
##################################################

#
# how to enable TC flower offloading is a bit different
# from device to device, this scripts goes through
# the steps needed for mlx5_core devices
#

export PATH=/usr/sbin:/usr/bin:/sbin:/bin

bail(){
	local msg="$(printf "$@")"
	echo "$msg" >&2
	e"$msg"1
}

is_netdev(){
	[ -d "/sys/class/net/$1/" ]
}

get_device_driver(){
	local ifname="$1"
	readlink -f "/sys/class/net/$ifname/device/driver" 2>/dev/null
}

is_mlx5(){
	local ifname="$1"
	local device_driver="$(get_device_driver "$ifname")"
	local expected='/sys/bus/pci/drivers/mlx5_core'
	[ "v$device_driver" = "v$expected" ]
}

wait_for_netdev(){
	local ifname="$1"
	sleep 5
	for i in $(seq 1 30); do
		if is_netdev "$ifname" ; then
			return 0
		fi
		sleep 1
	done
	bail 'lost hope while waiting for %s' "$ifname"
}

get_eswitch_mode_raw(){
	local devaddr="$1"
	devlink dev eswitch show "$devaddr" |
		grep -w mode |
		sed -e 's/^.* mode \([^ ]\+\) .*$/\1/g'
}

get_eswitch_mode(){
	local devaddr="$1"
	local mode="$(get_eswitch_mode_raw "$devaddr" 2>/dev/null)"
	if [ "$mode" = '' ] ; then
		echo legacy
	else
		echo "$mode"
	fi
}

set_eswitch_mode(){
	local devaddr="$1"
	local mode="$2"

	devlink dev eswitch set "$devaddr" mode "$mode"
}

get_pci_address(){
	local ifname="$1"
	ls -1d /sys/bus/*/devices/*/net/"$ifname" | cut -d/ -f4,6
}

ensure_switchdev(){
	local ifname="$1"
	local devaddr="$(get_pci_address "$ifname")"

	local current_mode="$(get_eswitch_mode "$devaddr")"

	if [ "$current_mode" = 'legacy' ] ; then
		set_eswitch_mode "$devaddr" 'switchdev'
		# it used to be so that the device would disappear
		# and come back again a few seconds later
		# TODO: find out when this changed and consider
		# to simplify this
		wait_for_netdev "$ifname"
	fi

	local new_mode="$(get_eswitch_mode "$devaddr")"
	if [ "$new_mode" != "switchdev" ] ; then
		bail "error changing eswitch mode"
	fi

	return 0
}

get_max_queues(){
	local ifname="$1"
	ethtool -l "$ifname" |
		grep -B10 'Current hardware settings:' |
		grep 'Combined' |
		cut -d: -f2 |
		xargs echo
}

prepare_device(){
	local ifname="$1"

	# change to switchdev mode
	ensure_switchdev "$ifname"

	# change queues back to max, it defaults to 1 in switchdev mode
	ethtool -L "$ifname" combined $(get_max_queues "$ifname")

	# enable TC offload
	ethtool -K "$ifname" hw-tc-offload on

	# add ingress qdisc
	tc qdisc add dev "$ifname" ingress 2>/dev/null
}

main(){
	if [ $# -ne 1 ] ; then
		bail "Usage: %s <ifname>" "$0"
	fi

	local ifname="$1"
	if is_netdev "$ifname" ; then
		if is_mlx5 "$ifname" ; then
			prepare_device "$ifname"
		else
			bail '%s is not a mlx5_core device' "$ifname"
		fi
	else
		bail '"%s" is not a valid interface' "$ifname"
	fi
}

main "$@"
