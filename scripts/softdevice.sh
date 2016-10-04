#!/bin/bash

cmd=${1:? "$0 requires \"cmd\" as first argument"}

# get working path in absolute sense
path="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [[ $cmd != "help" ]]; then

	# configure environment variables, load configuration files, check targets and
	# assign serial_num from target
	source $path/_config.sh

	SD_BINDIR=${BLUENET_BIN_DIR}
fi

build() {
	echo "There is no real building step. Nordic provides a binary blob as SoftDevice"
	echo "However, we still need to extract the binary and the config blob"
	echo "  from $SOFTDEVICE_DIR/$SOFTDEVICE_DIR_HEX"
	$path/_softdevice_objcopy.sh $SOFTDEVICE_DIR/$SOFTDEVICE_DIR_HEX $SD_BINDIR $SOFTDEVICE $COMPILER_PATH $COMPILER_TYPE $SOFTDEVICE_NO_SEPARATE_UICR_SECTION
}

upload() {
	$path/_softdevice_upload.sh $SD_BINDIR $serial_num
}

all() {
	build
	sleep 1
	upload
}

case "$cmd" in
	build)
		build
		;;
	upload)
		upload
		;;
	all)
		all
		;;
	*)
		echo $"Usage: $0 {build|upload|switch|all}"
		exit 1
esac
