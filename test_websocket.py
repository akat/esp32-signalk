#!/usr/bin/env python3
"""
Test WebSocket connection to SignalK server
"""

import asyncio
import websockets
import json

ESP32_IP = "192.168.68.201"
PORT = 3000
WS_URL = f"ws://{ESP32_IP}:{PORT}/signalk/v1/stream"

print("=" * 60)
print("WebSocket Connection Test")
print("=" * 60)
print(f"Connecting to: {WS_URL}")
print()

async def test_websocket():
    try:
        # Try connecting without authentication first
        print("Attempting connection (no auth)...")
        async with websockets.connect(WS_URL) as websocket:
            print("✓ Connected successfully!")

            # Wait for hello message
            print("\nWaiting for server hello message...")
            message = await asyncio.wait_for(websocket.recv(), timeout=5.0)
            print(f"Received: {message}")

            data = json.loads(message)
            if "self" in data:
                print(f"✓ Server hello received, vessel: {data.get('self')}")

            # Send subscription request
            print("\nSending subscription request...")
            subscribe_msg = {
                "context": "*",
                "subscribe": [
                    {"path": "*"}
                ]
            }
            await websocket.send(json.dumps(subscribe_msg))
            print("✓ Subscription sent")

            # Wait for data
            print("\nWaiting for data (5 seconds)...")
            try:
                for i in range(3):
                    message = await asyncio.wait_for(websocket.recv(), timeout=2.0)
                    data = json.loads(message)
                    if "updates" in data:
                        print(f"✓ Received delta update with {len(data['updates'])} updates")
                    else:
                        print(f"Received: {message[:100]}...")
            except asyncio.TimeoutError:
                print("No more data received")

            print("\n" + "=" * 60)
            print("SUCCESS! WebSocket connection working!")
            print("=" * 60)

    except websockets.exceptions.WebSocketException as e:
        print(f"✗ WebSocket error: {e}")
        return False
    except Exception as e:
        print(f"✗ Error: {e}")
        return False

    return True

# Run the test
try:
    result = asyncio.run(test_websocket())
    if not result:
        exit(1)
except KeyboardInterrupt:
    print("\nTest interrupted")
    exit(1)
