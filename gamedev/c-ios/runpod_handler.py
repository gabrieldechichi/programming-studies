#!/usr/bin/env python3

import runpod
import subprocess
import base64
import os
import json
import logging
import time
import atexit
import socket

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

SOCKET_PATH = "/tmp/video_renderer.sock"
video_renderer_process = subprocess.Popen(
    ['./video_renderer'],
    cwd='/app'
)

def send_render_request(seconds=8.33):
    """Send a render request to the daemon via Unix socket and get the response"""

    # Check if socket exists
    if not os.path.exists(SOCKET_PATH):
        logger.error(f"Socket file {SOCKET_PATH} does not exist")
        return None

    try:
        # Create Unix socket client
        client_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

        # Connect to the daemon
        client_socket.connect(SOCKET_PATH)
        logger.info(f"Connected to socket: {SOCKET_PATH}")

        # Send JSON request
        request = {"seconds": seconds}
        request_str = json.dumps(request) + '\n'

        logger.info(f"Sending render request: {request}")
        client_socket.send(request_str.encode('utf-8'))

        # Read response (up to 10MB for large base64 videos)
        logger.info("Waiting for response...")
        response_data = b''
        while True:
            chunk = client_socket.recv(4096)
            if not chunk:
                break
            response_data += chunk
            # Check if we have a complete JSON response (ends with })
            if response_data.rstrip().endswith(b'}'):
                break

        client_socket.close()

        if not response_data:
            logger.error("No response received from daemon")
            return None

        # Decode and parse JSON response
        try:
            response_str = response_data.decode('utf-8')
            response = json.loads(response_str)
            logger.info(f"Received response: success={response.get('success')}, file_size={response.get('file_size', 'N/A')}")
            return response
        except json.JSONDecodeError as e:
            logger.error(f"Failed to parse JSON response: {e}")
            logger.error(f"Raw response (first 500 chars): {response_str[:500]}")
            return None

    except ConnectionRefusedError:
        logger.error(f"Connection refused to {SOCKET_PATH} - daemon may not be ready")
        return None
    except FileNotFoundError:
        logger.error(f"Socket file {SOCKET_PATH} not found")
        return None
    except Exception as e:
        logger.error(f"Error communicating with daemon: {e}")
        return None

# Don't initialize at module load - let handler do it on first request

def handler(event):
    """
    RunPod serverless handler for video rendering using persistent daemon.

    Input: event with optional parameters for video generation
    Output: Base64-encoded video file
    """
    try:
        logger.info("=== Processing video rendering request ===")
        logger.info(f"Event received: {event}")

        # Check if video_renderer_process is alive
        global video_renderer_process
        if video_renderer_process is None:
            logger.info("video_renderer_process is None")
        else:
            poll_result = video_renderer_process.poll()
            if poll_result is None:
                logger.info(f"video_renderer_process is ALIVE (pid: {video_renderer_process.pid})")
            else:
                logger.info(f"video_renderer_process is DEAD (exit code: {poll_result}, pid: {video_renderer_process.pid})")

        # Check if socket exists
        if os.path.exists(SOCKET_PATH):
            logger.info(f"Socket file EXISTS at {SOCKET_PATH}")
        else:
            logger.info(f"Socket file DOES NOT EXIST at {SOCKET_PATH}")

        # Get input parameters
        input_data = event.get('input', {})
        seconds = input_data.get('seconds', 8.33)  # Default ~200 frames at 24fps
        logger.info(f"Rendering {seconds} seconds of video")

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