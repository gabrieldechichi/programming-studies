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

def health_check():
    """Check the health of the video_renderer daemon"""
    global video_renderer_process

    daemon_alive = False
    socket_exists = os.path.exists(SOCKET_PATH)

    if video_renderer_process is not None:
        poll_result = video_renderer_process.poll()
        daemon_alive = poll_result is None
        if daemon_alive:
            logger.info(f"video_renderer_process is ALIVE (pid: {video_renderer_process.pid})")
        else:
            logger.info(f"video_renderer_process is DEAD (exit code: {poll_result})")
    else:
        logger.info("video_renderer_process is None")

    logger.info(f"Socket exists: {socket_exists}")

    return {
        "success": True,
        "daemon_alive": daemon_alive,
        "socket_exists": socket_exists,
        "pid": video_renderer_process.pid if video_renderer_process and daemon_alive else None
    }

def generate_video(input_data):
    """Generate video by sending request to daemon and returning its response"""
    seconds = input_data.get('seconds', 8.33)  # Default ~200 frames at 24fps
    logger.info(f"Rendering {seconds} seconds of video")

    # Send render request to daemon
    response = send_render_request(seconds)

    if not response:
        return {
            "success": False,
            "error": "Failed to get response from video renderer daemon"
        }

    # Return the raw response from the daemon
    logger.info(f"Returning daemon response: {response}")
    return response

def handler(event):
    """
    RunPod serverless handler with multiple endpoints.

    Endpoints:
    - /health: Check daemon status
    - /generate_video: Generate video and return daemon response
    """
    try:
        logger.info("=== Processing request ===")
        logger.info(f"Event received: {event}")

        # Get input data and endpoint
        input_data = event.get('input', {})
        endpoint = input_data.get('endpoint', '/generate_video')  # Default to generate_video

        logger.info(f"Endpoint requested: {endpoint}")

        # Route to appropriate handler
        if endpoint == '/health':
            return health_check()
        elif endpoint == '/generate_video':
            return generate_video(input_data)
        else:
            return {
                "success": False,
                "error": f"Unknown endpoint: {endpoint}"
            }

    except Exception as e:
        logger.error(f"Unexpected error: {str(e)}", exc_info=True)
        return {
            "success": False,
            "error": f"Unexpected error: {str(e)}"
        }

if __name__ == "__main__":
    runpod.serverless.start({"handler": handler})