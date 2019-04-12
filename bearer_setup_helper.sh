#!/bin/sh

BEARERS_DESCRIPTIONS_BASE_DIR="/tmp/"
BEARERS_DESCRIPTIONS_PATH="${BEARERS_DESCRIPTIONS_BASE_DIR}/bearers_descriptions"
CURRENT_BFILE=""

# . /tmp/data_model

bearer_descriptions_dir_init() {
	if [ ! -d "${BEARERS_DESCRIPTIONS_PATH}" ]; then
		echo "Creating beaarer descriptions directory at ${BEARERS_DESCRIPTIONS_PATH}"
		mkdir "${BEARERS_DESCRIPTIONS_PATH}"
	else
		echo "Base directory already exists."
	fi
}

process_bearer_data() {
	BFILE="${BEARERS_DESCRIPTIONS_PATH}/bb$(basename ${BEARER_PATH})"
	CURRENT_BFILE="${BFILE}"

	echo "Processing bearer data for ${BEARER_PATH}"

	if [ "${BEARER_INTERFACE}" == "unknown" ]; then
		rm "${BFILE}"
	else
		set | grep 'BEARER_' >"${BFILE}"
	fi
}

update_bearers_descriptions() {
	ls "${BEARERS_DESCRIPTIONS_PATH}" | while read a; do
		if [ "${BEARERS_DESCRIPTIONS_PATH}"/"${a}" == "${CURRENT_BFILE}" ]; then
			echo "skipping ${a}"
			continue
		fi
		. "${BEARERS_DESCRIPTIONS_PATH}"/"${a}"
		BCONNECTED=$(mmcli -b "${BEARER_PATH}" -K | tr -d ' ' | grep 'bearer.status.connected:' | cut -d ':' -f 2)
		if [ "${BCONNECTED}" != "yes" ]; then
			echo "Deleting $a obsolescent bearer description."
			rm "${BEARERS_DESCRIPTIONS_PATH}"/"${a}"
		fi
	done
}

if [ ! -z "${BEARER_PATH}" ]; then
	bearer_descriptions_dir_init
	process_bearer_data
	update_bearers_descriptions
	ifup agh_mm
fi
