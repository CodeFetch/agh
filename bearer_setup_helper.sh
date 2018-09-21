#!/bin/sh

# This file is under GPL V2 or later, at your option.

# Why not a protocol handler?

. /lib/functions.sh
. /usr/share/libubox/jshn.sh

[ -x /usr/bin/readlink ] || exit 0

PATH="/usr/sbin:/usr/bin:/sbin:/bin"

# test
# . ./mydata_down

# Copied directly from ModemManager protocol handler by Aleksander Morgado
cdr2mask ()
{
	# Number of args to shift, 255..255, first non-255 byte, zeroes
	set -- $(( 5 - ($1 / 8) )) 255 255 255 255 $(( (255 << (8 - ($1 % 8))) & 255 )) 0 0 0
	[ $1 -gt 1 ] && shift $1 || shift
	echo ${1-0}.${2-0}.${3-0}.${4-0}
}

get_intfname() {
	(
	echo "mm_$(readlink /proc/self)"
	)
}

process_bearer() {
	if [ "${BEARER_INTERFACE}" != "unknown" ] ; then
		echo "Bearer is coming up"
		add_interface
	else
		echo "Bearer is going down."
	fi
}

add_interface() {
	local exit_status=0
	local uci_network_intf=""
	local logical_intf

	# Does interface exist?
	echo "Checking if interface exists."
	while [ "$exit_status" -eq 0 ]; do
		logical_intf=$(get_intfname)
		# logical_intf="mmtest"
		if [ "$(uci_get network $logical_intf)" = "interface" ]; then
			echo "It does, and is an interface"
			check_and_maybe_destroy_intf "$logical_intf"
			exit_status=$?
		else
			exit_status=1
		fi
	done

	echo "Logical interface: $logical_intf"

	case "$BEARER_IP_METHOD" in
		ppp) echo "ppp connection method support to be implemented";;
		static) add_interface_static "$logical_intf" ;;
		DHCP) add_interface_dhcp "$logical_intf" ;;
		*) echo "Unknown connection method $BEARER_IP_METHOD";;
	esac
}

add_interface_static() {
	local logical_intf_name="$1"

	echo "Ok, we are here"
	add_interface_common "$logical_intf_name"

	uci_set network "$logical_intf_name" proto static
	uci_set network "$logical_intf_name" ipaddr "${BEARER_IP_ADDRESS}"
	uci_set network "$logical_intf_name" netmask $(cdr2mask "${BEARER_IP_PREFIX}")
	uci_set network "$logical_intf_name" gateway "${BEARER_IP_GATEWAY}"

	# we may be interested in ip6addr, ip6ifaceid, ip6gw, ip6assign, ip6hint, ip6prefix and ip6class

	# Sorry, haven't found a way to do this via uci shell API
	uci add_list network."$logical_intf_name".dns="${BEARER_IP_DNS_1}"
	uci add_list network."$logical_intf_name".dns="${BEARER_IP_DNS_2}"

	# We may be interested in modifying the metric

	uci_set network "$logical_intf_name" metric 100

	interface_complete_common "$logical_intf_name"

}

add_interface_dhcp() {
	local logical_intf_name="$1"

	add_interface_common "$logical_intf_name"

	uci_set network "$logical_intf_name" proto dhcp

	uci_set network "$logical_intf_name" metric 100

	interface_complete_common "$logical_intf_name"

}

add_interface_common() {
	echo add_interface_common

	uci_add network interface "$logical_intf_name"

	uci_set network "$logical_intf_name" bearer_path "${BEARER_PATH}"
	uci_set network "$logical_intf_name" ifname "${BEARER_INTERFACE}"
	uci_set network "$logical_intf_name" mtu "${BEARER_IP_MTU}"
	uci_set network "$logical_intf_name" auto 0

	# we may be interested in setting macaddr, ipv6 and ip4table + ip6table for specific routes IPV4 / IPV6 tables ??
}

interface_complete_common() {
	local logical_intf_name="$1"

	ubus call network reload

	echo "ifup $logical_intf_name"
	ifup "$logical_intf_name"
	uci add_list "firewall.@zone[1].network=$logical_intf_name"
	/etc/init.d/firewall restart
}

# exit values: 1=success, 0=failure
check_and_maybe_destroy_intf() {
	local logical_intfname="$1"
	local bpath
	local exit_status=0
	local bcheck_exitstatus=0

	bpath="$(uci_get network $logical_intfname bearer_path not_present)"

	if [ "$bpath" = "not_present" ]; then
		echo "This does not seem to be one of our interfaces"
		return $exit_status
	fi

	json_init

	json_load "$(ifstatus $logical_intfname)"

	json_get_var is_up up

	if [ $is_up -eq 1 ]; then
		echo "Checking if bearer exists"
		mmcli -b "$bpath" >/dev/null 2>&1
		bcheck_exitstatus=$?
		if [ $bcheck_exitstatus -eq 1 ]; then
			ifdown "$logical_intfname"
			json_load "$(ifstatus $logical_intfname)"
			json_get_var is_up up
		fi
	fi

	if [ $is_up -eq 0 ]; then
		uci_remove network "$logical_intfname"
		uci del_list "firewall.@zone[1].network=$logical_intfname"
		ubus call network reload
		/etc/init.d/firewall restart
	else
		exit_status=1
	fi

	json_cleanup

	return $exit_status
}

search_and_remove_interface() {
	config_load network
	config_foreach "$1" interface
}

if [ ! -z ${BEARER_PATH} ]; then
	search_and_remove_interface check_and_maybe_destroy_intf
	process_bearer
fi
