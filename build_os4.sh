#!/usr/bin/env bash

# This script is used to build the AmigaOS 4 version of the app using the Docker container
# Set the CLEAN environment variable to 1 to clean the build directory before building

set -e

if [[ "${CLEAN}" == "1" ]]; then
	rm -rf build
fi

# Get the current user ID and group ID. We want ti to be the same as the host user so that
# the files created by the container have the same permissions as the host user.
USER_ID=$(id -u)
GROUP_ID=$(id -g)

docker run --rm \
	-v ${PWD}:/work \
	-u ${USER_ID}:${GROUP_ID} \
	-e USER=${USER_ID} -e GROUP=${GROUP_ID} -e DEBUG=${DEBUG} \
	sacredbanana/amiga-compiler:ppc-amigaos make -f Makefile.OS4
