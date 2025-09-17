#!/bin/bash

# Run the video renderer Docker container

set -e

echo "Running video renderer container..."

# Create output directory if it doesn't exist
mkdir -p output

# Check if Docker image exists
if ! docker image inspect video-renderer:latest >/dev/null 2>&1; then
    echo "Error: Docker image 'video-renderer:latest' not found"
    echo "Please build the image first using: ./docker-build.sh"
    exit 1
fi

# Check if NVIDIA Docker runtime is available
if ! docker info | grep -q nvidia; then
    echo "Warning: NVIDIA Docker runtime not detected"
    echo "Make sure you have nvidia-docker2 installed and Docker daemon restarted"
    echo "Proceeding anyway..."
fi

echo "Running container with GPU access..."
echo "Output will be saved to ./output/output.mp4"
echo ""

# Run the container with privileged access for full GPU support
docker run --rm \
    --privileged \
    --gpus all \
    -e NVIDIA_DRIVER_CAPABILITIES=all \
    -e NVIDIA_VISIBLE_DEVICES=all \
    -v "$(pwd)/output:/app/output" \
    --entrypoint=/bin/bash \
    video-renderer:latest -c "/app/video_renderer && mv output.mp4 /app/output/ && chmod 666 /app/output/output.mp4 2>/dev/null || echo 'Video created in container'"

echo ""
echo "✅ Container execution completed!"

# Check if output file was created
if [ -f "output/output.mp4" ]; then
    echo "✅ Video file created: ./output/output.mp4"
    echo "File size: $(du -h output/output.mp4 | cut -f1)"

    # Fix permissions if file is owned by root
    if [ "$(stat -c %U output/output.mp4)" = "root" ]; then
        echo ""
        echo "⚠️  Video file is owned by root. To fix permissions, run:"
        echo "    sudo chown $USER:$USER output/output.mp4"
        echo ""
        echo "Or play the video with:"
        echo "    sudo -E mpv output/output.mp4"
        echo "    sudo -E vlc output/output.mp4"
    fi
else
    echo "❌ No output file found. Check container logs for errors."
    exit 1
fi