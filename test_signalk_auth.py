#!/usr/bin/env python3
"""
Test script to verify SignalK authentication workflow
This simulates what SensESP should be doing
"""

import requests
import json
import time
import sys

# Configuration - CHANGE THIS TO YOUR ESP32'S IP ADDRESS
ESP32_IP = "192.168.68.161"  # CHANGE THIS!
PORT = 3000

BASE_URL = f"http://{ESP32_IP}:{PORT}"

print("=" * 60)
print("SignalK Authentication Test")
print("=" * 60)
print(f"Testing server at: {BASE_URL}")
print()

# Step 1: Test discovery endpoint
print("Step 1: Testing discovery endpoint...")
print(f"GET {BASE_URL}/signalk")
try:
    response = requests.get(f"{BASE_URL}/signalk", timeout=5)
    print(f"Status: {response.status_code}")
    if response.status_code == 200:
        data = response.json()
        print("Response:")
        print(json.dumps(data, indent=2))

        # Extract WebSocket URL
        ws_url = data.get("endpoints", {}).get("v1", {}).get("signalk-ws", "")
        print(f"\nWebSocket URL: {ws_url}")
    else:
        print(f"ERROR: Expected 200, got {response.status_code}")
        sys.exit(1)
except Exception as e:
    print(f"ERROR: {e}")
    print("\nMake sure:")
    print("1. ESP32 is powered on")
    print("2. You're connected to the same network")
    print("3. The IP address in this script is correct")
    sys.exit(1)

print("\n" + "-" * 60 + "\n")

# Step 2: Request access
print("Step 2: Requesting access...")
access_request = {
    "clientId": "test-client-12345",
    "description": "Python test client",
    "permissions": "readwrite"
}

print(f"POST {BASE_URL}/signalk/v1/access/requests")
print("Body:", json.dumps(access_request, indent=2))

try:
    response = requests.post(
        f"{BASE_URL}/signalk/v1/access/requests",
        json=access_request,
        timeout=5
    )
    print(f"\nStatus: {response.status_code}")
    print("Response:")
    response_data = response.json()
    print(json.dumps(response_data, indent=2))

    if response.status_code != 202:
        print(f"\nERROR: Expected 202, got {response.status_code}")
        sys.exit(1)

    state = response_data.get("state")
    request_id = response_data.get("requestId")
    href = response_data.get("href", "")

    print(f"\nState: {state}")
    print(f"RequestId: {request_id}")

    if state == "PENDING":
        print("✓ Correct: State is PENDING")
    elif state == "COMPLETED":
        # Check if token is included
        if "accessRequest" in response_data:
            token = response_data["accessRequest"].get("token")
            print(f"✓ State is COMPLETED with token: {token}")
            print("\n" + "=" * 60)
            print("SUCCESS! Authentication completed immediately")
            print("=" * 60)
            sys.exit(0)
        else:
            print("ERROR: State is COMPLETED but no token included")
            sys.exit(1)

except Exception as e:
    print(f"ERROR: {e}")
    sys.exit(1)

print("\n" + "-" * 60 + "\n")

# Step 3: Poll for access approval
print("Step 3: Polling for access approval...")
poll_url = f"{BASE_URL}/signalk/v1/access/requests/{request_id}"
print(f"GET {poll_url}")

max_attempts = 5
for attempt in range(1, max_attempts + 1):
    print(f"\nAttempt {attempt}/{max_attempts}...")

    try:
        response = requests.get(poll_url, timeout=5)
        print(f"Status: {response.status_code}")

        if response.status_code == 200:
            data = response.json()
            print("Response:")
            print(json.dumps(data, indent=2))

            state = data.get("state")

            if state == "COMPLETED":
                access_request = data.get("accessRequest", {})
                permission = access_request.get("permission")
                token = access_request.get("token")

                if permission == "APPROVED" and token:
                    print("\n" + "=" * 60)
                    print("SUCCESS! Access granted")
                    print("=" * 60)
                    print(f"Token: {token}")
                    print(f"\nYou can now connect to WebSocket: {ws_url}")
                    print(f"Use token: {token}")
                    sys.exit(0)
                else:
                    print("ERROR: Completed but no token or not approved")
                    sys.exit(1)
            elif state == "PENDING":
                print("Still pending... waiting 2 seconds")
                time.sleep(2)
            else:
                print(f"ERROR: Unexpected state: {state}")
                sys.exit(1)
        else:
            print(f"ERROR: Expected 200, got {response.status_code}")
            sys.exit(1)

    except Exception as e:
        print(f"ERROR: {e}")
        sys.exit(1)

print("\n" + "=" * 60)
print("TIMEOUT: Access was not approved after polling")
print("=" * 60)
sys.exit(1)
