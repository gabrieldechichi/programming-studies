#!/usr/bin/env python3

import socket
import json
import time

def test_socket_connection():
    """Test Unix socket connection to video_renderer daemon"""

    socket_path = "/tmp/video_renderer.sock"

    print(f"Connecting to {socket_path}...")

    # Create Unix socket client
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    try:
        # Connect to the server
        client.connect(socket_path)
        print("Connected successfully!")

        # Prepare request
        request = {"seconds": 0.5}  # Short test video
        request_json = json.dumps(request) + '\n'

        print(f"Sending request: {request}")
        client.send(request_json.encode())

        # Receive response
        print("Waiting for response...")
        response = client.recv(1024 * 1024)  # 1MB buffer

        if response:
            response_str = response.decode('utf-8')
            print(f"Response received: {response_str[:200]}...")  # First 200 chars

            # Try to parse JSON
            try:
                response_json = json.loads(response_str)
                print(f"Success: {response_json.get('success')}")
                print(f"File size: {response_json.get('file_size', 'N/A')} bytes")
            except json.JSONDecodeError as e:
                print(f"Failed to parse JSON response: {e}")
        else:
            print("No response received")

    except FileNotFoundError:
        print(f"Error: Socket file {socket_path} not found.")
        print("Make sure the video_renderer daemon is running.")
    except ConnectionRefusedError:
        print(f"Error: Connection refused to {socket_path}")
        print("Make sure the video_renderer daemon is running.")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        client.close()
        print("Connection closed")

if __name__ == "__main__":
    test_socket_connection()