#!/bin/bash

# Push RunPod video renderer to Docker registry with version tagging

set -e

# Check if version argument is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <version>"
    echo "Example: $0 v1.0.10"
    exit 1
fi

VERSION=$1
REGISTRY_USER="gabrieldechichi"
IMAGE_NAME="video-renderer-runpod"
LOCAL_IMAGE="video-renderer-runpod:latest"
TAGGED_IMAGE="${REGISTRY_USER}/${IMAGE_NAME}:${VERSION}"
LATEST_IMAGE="${REGISTRY_USER}/${IMAGE_NAME}:latest"

echo "🚀 Pushing RunPod video renderer to Docker registry..."
echo "Version: ${VERSION}"
echo "Registry: ${REGISTRY_USER}/${IMAGE_NAME}"
echo ""

# Check if local image exists
if ! docker image inspect ${LOCAL_IMAGE} >/dev/null 2>&1; then
    echo "❌ Error: Local image '${LOCAL_IMAGE}' not found"
    echo "Please build the image first using: ./runpod-build.sh"
    exit 1
fi

# Tag with version
echo "📦 Tagging image as ${TAGGED_IMAGE}..."
docker tag ${LOCAL_IMAGE} ${TAGGED_IMAGE}

# Tag as latest
echo "📦 Tagging image as ${LATEST_IMAGE}..."
docker tag ${LOCAL_IMAGE} ${LATEST_IMAGE}

# Push versioned image
echo "⬆️  Pushing ${TAGGED_IMAGE}..."
docker push ${TAGGED_IMAGE}

# Push latest
echo "⬆️  Pushing ${LATEST_IMAGE}..."
docker push ${LATEST_IMAGE}

echo ""
echo "✅ Successfully pushed!"
echo ""
echo "Images pushed:"
echo "  - ${TAGGED_IMAGE}"
echo "  - ${LATEST_IMAGE}"
echo ""
echo "To use in RunPod:"
echo "  Docker Image: ${TAGGED_IMAGE}"
echo "  or"
echo "  Docker Image: ${LATEST_IMAGE}"
echo ""
echo "To pull locally:"
echo "  docker pull ${TAGGED_IMAGE}"