#ifndef ADMIN_HTML_H
#define ADMIN_HTML_H

const char* HTML_ADMIN = R"html(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Token Management - SignalK</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #f5f5f5; padding: 20px; }
    .container { max-width: 900px; margin: 0 auto; }
    .card { background: white; border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    h1 { color: #333; margin-bottom: 10px; font-size: 24px; }
    h2 { color: #666; margin-bottom: 15px; font-size: 18px; }
    table { width: 100%; border-collapse: collapse; }
    th, td { text-align: left; padding: 12px; border-bottom: 1px solid #eee; }
    th { background: #f8f8f8; font-weight: 600; }
    .btn { padding: 8px 16px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }
    .btn-approve { background: #4CAF50; color: white; }
    .btn-deny { background: #f44336; color: white; }
    .btn-revoke { background: #ff9800; color: white; }
    .btn:hover { opacity: 0.9; }
    .status { display: inline-block; padding: 4px 8px; border-radius: 4px; font-size: 12px; font-weight: 600; }
    .status-pending { background: #fff3cd; color: #856404; }
    .status-approved { background: #d4edda; color: #155724; }
    .empty { text-align: center; color: #999; padding: 20px; }
  </style>
</head>
<body>
  <div class="container">
    <div class="card">
      <h1>Token Management</h1>
      <p style="color: #666; margin-top: 10px;">Manage access tokens for SignalK clients</p>
    </div>

    <div class="card">
      <h2>Pending Requests</h2>
      <div id="pending-requests"></div>
    </div>

    <div class="card">
      <h2>Approved Tokens</h2>
      <div id="approved-tokens"></div>
    </div>
  </div>

  <script>
    function loadData() {
      fetch('/api/admin/tokens')
        .then(r => r.json())
        .then(data => {
          renderPending(data.pending);
          renderApproved(data.approved);
        });
    }

    function renderPending(requests) {
      const el = document.getElementById('pending-requests');
      if (!requests || requests.length === 0) {
        el.innerHTML = '<div class="empty">No pending requests</div>';
        return;
      }

      el.innerHTML = '<table><tr><th>Client ID</th><th>Description</th><th>Permissions</th><th>Actions</th></tr>' +
        requests.map(r => `
          <tr>
            <td><strong>${r.clientId}</strong></td>
            <td>${r.description}</td>
            <td>${r.permissions}</td>
            <td>
              <button class="btn btn-approve" onclick="approve('${r.requestId}')">Approve</button>
              <button class="btn btn-deny" onclick="deny('${r.requestId}')">Deny</button>
            </td>
          </tr>
        `).join('') + '</table>';
    }

    function renderApproved(tokens) {
      const el = document.getElementById('approved-tokens');
      if (!tokens || tokens.length === 0) {
        el.innerHTML = '<div class="empty">No approved tokens</div>';
        return;
      }

      el.innerHTML = '<table><tr><th>Client ID</th><th>Description</th><th>Token</th><th>Actions</th></tr>' +
        tokens.map(t => `
          <tr>
            <td><strong>${t.clientId}</strong></td>
            <td>${t.description}</td>
            <td><code style="font-size:11px">${t.token.substring(0,20)}...</code></td>
            <td>
              <button class="btn btn-revoke" onclick="revoke('${t.token}')">Revoke</button>
            </td>
          </tr>
        `).join('') + '</table>';
    }

    function approve(requestId) {
      if (!confirm('Approve this access request?')) return;
      fetch(`/api/admin/approve/${requestId}`, {method: 'POST'})
        .then(r => r.json())
        .then(() => { alert('Approved!'); loadData(); });
    }

    function deny(requestId) {
      if (!confirm('Deny this access request?')) return;
      fetch(`/api/admin/deny/${requestId}`, {method: 'POST'})
        .then(r => r.json())
        .then(() => { alert('Denied!'); loadData(); });
    }

    function revoke(token) {
      if (!confirm('Revoke this token? The client will lose access.')) return;
      fetch(`/api/admin/revoke/${token}`, {method: 'POST'})
        .then(r => r.json())
        .then(() => { alert('Token revoked!'); loadData(); });
    }

    loadData();
    setInterval(loadData, 5000); // Refresh every 5 seconds
  </script>
</body>
</html>
)html";

#endif // ADMIN_HTML_H
