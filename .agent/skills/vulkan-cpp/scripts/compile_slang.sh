#!/bin/bash
# Script utilitaire pour compiler les shaders Slang en SPIR-V (Vulkan 1.4)

SHADERS_DIR="assets/shaders"
OUTPUT_DIR="assets/shaders/spv"

mkdir -p $OUTPUT_DIR

for shader in $SHADERS_DIR/*.slang; do
    if [ -f "$shader" ]; then
        filename=$(basename "$shader" .slang)
        echo "Compiling $filename.slang..."
        slangc "$shader" -target spirv -profile sm_6_6 -O3 -o "$OUTPUT_DIR/$filename.spv"
    fi
done

echo "Compilation Slang terminée."
