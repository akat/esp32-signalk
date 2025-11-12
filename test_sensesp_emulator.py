#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SensESP Emulator - Simulates SensESP connection behavior
Tests both initial connection and reconnection with cached token
"""

import asyncio
import websockets
import requests
import json
import sys
import os

# Fix Windows console encoding
if sys.platform == 'win32':
    os.system('chcp 65001 >nul 2>&1')

ESP32_IP = "172.20.10.2"  # CHANGE THIS TO YOUR ESP32 IP
PORT = 3000
BASE_URL = f"http://{ESP32_IP}:{PORT}"

# Simulate SensESP's persistent storage
cached_token = None

print("=" * 70)
print("SensESP Connection Emulator")
print("=" * 70)
print(f"Target: {BASE_URL}")
print()

def discover_server():
    """Step 1: Discovery endpoint - check for security configuration"""
    print("\n" + "=" * 70)
    print("STEP 1: DISCOVERY")
    print("=" * 70)
    print(f"GET {BASE_URL}/signalk")

    try:
        response = requests.get(f"{BASE_URL}/signalk", timeout=5)
        print(f"Status: {response.status_code}")

        if response.status_code == 200:
            data = response.json()
            print("\nDiscovery response:")
            print(json.dumps(data, indent=2))

            # Check if security is configured
            security = data.get("server", {}).get("security")
            ws_url = data.get("endpoints", {}).get("v1", {}).get("signalk-ws", "")

            print(f"\nWebSocket URL: {ws_url}")
            print(f"Security configured: {'YES' if security else 'NO'}")

            return ws_url, security
        else:
            print(f"ERROR: Expected 200, got {response.status_code}")
            return None, None

    except Exception as e:
        print(f"ERROR: {e}")
        return None, None


def request_access_token():
    """Step 2: Request access token (only if security is configured)"""
    print("\n" + "=" * 70)
    print("STEP 2: REQUEST ACCESS TOKEN")
    print("=" * 70)

    access_request = {
        "clientId": "python-sensesp-emulator",
        "description": "Python SensESP emulator",
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
        response_data = response.json()
        print("Response:")
        print(json.dumps(response_data, indent=2))

        if response.status_code == 202:
            request_id = response_data.get("requestId")
            state = response_data.get("state")

            if state == "COMPLETED" and "accessRequest" in response_data:
                token = response_data["accessRequest"].get("token")
                print(f"\n[OK] Token received immediately: {token}")
                return token
            elif state == "PENDING":
                print(f"\n[OK] Request pending, need to poll: {request_id}")
                return poll_for_token(request_id)

        return None

    except Exception as e:
        print(f"ERROR: {e}")
        return None


def poll_for_token(request_id):
    """Step 3: Poll for token approval"""
    print("\n" + "=" * 70)
    print("STEP 3: POLL FOR TOKEN")
    print("=" * 70)

    poll_url = f"{BASE_URL}/signalk/v1/access/requests/{request_id}"
    print(f"GET {poll_url}")

    max_attempts = 3
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
                    token = access_request.get("token")

                    if token:
                        print(f"\n[OK] Token received: {token}")
                        return token
                    else:
                        print("ERROR: Completed but no token")
                        return None
                elif state == "PENDING":
                    print("Still pending...")
                    if attempt < max_attempts:
                        asyncio.run(asyncio.sleep(1))
            else:
                print(f"ERROR: Expected 200, got {response.status_code}")
                return None

        except Exception as e:
            print(f"ERROR: {e}")
            return None

    print("\nTimeout: Token not approved")
    return None


async def connect_websocket(ws_url, use_auth=False):
    """Step 4: Connect to WebSocket (with or without auth)"""
    print("\n" + "=" * 70)
    print(f"STEP 4: WEBSOCKET CONNECTION {'WITH AUTH' if use_auth else 'WITHOUT AUTH'}")
    print("=" * 70)
    print(f"Connecting to: {ws_url}")

    if use_auth and cached_token:
        print(f"Using cached token: {cached_token}")
        headers = {"Authorization": f"Bearer {cached_token}"}
    else:
        print("No authentication headers")
        headers = {}

    try:
        # Try to connect
        print("\nAttempting connection...")
        # Use additional_headers parameter for older websockets library
        if headers:
            websocket = await websockets.connect(ws_url, additional_headers=headers)
        else:
            websocket = await websockets.connect(ws_url)

        try:
            print("[OK] Connected successfully!")

            # Wait for hello message
            print("\nWaiting for hello message...")
            message = await asyncio.wait_for(websocket.recv(), timeout=5.0)
            print(f"Received: {message}")

            data = json.loads(message)
            if "self" in data:
                print(f"[OK] Server hello received, vessel: {data.get('self')}")

            # Send subscription
            print("\nSending subscription...")
            subscribe_msg = {
                "context": "*",
                "subscribe": [{"path": "*"}]
            }
            await websocket.send(json.dumps(subscribe_msg))
            print("[OK] Subscription sent")

            # Wait for one delta update
            print("\nWaiting for data...")
            try:
                message = await asyncio.wait_for(websocket.recv(), timeout=3.0)
                data = json.loads(message)
                if "updates" in data:
                    print(f"[OK] Received delta update")
                else:
                    print(f"Received: {message[:100]}...")
            except asyncio.TimeoutError:
                print("(No data yet, but connection is working)")

            return True
        finally:
            await websocket.close()

    except Exception as e:
        print(f"[FAIL] Connection failed: {e}")
        return False


async def simulate_initial_connection():
    """Simulate SensESP initial connection (no cached token)"""
    global cached_token

    print("\n" + "#" * 70)
    print("# SCENARIO 1: INITIAL CONNECTION (like after hard reset)")
    print("#" * 70)
    print("Simulating: SensESP has NO cached token")

    # Step 1: Discovery
    ws_url, security = discover_server()
    if not ws_url:
        print("\n[FAIL] FAILED: Could not discover server")
        return False

    # Step 2: Request token (only if security is configured)
    if security:
        print("\nSecurity is configured - requesting token...")
        token = request_access_token()
        if not token:
            print("\n[FAIL] FAILED: Could not get access token")
            return False

        # Cache the token (like SensESP does in flash memory)
        cached_token = token
        print(f"\n[OK] Token cached: {cached_token}")
    else:
        print("\nNo security configured - proceeding without token")
        cached_token = None

    # Step 3: Connect WebSocket (without auth, even if we have token)
    # This simulates SensESP behavior on first connection
    success = await connect_websocket(ws_url, use_auth=False)

    if success:
        print("\n" + "=" * 70)
        print("[OK] SUCCESS: Initial connection works!")
        print("=" * 70)
        return True
    else:
        print("\n" + "=" * 70)
        print("[FAIL] FAILED: Initial connection failed")
        print("=" * 70)
        return False


async def simulate_reconnection():
    """Simulate SensESP reconnection (with cached token)"""
    print("\n\n" + "#" * 70)
    print("# SCENARIO 2: RECONNECTION (like after normal restart)")
    print("#" * 70)
    print(f"Simulating: SensESP HAS cached token: {cached_token}")

    # Step 1: Discovery
    ws_url, security = discover_server()
    if not ws_url:
        print("\n[FAIL] FAILED: Could not discover server")
        return False

    # Step 2: If security is configured AND we have cached token,
    # try to use it directly
    if security and cached_token:
        print("\nSecurity configured + cached token exists")
        print("=> SensESP will try to connect WITH Authorization header")
        success = await connect_websocket(ws_url, use_auth=True)
    else:
        print("\nNo security or no cached token")
        print("=> SensESP will connect without Authorization header")
        success = await connect_websocket(ws_url, use_auth=False)

    if success:
        print("\n" + "=" * 70)
        print("[OK] SUCCESS: Reconnection works!")
        print("=" * 70)
        return True
    else:
        print("\n" + "=" * 70)
        print("[FAIL] FAILED: Reconnection failed (THIS IS THE BUG!)")
        print("=" * 70)
        print("\nThis proves the AsyncWebSocket library bug:")
        print("- Connection WITH Authorization header fails")
        print("- Connection WITHOUT Authorization header works")
        return False


async def main():
    """Main test flow"""

    # Test 1: Initial connection
    success1 = await simulate_initial_connection()

    if not success1:
        print("\nStopping - initial connection failed")
        sys.exit(1)

    # Simulate waiting between connections
    print("\n\nWaiting 2 seconds before reconnection test...")
    await asyncio.sleep(2)

    # Test 2: Reconnection
    success2 = await simulate_reconnection()

    # Final summary
    print("\n\n" + "=" * 70)
    print("FINAL SUMMARY")
    print("=" * 70)
    print(f"Initial connection:  {'[OK] PASS' if success1 else '[FAIL] FAIL'}")
    print(f"Reconnection:        {'[OK] PASS' if success2 else '[FAIL] FAIL'}")
    print("=" * 70)

    if success1 and success2:
        print("\n[OK] ALL TESTS PASSED - SensESP should work perfectly!")
        print("  Both initial connection and reconnection work.")
        sys.exit(0)
    elif success1 and not success2:
        print("\n[WARN] RECONNECTION ISSUE DETECTED")
        print("  Initial connection works, but reconnection fails.")
        print("  This is the AsyncWebSocket library bug with Authorization headers.")
        sys.exit(1)
    else:
        print("\n[FAIL] CONNECTION FAILED")
        print("  Check if ESP32 is running and accessible.")
        sys.exit(1)


# Run the emulator
if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\nTest interrupted by user")
        sys.exit(1)
    except Exception as e:
        print(f"\n\nUnexpected error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
