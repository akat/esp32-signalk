# Admin Password Management - User Guide

## Overview

Your ESP32 SignalK server has **full admin password management** already implemented!

## 🔐 Default Credentials

```
Username: admin
Password: 12345
```

⚠️ **SECURITY WARNING:** Change the default password immediately after first setup!

## 🌐 How to Change Password

### Method 1: Via Web Interface (Recommended)

1. **Connect to ESP32:**
   - WiFi: `ESP32-SignalK`
   - Password: `signalk123`

2. **Open Settings Page:**
   ```
   http://192.168.4.1:3000/settings
   ```

3. **Login with current credentials:**
   - Username: `admin`
   - Password: `12345` (or your current password)

4. **Fill in the form:**
   - **Current Password:** Enter old password
   - **New Password:** Enter new password (min 4 characters)
   - **Confirm Password:** Re-enter new password

5. **Click "Change Password"**

6. **Success!** You'll see:
   ```
   ✅ Password changed successfully. Please log in again.
   ```

7. **Re-login** with new credentials

---

### Method 2: Via API (for advanced users)

**Endpoint:** `POST /api/auth/change-password`

**Request:**
```json
{
  "oldPassword": "12345",
  "newPassword": "myNewSecurePassword123"
}
```

**Response (Success):**
```json
{
  "success": true,
  "message": "Password changed successfully"
}
```

**Response (Error):**
```json
{
  "error": "Invalid old password or new password too short"
}
```

**Example using curl:**
```bash
curl -X POST http://192.168.4.1:3000/api/auth/change-password \
  -H "Content-Type: application/json" \
  -d '{"oldPassword":"12345","newPassword":"newpass123"}'
```

---

## 🔒 Password Requirements

- **Minimum length:** 4 characters
- **Recommended:** 8+ characters with mixed case, numbers, symbols
- **Stored as:** SHA-256 hash (secure)
- **Flash storage:** Password persists across reboots

---

## 🛡️ Security Features

### 1. **Password Hashing**
Passwords are stored as SHA-256 hashes, never in plain text.

```cpp
// Default password hash for "12345":
5994471abb01112afcc18159f6cc74b4f511b99806da59b3caf5a9c173cacfc5
```

### 2. **Session Management**
- Sessions expire after 30 minutes of inactivity
- Changing password invalidates all existing sessions
- Forces re-login on all devices

### 3. **Flash Storage**
Password hash is stored in ESP32's Preferences (NVS flash).

**Storage location:** `web_auth` namespace
**Key:** `password_hash`

---

## 📖 Technical Details

### Files Involved

| File | Purpose |
|------|---------|
| `src/api/web_auth.h` | Authentication interface |
| `src/api/web_auth.cpp` | Password validation & session management |
| `src/api/routes.cpp` | `/api/auth/change-password` endpoint |
| `src/api/settings_html.h` | Password change UI |

### Password Change Flow

```
User submits form
    ↓
POST /api/auth/change-password
    ↓
Validate old password (SHA-256 hash check)
    ↓
Validate new password (min 4 chars)
    ↓
Hash new password (SHA-256)
    ↓
Save to flash (Preferences)
    ↓
Update in-memory hash
    ↓
Invalidate all sessions
    ↓
Return success
```

---

## 🚨 Troubleshooting

### "Invalid old password"
- **Problem:** Current password is incorrect
- **Solution:** Try default password `12345` or reset flash

### "New password too short"
- **Problem:** Password < 4 characters
- **Solution:** Use longer password

### "Passwords do not match"
- **Problem:** New password and confirmation don't match
- **Solution:** Re-type carefully

### Forgot Password?
**Option 1: Flash Reset**
```bash
# Erase flash and re-upload firmware
platformio run --target erase
platformio run --target upload
```

**Option 2: Code Reset**
Temporarily add this to `setup()` in `main.cpp`:
```cpp
void setup() {
  // ... existing code ...

  // Reset to default password (REMOVE AFTER FIRST BOOT!)
  Preferences prefs;
  prefs.begin("web_auth", false);
  prefs.remove("password_hash");
  prefs.end();

  // ... rest of setup ...
}
```

Then upload, login with `admin/12345`, change password, remove code, re-upload.

---

## 🔐 Best Practices

1. **Change default password immediately**
   - Default `12345` is insecure!

2. **Use strong password**
   - Min 8 characters
   - Mix uppercase, lowercase, numbers, symbols
   - Example: `MyBoat2025!`

3. **Don't share credentials**
   - Each user should have their own login (future feature)

4. **Regular updates**
   - Change password periodically
   - Especially if device was exposed to untrusted networks

5. **Monitor sessions**
   - Sessions auto-expire after 30 minutes
   - Manual logout available at `/logout`

---

## 📊 Session Management

### View Active Sessions

Currently, sessions are stored in memory only. To view active sessions, check serial output:

```
Web Auth: Login successful for user: admin
Web Auth: Created session: a1b2c3d4e5f6...
```

### Logout

**Manual logout:**
```
GET /logout
```

**All sessions invalidated when:**
- Password is changed
- Device reboots (sessions are in RAM)
- 30 minutes of inactivity

---

## 🎯 Quick Reference

| Action | URL | Method |
|--------|-----|--------|
| **Login page** | `/login` | GET |
| **Settings page** | `/settings` | GET (requires auth) |
| **Change password** | `/api/auth/change-password` | POST |
| **Logout** | `/logout` | GET |

**Default credentials:**
```
Username: admin
Password: 12345
```

**Change password form:**
```
http://192.168.4.1:3000/settings
```

---

## ✅ Verification

After changing password, verify it works:

1. Logout: `http://192.168.4.1:3000/logout`
2. Login again with NEW password
3. Check serial monitor:
   ```
   Web Auth: Login successful for user: admin
   ```

---

## 🆘 Support

If you have issues:

1. **Check serial monitor** for authentication logs
2. **Verify password requirements** (min 4 chars)
3. **Try default password** if recently flashed
4. **Reset flash** if completely stuck

---

**Last Updated:** January 2025
**Feature Status:** ✅ Fully Implemented
**Security:** SHA-256 hashed passwords, session-based auth
