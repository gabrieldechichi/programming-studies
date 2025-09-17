#!/bin/bash

# Test the RunPod video renderer locally

set -e

echo "Testing RunPod video renderer locally..."

# Check if Docker image exists
if ! docker image inspect video-renderer-runpod:latest >/dev/null 2>&1; then
    echo "Error: Docker image 'video-renderer-runpod:latest' not found"
    echo "Please build the image first using: ./runpod-build.sh"
    exit 1
fi

echo "Starting local RunPod test server..."
echo "This will start the handler in local server mode for testing"
echo ""

# Run the container with local testing
docker run --rm \
    --gpus all \
    -e NVIDIA_DRIVER_CAPABILITIES=all \
    -e NVIDIA_VISIBLE_DEVICES=all \
    -p 8000:8000 \
    video-renderer-runpod:latest \
    python3 runpod_handler.py --rp_serve_api --rp_api_port 8000 --rp_api_host 0.0.0.0 --rp_log_level INFO

echo ""
echo "Local test server started!"
echo "You can now test the endpoint by sending a POST request to:"
echo "  http://localhost:8000/run"
echo ""
echo "Example curl command:"
echo '  curl -X POST http://localhost:8000/run -H "Content-Type: application/json" -d '"'"'{"input": {"test": true}}'"'"''