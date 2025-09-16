#!/bin/bash

echo "Building optimized video renderer..."

# Compile the fast renderer
clang -o video_renderer_fast \
    src/video_renderer_fast.c \
    -I../../../other/sokol \
    -I. \
    -framework Metal \
    -framework CoreGraphics \
    -framework Foundation \
    -ObjC \
    -O3 \
    -march=native \
    -pthread \
    -lm

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo "Running video renderer..."
    time ./video_renderer_fast
else
    echo "Build failed!"
    exit 1
fi