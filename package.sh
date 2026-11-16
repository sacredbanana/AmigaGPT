#!/bin/bash

# Exit on error
set -e

# Run build_os3.sh with CLEAN=1
CLEAN=1 ./build_os3.sh

# Copy AmigaGPT executable to bundle directory
cp -R out/AmigaGPT_OS3 bundle/AmigaGPT/AmigaGPT/AmigaGPT_OS3

# Copy AmigaGPTD executable to bundle directory
cp -R out/AmigaGPTD_OS3 bundle/AmigaGPT/AmigaGPT/AmigaGPTD_OS3

# Run build_os4.sh with CLEAN=1
CLEAN=1 ./build_os4.sh

# Copy AmigaGPT_OS4 executable to bundle directory
cp -R out/AmigaGPT_OS4 bundle/AmigaGPT/AmigaGPT/AmigaGPT_OS4

# Copy AmigaGPTD_OS4 executable to bundle directory
cp -R out/AmigaGPTD_OS4 bundle/AmigaGPT/AmigaGPT/AmigaGPTD_OS4

# Run build_morphos.sh with CLEAN=1
CLEAN=1 ./build_morphos.sh

# Copy AmigaGPT_MorphOS executable to bundle directory
cp -R out/AmigaGPT_MorphOS bundle/AmigaGPT/AmigaGPT/AmigaGPT_MorphOS

# Copy AmigaGPTD_MorphOS executable to bundle directory
cp -R out/AmigaGPTD_MorphOS bundle/AmigaGPT/AmigaGPT/AmigaGPTD_MorphOS

# Copy the BestTextEditor MUI classes (built by the build scripts above)
for platform in OS3 OS4 MorphOS; do
    cp out/BestTextEditor_${platform}.mcc out/BestTextEditor_${platform}.mcp bundle/AmigaGPT/MCC/
done

# Copy catalog files to bundle directory, BestTextEditor.mcp's included
mkdir -p bundle/AmigaGPT/catalogs
rsync -av --include='*/' --include='*.catalog' --exclude='*' catalogs/ bundle/AmigaGPT/catalogs/
rsync -av --include='*/' --include='*.catalog' --exclude='*' mcc/BestTextEditor/catalogs/ bundle/AmigaGPT/catalogs/

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

rm -f AmigaGPT/AmigaGPT/AmigaGPT_OS3 AmigaGPT/AmigaGPT/AmigaGPT_OS4 AmigaGPT/AmigaGPT/AmigaGPT_MorphOS AmigaGPT/AmigaGPT/AmigaGPTD_OS3 AmigaGPT/AmigaGPT/AmigaGPTD_OS4 AmigaGPT/AmigaGPT/AmigaGPTD_MorphOS
rm -dr AmigaGPT/catalogs
rm -f AmigaGPT/MCC/BestTextEditor_*.mcc AmigaGPT/MCC/BestTextEditor_*.mcp
