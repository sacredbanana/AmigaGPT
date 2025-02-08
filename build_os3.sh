#!/usr/bin/env bash

# This script is used to build the AmigaOS 3 version of the app using the Docker container
# Set the CLEAN environment variable to 1 to clean the build directory before building

set -e

if [[ "${CLEAN}" == "1" ]]; then
	docker run --rm \
	-v ${PWD}:/work \
	-e USER=$(id -u) -e GROUP=$(id -g) -e DEBUG=${DEBUG} \
	sacredbanana/amiga-compiler:m68k-amigaos make clean
fi

docker run --rm \
	-v ${PWD}:/work \
	-e USER=$(id -u) -e GROUP=$(id -g) -e DEBUG=${DEBUG} \
	sacredbanana/amiga-compiler:m68k-amigaos make
