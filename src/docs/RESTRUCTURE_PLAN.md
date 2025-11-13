# ESP32-SignalK Project Restructure Plan

## Current Status

### Completed
- ✅ Created new folder structure (ui/, api/, services/, utils/, hardware/, signalk/)
- ✅ Extracted HTML pages to ui/ folder:
  - `ui/dashboard.h` - Main dashboard UI
  - `ui/config.h` - TCP configuration page
  - `ui/admin.h` - Token management page
- ✅ Created utility modules in utils/:
  - `utils/uuid.h/.cpp` - UUID generation
  - `utils/time_utils.h/.cpp` - ISO8601 timestamps
  - `utils/conversions.h/.cpp` - Unit conversions & geospatial calculations
- ✅ Created initial signalk module:
  - `signalk/data_store.h` - Path operations header
- ✅ Backed up original main.cpp to main_original.cpp

### In Progress
- 🔄 Creating signalk module implementation
- 🔄 Creating services modules

### Pending
- ⏳ Complete signalk/data_store.cpp implementation
- ⏳ Create services/ modules:
  - `services/storage.h/.cpp` - NVS/Preferences persistence
  - `services/websocket.h/.cpp` - WebSocket handlers and broadcasting
  - `services/alarms.h/.cpp` - Geofence, depth, wind alarms
  - `services/expo_push.h/.cpp` - Expo push notifications
- ⏳ Create hardware/ modules:
  - `hardware/nmea0183.h/.cpp` - NMEA 0183 parsing functions
  - `hardware/nmea2000.h/.cpp` - NMEA 2000 (CAN) handlers
  - `hardware/sensors.h/.cpp` - I2C sensor reading (BME280)
  - `hardware/gps.h/.cpp` - GPS data handling
- ⏳ Create api/ modules:
  - `api/security.h/.cpp` - Token validation and auth
  - `api/access_requests.h/.cpp` - Access request management
  - `api/handlers.h/.cpp` - REST API endpoint handlers
  - `api/routes.h/.cpp` - Route setup and configuration
- ⏳ Refactor main.cpp to:
  - Include all new headers
  - Keep only global variables, setup(), and loop()
  - Move all functions to appropriate modules
- ⏳ Build test and fix compilation errors
- ⏳ Runtime test to verify functionality

## Project Structure

```
esp32-signalk/
├── src/
│   ├── main.cpp                    # Entry point (setup/loop only)
│   ├── main_original.cpp           # Backup of original monolithic file
│   ├── config.h                    # Pin definitions (existing)
│   ├── types.h                     # Data structures (existing)
│   │
│   ├── ui/                         # ✅ DONE
│   │   ├── dashboard.h
│   │   ├── config.h
│   │   └── admin.h
│   │
│   ├── utils/                      # ✅ DONE
│   │   ├── uuid.h / uuid.cpp
│   │   ├── time_utils.h / time_utils.cpp
│   │   └── conversions.h / conversions.cpp
│   │
│   ├── signalk/                    # 🔄 IN PROGRESS
│   │   ├── data_store.h            # ✅ DONE
│   │   └── data_store.cpp          # ⏳ TODO
│   │
│   ├── services/                   # ⏳ TODO
│   │   ├── storage.h / storage.cpp
│   │   ├── websocket.h / websocket.cpp
│   │   ├── alarms.h / alarms.cpp
│   │   └── expo_push.h / expo_push.cpp
│   │
│   ├── hardware/                   # ⏳ TODO
│   │   ├── nmea0183.h / nmea0183.cpp
│   │   ├── nmea2000.h / nmea2000.cpp
│   │   ├── sensors.h / sensors.cpp
│   │   └── gps.h / gps.cpp
│   │
│   └── api/                        # ⏳ TODO
│       ├── security.h / security.cpp
│       ├── access_requests.h / access_requests.cpp
│       ├── handlers.h / handlers.cpp
│       └── routes.h / routes.cpp
│
├── platformio.ini                  # Build configuration (existing)
└── RESTRUCTURE_PLAN.md             # This file
```

## Code Distribution (from main_original.cpp - 4098 lines, 144KB)

### Lines Distribution:
- Lines 1-37: Includes
- Lines 38-100: Configuration & Pin Definitions
- Lines 101-450: Global Variables & State
- Lines 451-810: Utility Functions (UUID, Time, Conversions, Path Ops, Notifications)
- Lines 811-1117: NMEA 0183 Parsing Functions
- Lines 1117-1197: WebSocket Broadcasting
- Lines 1197-1591: WebSocket Message Handlers
- Lines 1591-2286: REST API Discovery & Data Endpoints
- Lines 2287-2727: HTML Pages & Admin Handlers
- Lines 2728-3280: API Handlers (Token, Config, Expo)
- Lines 3281-3390: NMEA 2000 Handlers
- Lines 3391-3463: Sensor Functions
- Lines 3464-3700: Setup Function
- Lines 3701-4098: Loop Function & Support Code

## Benefits of New Structure

1. **Modularity**: Each module handles a specific concern
2. **Maintainability**: Easier to find and modify code
3. **Testability**: Modules can be tested independently
4. **Reusability**: Utils and services can be reused in other projects
5. **Collaboration**: Multiple developers can work on different modules
6. **Build Speed**: Better dependency management
7. **Code Review**: Smaller, focused files are easier to review

## Build Strategy

To ensure the project remains functional during restructuring:

1. Keep main_original.cpp as backup
2. Move code incrementally, one module at a time
3. Test compilation after each module creation
4. Test runtime functionality periodically
5. Once all modules are created and tested, finalize main.cpp

## Next Steps

1. Complete `signalk/data_store.cpp` implementation
2. Create `services/storage` module (token, config persistence)
3. Create `services/expo_push` module (push notifications)
4. Create `services/alarms` module (geofence, depth, wind)
5. Create `services/websocket` module (WS handling)
6. Create `hardware/nmea0183` module (NMEA sentence parsing)
7. Create `hardware/nmea2000` module (CAN bus handlers)
8. Create `hardware/sensors` module (BME280 I2C)
9. Create `api/security` module (authentication)
10. Create `api/handlers` module (REST endpoints)
11. Refactor main.cpp to use all modules
12. Build and test
13. Remove main_original.cpp once verified

## Estimated Effort

- Total modules to create: ~15 files (headers + implementations)
- Estimated time per module: 15-30 minutes
- Total estimated time: 4-6 hours
- Testing and bug fixes: 2-3 hours
- **Total: 6-9 hours of development work**
