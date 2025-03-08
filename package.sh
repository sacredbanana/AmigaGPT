#!/bin/bash

# Exit on error
set -e

# Run build_os3.sh with CLEAN=1
CLEAN=1 ./build_os3.sh

# Copy AmigaGPT executable to bundle directory
cp -R out/AmigaGPT bundle/AmigaGPT/AmigaGPT

# Run build_os4.sh with CLEAN=1
CLEAN=1 ./build_os4.sh

# Copy AmigaGPT_OS4 executable to bundle directory
cp -R out/AmigaGPT_OS4 bundle/AmigaGPT/AmigaGPT_OS4

# Run build_morphos.sh with CLEAN=1
CLEAN=1 ./build_morphos.sh

# Copy AmigaGPT_MorphOS executable to bundle directory
cp -R out/AmigaGPT_MorphOS bundle/AmigaGPT/AmigaGPT_MorphOS

# Copy catalog files to bundle directory
mkdir -p bundle/AmigaGPT/catalogs
rsync -av --include='*/' --include='*.catalog' --exclude='*' catalogs/ bundle/AmigaGPT/catalogs/

# Copy installer script
cp Install-AmigaGPT bundle/AmigaGPT/Install-AmigaGPT

# Change directory to bundle directory
cd bundle

# Delete LHA archive if exists
rm -f ../out/AmigaGPT.lha

# Create LHA archive with verbose output
lha av --ignore-mac-files ../out/AmigaGPT.lha AmigaGPT AmigaGPT.info

rm -f AmigaGPT/AmigaGPT AmigaGPT/AmigaGPT.info AmigaGPT/AmigaGPT_OS4 AmigaGPT/AmigaGPT_OS4.info AmigaGPT/AmigaGPT_MorphOS AmigaGPT/AmigaGPT_MorphOS.info
