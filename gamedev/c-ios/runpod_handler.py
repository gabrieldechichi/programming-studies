#!/usr/bin/env python3

import runpod
import subprocess
import base64
import os
import json
import logging
import time
import atexit

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Global process for persistent video renderer daemon
video_renderer_process = None

def cleanup_process():
    """Clean up the video renderer process on exit"""
    global video_renderer_process
    if video_renderer_process and video_renderer_process.poll() is None:
        logger.info("Terminating video renderer daemon...")
        video_renderer_process.terminate()
        try:
            video_renderer_process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            logger.warning("Force killing video renderer daemon...")
            video_renderer_process.kill()

# Register cleanup function
atexit.register(cleanup_process)

def start_video_renderer_daemon():
    """Start the persistent video renderer daemon"""
    global video_renderer_process

    if video_renderer_process and video_renderer_process.poll() is None:
        # Process is already running
        return True

    logger.info("Starting video renderer daemon...")

    # Ensure we're in the correct directory
    os.chdir('/app')

    # Check if video_renderer exists and is executable
    if not os.path.exists('./video_renderer'):
        logger.error("video_renderer binary not found!")
        return False

    if not os.access('./video_renderer', os.X_OK):
        logger.error("video_renderer binary is not executable!")
        return False

    try:
        video_renderer_process = subprocess.Popen(
            ['./video_renderer'],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,  # Line buffered
            cwd='/app'
        )

        # Give it a moment to initialize
        time.sleep(0.1)

        # Check if process started successfully
        if video_renderer_process.poll() is not None:
            # Process has already terminated
            stdout, stderr = video_renderer_process.communicate()
            logger.error(f"Video renderer daemon failed to start: {stderr}")
            logger.error(f"STDOUT: {stdout}")
            return False

        logger.info("Video renderer daemon started successfully")
        return True

    except Exception as e:
        logger.error(f"Failed to start video renderer daemon: {e}")
        return False

def send_render_request(seconds=8.33):
    """Send a render request to the daemon and get the response"""
    global video_renderer_process

    if not video_renderer_process or video_renderer_process.poll() is not None:
        logger.error("Video renderer daemon is not running")
        return None

    try:
        # Send JSON request
        request = {"seconds": seconds}
        request_str = json.dumps(request) + '\n'

        logger.info(f"Sending render request: {request}")
        video_renderer_process.stdin.write(request_str)
        video_renderer_process.stdin.flush()

        # Read response
        logger.info("Waiting for response...")
        response_line = video_renderer_process.stdout.readline()

        if not response_line:
            logger.error("No response received from daemon")
            return None

        # Try to parse JSON response
        try:
            response = json.loads(response_line.strip())
            logger.info(f"Received response: {response}")
            return response
        except json.JSONDecodeError as e:
            logger.error(f"Failed to parse JSON response: {e}")
            logger.error(f"Raw response: {response_line}")
            return None

    except Exception as e:
        logger.error(f"Error communicating with daemon: {e}")
        return None

def handler(event):
    """
    RunPod serverless handler for video rendering using persistent daemon.

    Input: event with optional parameters for video generation
    Output: Base64-encoded video file
    """
    try:
        logger.info("=== Starting video rendering (daemon mode) ===")
        logger.info(f"Event received: {event}")

        # Get input parameters
        input_data = event.get('input', {})
        seconds = input_data.get('seconds', 8.33)  # Default ~200 frames at 24fps
        logger.info(f"Rendering {seconds} seconds of video")

        # Start daemon if not running
        if not start_video_renderer_daemon():
            return {
                "success": False,
                "error": "Failed to start video renderer daemon"
            }

        # Clean up any existing output files
        if os.path.exists('/app/output.mp4'):
            os.remove('/app/output.mp4')
            logger.info("Removed existing output.mp4")

        # Send render request
        response = send_render_request(seconds)

        if not response:
            return {
                "success": False,
                "error": "Failed to get response from video renderer daemon"
            }

        if not response.get('success', False):
            return {
                "success": False,
                "error": response.get('error', 'Unknown error from daemon')
            }

        # Check if output file was created
        if not os.path.exists('/app/output.mp4'):
            logger.error("Output file output.mp4 was not created")
            return {
                "success": False,
                "error": "Output video file was not generated"
            }

        # Get file size for logging
        file_size = os.path.getsize('/app/output.mp4')
        logger.info(f"Generated video file size: {file_size} bytes")

        # Read and encode the video file
        with open('/app/output.mp4', 'rb') as video_file:
            video_data = video_file.read()

        # Encode to base64
        video_base64 = base64.b64encode(video_data).decode('utf-8')

        logger.info(f"Video encoded to base64, length: {len(video_base64)} characters")

        return {
            "success": True,
            "video": video_base64,
            "file_size": file_size,
            "encoding": "base64",
            "seconds_rendered": seconds
        }

    except Exception as e:
        logger.error(f"Unexpected error: {str(e)}", exc_info=True)
        return {
            "success": False,
            "error": f"Unexpected error: {str(e)}"
        }

if __name__ == "__main__":
    runpod.serverless.start({"handler": handler})