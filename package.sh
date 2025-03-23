#!/bin/bash

# Exit on error
set -e

# Run build_os3.sh with CLEAN=1
CLEAN=1 ./build_os3.sh

# Copy AmigaGPT executable to bundle directory
cp -R out/AmigaGPT_OS3 bundle/AmigaGPT/AmigaGPT/AmigaGPT_OS3

# Run build_os4.sh with CLEAN=1
CLEAN=1 ./build_os4.sh

# Copy AmigaGPT_OS4 executable to bundle directory
cp -R out/AmigaGPT_OS4 bundle/AmigaGPT/AmigaGPT/AmigaGPT_OS4

# Run build_morphos.sh with CLEAN=1
CLEAN=1 ./build_morphos.sh

# Copy AmigaGPT_MorphOS executable to bundle directory
cp -R out/AmigaGPT_MorphOS bundle/AmigaGPT/AmigaGPT/AmigaGPT_MorphOS

# Copy catalog files to bundle directory
mkdir -p bundle/AmigaGPT/catalogs
rsync -av --include='*/' --include='*.catalog' --exclude='*' catalogs/ bundle/AmigaGPT/catalogs/

# Change directory to bundle directory
cd bundle

# Delete LHA archive if exists
rm -f ../out/AmigaGPT.lha

# Check if running on macOS and set appropriate LHA options
if [[ "$(uname -s)" == "Darwin" ]]; then
    LHA_OPTS="av --ignore-mac-files"
else
    LHA_OPTS="av"
fi

# Create LHA archive with verbose output
lha $LHA_OPTS ../out/AmigaGPT.lha AmigaGPT AmigaGPT.info

rm -f AmigaGPT/AmigaGPT/AmigaGPT_OS3 AmigaGPT/AmigaGPT/AmigaGPT_OS4 AmigaGPT/AmigaGPT/AmigaGPT_MorphOS
rm -dr AmigaGPT/catalogs
