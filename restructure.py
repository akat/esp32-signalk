#!/usr/bin/env python3
"""
Script to restructure the ESP32-SignalK project by splitting main.cpp
into organized modules.
"""

import os
import re
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent
SRC_DIR = PROJECT_ROOT / "src"
MAIN_CPP = SRC_DIR / "main.cpp"

# Read the main.cpp file
with open(MAIN_CPP, 'r', encoding='utf-8') as f:
    content = f.read()
    lines = content.splitlines()

print(f"Total lines in main.cpp: {len(lines)}")
print(f"File size: {len(content)} bytes")

# Extract HTML sections (already done manually, but we can verify)
html_sections = {
    'HTML_UI': (2287, 2435),
    'HTML_CONFIG': (2442, 2593),
    'HTML_ADMIN': (2600, 2723)
}

# Extract function ranges (approximate based on analysis)
utils_functions = {
    'uuid': (451, 457),
    'time': (459, 467),
    'conversions': (469, 485),
}

# Path operations
path_operations = (493, 585)

# Notifications
notifications_range = (588, 610)

# Alarms
alarms_range = (612, 750)

# Token management
token_management = (211, 380)

# NMEA parsing
nmea_parsing = (811, 1117)

# WebSocket
websocket_range = (1117, 1591)

# API handlers
api_handlers = (1591, 3400)

# NMEA2000
nmea2000_range = (3281, 3390)

# Sensors
sensors_range = (3411, 3463)

# Setup and loop
setup_loop = (3464, 4098)

print("\nExtracting code sections...")
print(f"Path operations: lines {path_operations[0]}-{path_operations[1]}")
print(f"NMEA parsing: lines {nmea_parsing[0]}-{nmea_parsing[1]}")
print(f"WebSocket: lines {websocket_range[0]}-{websocket_range[1]}")
print(f"API handlers: lines {api_handlers[0]}-{api_handlers[1]}")
print(f"NMEA2000: lines {nmea2000_range[0]}-{nmea2000_range[1]}")
print(f"Sensors: lines {sensors_range[0]}-{sensors_range[1]}")
print(f"Setup/Loop: lines {setup_loop[0]}-{setup_loop[1]}")

print("\n✓ Analysis complete!")
print("\nRecommended next steps:")
print("1. Continue manual extraction of large sections (NMEA, API, WebSocket)")
print("2. Create service modules for storage, alarms, expo push")
print("3. Create hardware modules for NMEA parsers and sensors")
print("4. Create API modules for all endpoint handlers")
print("5. Refactor main.cpp to only contain setup() and loop()")
