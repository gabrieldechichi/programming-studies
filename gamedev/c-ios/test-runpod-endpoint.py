#!/usr/bin/env python3

import requests
import json
import base64
import time

def test_local_endpoint():
    """Test the local RunPod endpoint"""

    url = "http://localhost:8000/run"

    # Test payload
    payload = {
        "input": {
            "test": True,
            "message": "Testing video generation"
        }
    }

    print("Testing RunPod video renderer endpoint...")
    print(f"URL: {url}")
    print(f"Payload: {json.dumps(payload, indent=2)}")
    print()

    try:
        print("Sending request...")
        start_time = time.time()

        response = requests.post(
            url,
            json=payload,
            headers={'Content-Type': 'application/json'},
            timeout=60  # 60 second timeout
        )

        end_time = time.time()
        duration = end_time - start_time

        print(f"Response received in {duration:.2f} seconds")
        print(f"Status Code: {response.status_code}")
        print()

        if response.status_code == 200:
            response_data = response.json()
            print(f"Response data: {json.dumps(response_data, indent=2)}")

            if response_data.get("success"):
                print("✅ Success!")
                print(f"File size: {response_data.get('file_size', 'unknown')} bytes")
                print(f"Encoding: {response_data.get('encoding', 'unknown')}")

                # Get base64 video
                video_base64 = response_data.get("video")
                if video_base64:
                    print(f"Base64 length: {len(video_base64)} characters")

                    # Optionally save the video file
                    save_video = input("\nSave video to file? (y/n): ").lower().strip()
                    if save_video == 'y':
                        try:
                            video_data = base64.b64decode(video_base64)
                            with open('test_output.mp4', 'wb') as f:
                                f.write(video_data)
                            print("Video saved as 'test_output.mp4'")
                        except Exception as e:
                            print(f"Error saving video: {e}")
                else:
                    print("❌ No video data in response")
            else:
                print("❌ Request failed:")
                print(f"Error: {response_data.get('error', 'Unknown error')}")
                if 'returncode' in response_data:
                    print(f"Return code: {response_data['returncode']}")
                if 'stdout' in response_data:
                    print(f"STDOUT: {response_data['stdout']}")
                if 'stderr' in response_data:
                    print(f"STDERR: {response_data['stderr']}")
                if 'files_before' in response_data:
                    print(f"Files before: {response_data['files_before']}")
                if 'files_after' in response_data:
                    print(f"Files after: {response_data['files_after']}")
        else:
            print(f"❌ HTTP Error {response.status_code}")
            print(f"Response: {response.text}")

    except requests.exceptions.Timeout:
        print("❌ Request timed out after 60 seconds")
    except requests.exceptions.ConnectionError:
        print("❌ Connection error - is the server running?")
        print("Start the server with: ./runpod-test.sh")
    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    test_local_endpoint()