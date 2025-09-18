#!/usr/bin/env python3

import runpod
import subprocess
import base64
import os
import json
import logging

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

def handler(event):
    """
    RunPod serverless handler for video rendering.

    Input: event with optional parameters for video generation
    Output: Base64-encoded video file
    """
    try:
        logger.info("=== Starting video rendering ===")
        logger.info(f"Event received: {event}")

        # Get input parameters (for future extensibility)
        input_data = event.get('input', {})
        logger.info(f"Input data: {input_data}")

        # Ensure we're in the correct directory
        logger.info(f"Current working directory: {os.getcwd()}")
        os.chdir('/app')
        logger.info(f"Changed to directory: {os.getcwd()}")

        # List files in current directory for debugging
        files = os.listdir('.')
        logger.info(f"Files in /app: {files}")

        # Check if video_renderer exists and is executable
        if not os.path.exists('./video_renderer'):
            logger.error("video_renderer binary not found!")
            return {"error": "Video renderer binary not found"}

        if not os.access('./video_renderer', os.X_OK):
            logger.error("video_renderer binary is not executable!")
            return {"error": "Video renderer binary is not executable"}

        # Clean up any existing output files
        if os.path.exists('output.mp4'):
            os.remove('output.mp4')
            logger.info("Removed existing output.mp4")

        # Check NVIDIA driver availability
        logger.info("=== Checking NVIDIA driver availability ===")
        try:
            nvidia_check = subprocess.run(['nvidia-smi'], capture_output=True, text=True, timeout=10)
            logger.info(f"nvidia-smi return code: {nvidia_check.returncode}")
            logger.info(f"nvidia-smi stdout: {nvidia_check.stdout}")
            if nvidia_check.returncode != 0:
                logger.warning(f"nvidia-smi stderr: {nvidia_check.stderr}")
        except Exception as e:
            logger.warning(f"nvidia-smi check failed: {e}")

        # Check for NVIDIA device files
        try:
            nvidia_devices = [f for f in os.listdir('/dev') if f.startswith('nvidia')]
            logger.info(f"NVIDIA device files in /dev: {nvidia_devices}")
        except Exception as e:
            logger.warning(f"Could not list /dev directory: {e}")

        # Check NVIDIA driver libraries
        try:
            ldd_check = subprocess.run(['ldd', './video_renderer'], capture_output=True, text=True, timeout=10)
            logger.info(f"ldd output for video_renderer: {ldd_check.stdout}")
        except Exception as e:
            logger.warning(f"ldd check failed: {e}")

        # Run the video renderer
        logger.info("=== Executing video renderer ===")
        result = subprocess.run(
            ['./video_renderer'],
            capture_output=True,
            text=True,
            timeout=30,  # 30 second timeout
            cwd='/app'
        )

        logger.info(f"Video renderer return code: {result.returncode}")
        logger.info(f"STDOUT: {result.stdout}")
        logger.info(f"STDERR: {result.stderr}")

        if result.returncode != 0:
            logger.error(f"Video renderer failed with return code {result.returncode}")
            return {
                "success": False,
                "error": f"Video rendering failed: {result.stderr or result.stdout}",
                "returncode": result.returncode,
                "stdout": result.stdout,
                "stderr": result.stderr
            }

        logger.info("Video renderer completed successfully")

        # Check if output file was created
        if not os.path.exists('output.mp4'):
            logger.error("Output file output.mp4 was not created")
            # List files again to see what was created
            files_after = os.listdir('.')
            logger.info(f"Files after execution: {files_after}")
            return {
                "success": False,
                "error": "Output video file was not generated",
                "files_before": files,
                "files_after": files_after
            }

        # Get file size for logging
        file_size = os.path.getsize('output.mp4')
        logger.info(f"Generated video file size: {file_size} bytes")

        # Read and encode the video file
        with open('output.mp4', 'rb') as video_file:
            video_data = video_file.read()

        # Encode to base64
        video_base64 = base64.b64encode(video_data).decode('utf-8')

        logger.info(f"Video encoded to base64, length: {len(video_base64)} characters")

        return {
            "success": True,
            "video": video_base64,
            "file_size": file_size,
            "encoding": "base64"
        }

    except subprocess.TimeoutExpired:
        logger.error("Video rendering timed out after 30 seconds")
        return {
            "success": False,
            "error": "Video rendering timed out"
        }

    except Exception as e:
        logger.error(f"Unexpected error: {str(e)}", exc_info=True)
        return {
            "success": False,
            "error": f"Unexpected error: {str(e)}"
        }

if __name__ == "__main__":
    runpod.serverless.start({"handler": handler})