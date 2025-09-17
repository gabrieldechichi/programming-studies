FROM ubuntu:24.04

# Set non-interactive frontend to avoid prompts during build
ENV DEBIAN_FRONTEND=noninteractive

# Set NVIDIA environment variables for GPU access
ENV NVIDIA_VISIBLE_DEVICES=all
ENV NVIDIA_DRIVER_CAPABILITIES=compute,utility,graphics,video

# Update package list and install runtime dependencies
RUN apt-get update && apt-get install -y \
    # Vulkan runtime libraries
    libvulkan1 \
    vulkan-tools \
    # FFmpeg runtime libraries (dynamically linked)
    libavformat60 \
    libavcodec60 \
    libavutil58 \
    libswscale7 \
    # NVIDIA utilities for GPU detection
    pciutils \
    # Clean up to reduce image size
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# The NVIDIA Vulkan drivers will be mounted at runtime via nvidia-container-runtime
# No need to install them in the image - they come from the host driver

# Create application directory
WORKDIR /app

# Copy pre-built application and shaders
COPY out/linux/video_renderer ./video_renderer
COPY out/linux/*.spv ./
# Also create the expected directory structure for shaders
RUN mkdir -p /app/out/linux
COPY out/linux/*.spv /app/out/linux/

# Make the binary executable
RUN chmod +x ./video_renderer

# Set the entrypoint to run the video renderer
ENTRYPOINT ["./video_renderer"]