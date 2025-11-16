#!/bin/bash

# Build the video renderer Docker image

set -e

echo "Building video renderer Docker image..."

# Check if the binary exists
if [ ! -f "out/linux/video_renderer" ]; then
    echo "Error: Pre-built binary not found at out/linux/video_renderer"
    echo "Please build the application first using: ./build linux"
    exit 1
fi

# Check if shaders exist
if [ ! -f "out/linux/triangle.vert.spv" ] || [ ! -f "out/linux/triangle.frag.spv" ] || [ ! -f "out/linux/bgra_to_yuv.comp.spv" ]; then
    echo "Error: Shader files not found in out/linux/"
    echo "Please ensure all .spv files are built"
    exit 1
fi

# Build the Docker image
docker build -t video-renderer:latest .

echo "✅ Docker image 'video-renderer:latest' built successfully!"
echo ""
echo "To run the container:"
echo "  ./docker-run.sh"
echo ""
echo "Or manually:"
echo "  docker run --gpus all -v \$(pwd)/output:/app/output video-renderer:latest"