#!/usr/bin/env python3

import sys
import os

# Add the current directory to the path to import the handler
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# Now import and test the handler
from runpod_handler import handler, cleanup_process

def test_handler():
    """Test the RunPod handler locally"""

    # Simulate a RunPod event
    test_event = {
        'input': {
            'seconds': 0.5  # Short test video
        }
    }

    print("Testing RunPod handler with Unix socket...")
    print(f"Input: {test_event}")

    # Call the handler
    result = handler(test_event)

    # Check the result
    print(f"\nResult:")
    print(f"  Success: {result.get('success')}")
    print(f"  File size: {result.get('file_size', 'N/A')} bytes")
    print(f"  Encoding: {result.get('encoding', 'N/A')}")

    if result.get('video'):
        print(f"  Video data: {len(result['video'])} characters (base64)")
    else:
        print(f"  Error: {result.get('error', 'Unknown error')}")

    # Clean up
    cleanup_process()

    return result.get('success', False)

if __name__ == "__main__":
    success = test_handler()
    sys.exit(0 if success else 1)