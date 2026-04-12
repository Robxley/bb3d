#!/bin/bash
# Script utilitaire pour compiler les shaders en SPIR-V (Vulkan 1.3)

SHADERS_DIR="assets/shaders"
OUTPUT_DIR="assets/shaders/spv"

mkdir -p $OUTPUT_DIR

for shader in $SHADERS_DIR/*.{vert,frag,comp,geom,tesc,tese}; do
    if [ -f "$shader" ]; then
        filename=$(basename "$shader")
        echo "Compiling $filename..."
        glslc --target-env=vulkan1.3 "$shader" -o "$OUTPUT_DIR/$filename.spv"
    fi
done

echo "Compilation terminée."
