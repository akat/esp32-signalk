#ifndef SETTINGS_HTML_H
#define SETTINGS_HTML_H

const char SETTINGS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Settings - ESP32 SignalK</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: #f5f5f5;
      padding: 20px;
    }
    .container {
      max-width: 600px;
      margin: 0 auto;
      background: white;
      border-radius: 10px;
      box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
      padding: 30px;
    }
    h1 {
      color: #333;
      margin-bottom: 10px;
      font-size: 28px;
    }
    .nav {
      margin-bottom: 30px;
      padding-bottom: 20px;
      border-bottom: 2px solid #f0f0f0;
    }
    .nav a {
      color: #667eea;
      text-decoration: none;
      margin-right: 20px;
      font-weight: 500;
    }
    h2 {
      color: #333;
      font-size: 20px;
      margin-bottom: 15px;
    }
    .form-group {
      margin-bottom: 20px;
    }
    label {
      display: block;
      color: #333;
      margin-bottom: 8px;
      font-weight: 500;
      font-size: 14px;
    }
    input[type="password"] {
      width: 100%;
      padding: 12px 15px;
      border: 2px solid #e0e0e0;
      border-radius: 6px;
      font-size: 16px;
    }
    input[type="password"]:focus {
      outline: none;
      border-color: #667eea;
    }
    button {
      padding: 12px 24px;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      color: white;
      border: none;
      border-radius: 6px;
      font-size: 16px;
      cursor: pointer;
    }
    .logout-btn {
      background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
      margin-left: 10px;
    }
    .message {
      padding: 12px;
      border-radius: 6px;
      margin-bottom: 20px;
      font-size: 14px;
      display: none;
    }
    .message.success {
      background: #d4edda;
      color: #155724;
      display: block;
    }
    .message.error {
      background: #f8d7da;
      color: #721c24;
      display: block;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Settings</h1>

    <div class="nav">
      <a href="/">Dashboard</a>
      <a href="/admin">Admin</a>
      <a href="/config">TCP Config</a>
      <a href="/settings">Settings</a>
    </div>

    <div id="message" class="message"></div>

    <h2>Change Password</h2>
    <form id="passwordForm">
      <div class="form-group">
        <label for="oldPassword">Current Password</label>
        <input type="password" id="oldPassword" required autofocus>
      </div>

      <div class="form-group">
        <label for="newPassword">New Password (min 4 chars)</label>
        <input type="password" id="newPassword" required>
      </div>

      <div class="form-group">
        <label for="confirmPassword">Confirm New Password</label>
        <input type="password" id="confirmPassword" required>
      </div>

      <button type="submit">Change Password</button>
      <button type="button" class="logout-btn" onclick="logout()">Logout</button>
    </form>
  </div>

  <script>
    function showMessage(text, type) {
      const msg = document.getElementById('message');
      msg.textContent = text;
      msg.className = 'message ' + type;
    }

    document.getElementById('passwordForm').addEventListener('submit', async (e) => {
      e.preventDefault();

      const oldPassword = document.getElementById('oldPassword').value;
      const newPassword = document.getElementById('newPassword').value;
      const confirmPassword = document.getElementById('confirmPassword').value;

      if (newPassword !== confirmPassword) {
        showMessage('Passwords do not match', 'error');
        return;
      }

      if (newPassword.length < 4) {
        showMessage('Password must be at least 4 characters', 'error');
        return;
      }

      try {
        const response = await fetch('/api/auth/change-password', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ oldPassword, newPassword })
        });

        const data = await response.json();

        if (response.ok) {
          showMessage('Password changed successfully! Logging out...', 'success');
          document.getElementById('passwordForm').reset();
          setTimeout(() => window.location.href = '/login.html', 2000);
        } else {
          showMessage(data.error || 'Failed to change password', 'error');
        }
      } catch (err) {
        showMessage('Network error. Please try again.', 'error');
      }
    });

    async function logout() {
      try {
        await fetch('/api/auth/logout', { method: 'POST' });
      } catch (err) {
        console.error('Logout error:', err);
      }
      window.location.href = '/login.html';
    }
  </script>
</body>
</html>
)rawliteral";

#endif // SETTINGS_HTML_H
