#!/usr/bin/env python3

import requests
import json
import base64
import time

def test_async_endpoint():
    """Test the local RunPod endpoint with async polling"""

    base_url = "http://localhost:8000"
    run_url = f"{base_url}/run"

    # Test payload
    payload = {
        "input": {
            "test": True,
            "message": "Testing video generation"
        }
    }

    print("Testing RunPod video renderer endpoint (async)...")
    print(f"URL: {run_url}")
    print(f"Payload: {json.dumps(payload, indent=2)}")
    print()

    try:
        # Submit the job
        print("Submitting job...")
        start_time = time.time()

        response = requests.post(
            run_url,
            json=payload,
            headers={'Content-Type': 'application/json'},
            timeout=10
        )

        print(f"Job submission response: {response.status_code}")

        if response.status_code == 200:
            job_data = response.json()
            print(f"Job data: {json.dumps(job_data, indent=2)}")

            job_id = job_data.get("id")
            status = job_data.get("status")

            if not job_id:
                print("❌ No job ID returned")
                return

            print(f"Job ID: {job_id}")
            print(f"Initial status: {status}")

            # Poll for results
            status_url = f"{base_url}/status/{job_id}"
            print(f"Polling URL: {status_url}")

            max_polls = 30  # Poll for up to 30 seconds
            poll_interval = 1  # 1 second between polls

            for i in range(max_polls):
                print(f"Poll #{i+1}...", end=" ")

                try:
                    status_response = requests.get(status_url, timeout=5)

                    if status_response.status_code == 200:
                        result = status_response.json()
                        current_status = result.get("status", "unknown")
                        print(f"Status: {current_status}")

                        if current_status == "COMPLETED":
                            end_time = time.time()
                            duration = end_time - start_time
                            print(f"\n✅ Job completed in {duration:.2f} seconds!")

                            output = result.get("output", {})
                            print(f"Output: {json.dumps(output, indent=2)}")

                            if output.get("success"):
                                print("✅ Video generation successful!")
                                print(f"File size: {output.get('file_size', 'unknown')} bytes")

                                video_base64 = output.get("video")
                                if video_base64:
                                    print(f"Base64 length: {len(video_base64)} characters")

                                    # Optionally save the video file
                                    save_video = input("\nSave video to file? (y/n): ").lower().strip()
                                    if save_video == 'y':
                                        try:
                                            video_data = base64.b64decode(video_base64)
                                            with open('test_output_async.mp4', 'wb') as f:
                                                f.write(video_data)
                                            print("Video saved as 'test_output_async.mp4'")
                                        except Exception as e:
                                            print(f"Error saving video: {e}")
                            else:
                                print("❌ Video generation failed:")
                                print(f"Error: {output.get('error', 'Unknown error')}")

                            return

                        elif current_status == "FAILED":
                            print(f"\n❌ Job failed!")
                            output = result.get("output", {})
                            print(f"Error output: {json.dumps(output, indent=2)}")
                            return

                        elif current_status in ["IN_PROGRESS", "IN_QUEUE"]:
                            # Continue polling
                            time.sleep(poll_interval)
                            continue
                        else:
                            print(f"\n❓ Unknown status: {current_status}")
                            print(f"Full result: {json.dumps(result, indent=2)}")
                            time.sleep(poll_interval)

                    else:
                        print(f"Status check failed: {status_response.status_code}")
                        print(f"Response: {status_response.text}")
                        time.sleep(poll_interval)

                except requests.exceptions.Timeout:
                    print("Timeout on status check")
                    time.sleep(poll_interval)
                except Exception as e:
                    print(f"Error during status check: {e}")
                    time.sleep(poll_interval)

            print(f"\n❌ Job did not complete within {max_polls} seconds")

        else:
            print(f"❌ Job submission failed: {response.status_code}")
            print(f"Response: {response.text}")

    except requests.exceptions.Timeout:
        print("❌ Job submission timed out")
    except requests.exceptions.ConnectionError:
        print("❌ Connection error - is the server running?")
        print("Start the server with: ./runpod-test.sh")
    except Exception as e:
        print(f"❌ Error: {e}")

if __name__ == "__main__":
    test_async_endpoint()