#!/usr/bin/env python3
import subprocess
import json
import time
import sys

def test_multiple_requests():
    """Test multiple requests to the same video_renderer process."""

    print("Starting video_renderer daemon...")

    # Open a log file for debugging
    log_file = open('daemon_test.log', 'w')

    # Start the video_renderer process with pipes for stdin/stdout
    process = subprocess.Popen(
        ['./out/linux/video_renderer'],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,  # Redirect stderr to stdout to capture all output
        text=True,
        bufsize=1  # Line buffered
    )

    try:
        # Send multiple requests to the same process
        for i in range(1, 21):
            request = json.dumps({"seconds": 2.0})
            print(f"Request {i}/20: Sending {request}")

            # Send request to stdin
            process.stdin.write(request + '\n')
            process.stdin.flush()

            # Read response (the process outputs JSON response)
            response_started = False
            response_lines = []

            # Read until we get a complete JSON response
            while True:
                line = process.stdout.readline()
                if not line:
                    print(f"  ✗ Process died unexpectedly at request {i}")
                    # Check if process terminated
                    if process.poll() is not None:
                        print(f"Process exited with code: {process.returncode}")
                    return False

                # Log all output
                log_file.write(f"[Request {i}] {line}")
                log_file.flush()

                # Print Vulkan errors if they appear
                if "Vulkan error" in line:
                    print(f"  ❌ VULKAN ERROR: {line.strip()}")

                # Look for JSON response
                if '{' in line:
                    response_started = True

                if response_started:
                    response_lines.append(line.strip())
                    if '}' in line:
                        # We have a complete response
                        response_text = ''.join(response_lines)
                        try:
                            response = json.loads(response_text)
                            if response.get('success'):
                                print(f"  ✓ Request {i} completed successfully")
                            else:
                                print(f"  ✗ Request {i} failed: {response.get('error', 'Unknown error')}")
                                return False
                        except json.JSONDecodeError:
                            # Not a JSON line, continue
                            pass
                        break

            # Small delay between requests
            time.sleep(0.1)

        print("\n✅ All 20 requests completed successfully!")
        return True

    except Exception as e:
        print(f"\n❌ Test failed with exception: {e}")
        return False

    finally:
        # Clean up - terminate the process
        print("\nTerminating video_renderer process...")
        log_file.close()
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            print("Process didn't terminate, killing it...")
            process.kill()
            process.wait()
        print("Check daemon_test.log for full output")

if __name__ == "__main__":
    success = test_multiple_requests()
    sys.exit(0 if success else 1)