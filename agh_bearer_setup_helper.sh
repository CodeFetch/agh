#!/bin/sh

# Stores bearers state for usage by the AGH protocol handler (agh.proto).
# This file is distributed under the GPL license, version 2.0 or, at your option, any later version.

BEARERS_DESCRIPTIONS_BASE_DIR="/tmp/"
BEARERS_DESCRIPTIONS_PATH="${BEARERS_DESCRIPTIONS_BASE_DIR}/bearers_data"
CURRENT_BFILE=""
INCOMPLETE_BEARERS_DESCRIPTIONS=0

# . /tmp/data_model

bearer_descriptions_dir_init() {
	if [ ! -d "${BEARERS_DESCRIPTIONS_PATH}" ]; then
		echo "Creating beaarer descriptions directory at ${BEARERS_DESCRIPTIONS_PATH}"
		mkdir -p "${BEARERS_DESCRIPTIONS_PATH}"/settings "${BEARERS_DESCRIPTIONS_PATH}"/state "${BEARERS_DESCRIPTIONS_PATH}"/descriptions
	else
		echo "Base directory already exists."
	fi
}

process_bearer_data() {
	local BFILE
	local SETTINGS_FILE
	local STATE_FILE

	BFILE="${BEARERS_DESCRIPTIONS_PATH}/descriptions/bb$(basename ${BEARER_PATH})"
	SETTINGS_FILE="${BEARERS_DESCRIPTIONS_PATH}/settings/bb$(basename ${BEARER_PATH})"
	STATE_FILE="${BEARERS_DESCRIPTIONS_PATH}/state/bb$(basename ${BEARER_PATH})"
	CURRENT_BFILE="${BFILE}"

	echo "Processing bearer data for ${BEARER_PATH}"

	set | grep '^BEARER_' >"${BFILE}"
	if [ "${BEARER_INTERFACE}" == "unknown" ]; then
		if [ -n "${AGH_PROFILE_BEARER_SECTION}" ]; then
			set | grep 'AGH_PROFILE_BEARER_' >"${SETTINGS_FILE}"
			INCOMPLETE_BEARERS_DESCRIPTIONS=1
		else
			rm "${STATE_FILE}"
		fi
	else
		echo "CONNECTED_SINCE=$(date +%F+%H:%M:%S)" >"${STATE_FILE}"
	fi
}

update_bearers_descriptions() {
	local no_bearer
	local bearer_connected

	ls "${BEARERS_DESCRIPTIONS_PATH}/descriptions" | while read a; do
		if [ "${BEARERS_DESCRIPTIONS_PATH}"/descriptions/"${a}" == "${CURRENT_BFILE}" ]; then
			echo "skipping ${a}"
			continue
		fi

		(

		. "${BEARERS_DESCRIPTIONS_PATH}"/descriptions/"${a}"

		mmcli -b "${BEARER_PATH}" -K >/dev/null
		no_bearer=$?
		if [ "${no_bearer}" -ne 0 ]; then
			echo "Deleting $a obsolescent bearer description."
			rm "${BEARERS_DESCRIPTIONS_PATH}"/descriptions/"${a}" \
				"${BEARERS_DESCRIPTIONS_PATH}"/state/"${a}" \
				"${BEARERS_DESCRIPTIONS_PATH}"/settings/"${a}"
		else
			bearer_connected=$(mmcli -b "${BEARER_PATH}" -K | tr -d ' ' | grep 'connected' | cut -d ':' -f 2)

			echo "bearer_connected = ${bearer_connected}"

			if [ "${bearer_connected}" != "yes" ]; then
				if [ "${BEARER_INTERFACE}" != "unknown" ]; then
					echo "setting BEARER_INTERFACE to unknown ( ${BEARERS_DESCRIPTIONS_PATH}/descriptions/${a} )"
					echo BEARER_INTERFACE="unknown" >>"${BEARERS_DESCRIPTIONS_PATH}"/descriptions/"${a}"
				fi
			fi
		fi

		)

	done
}

if [ ! -z "${BEARER_PATH}" ]; then
	bearer_descriptions_dir_init
	process_bearer_data
	update_bearers_descriptions

	if [ "${INCOMPLETE_BEARERS_DESCRIPTIONS}" -eq 0 ]; then
		ifup agh_mm
	else
		echo "Incomplete bearers descriptions are present, not causing the protocol handler to refresh its knowledge."
	fi
fi
