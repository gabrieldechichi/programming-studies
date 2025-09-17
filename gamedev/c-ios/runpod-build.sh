#!/bin/bash

# Build the video renderer Docker image for RunPod deployment

set -e

echo "Building video renderer Docker image for RunPod..."

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

# Build the Docker image for RunPod
docker build -f Dockerfile.runpod -t video-renderer-runpod:latest .

echo "✅ Docker image 'video-renderer-runpod:latest' built successfully!"
echo ""
echo "Next steps:"
echo "1. Test locally: ./runpod-test.sh"
echo "2. Push to registry: docker tag video-renderer-runpod:latest your-dockerhub-username/video-renderer-runpod:latest"
echo "3. Push: docker push your-dockerhub-username/video-renderer-runpod:latest"
echo "4. Deploy on RunPod using the pushed image"