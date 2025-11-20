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
    :root {
      --bg: #f5f7fb;
      --card-bg: #ffffff;
      --primary: #1f7afc;
      --text: #1f2a37;
      --muted: #5d6b82;
      --border: #e4e7ec;
    }

    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: var(--bg); color: var(--text); padding: 24px; line-height: 1.45; }
    .container { max-width: 1000px; margin: 0 auto; }

    .card { background: var(--card-bg); border-radius: 18px; padding: 24px; margin-bottom: 24px; box-shadow: 0 20px 45px rgba(15, 23, 42, 0.08); }
    .hero-card { background: linear-gradient(135deg, #1f7afc, #6c5ce7); color: #fff; border: none; }
    .hero-content { display: flex; justify-content: space-between; gap: 24px; align-items: flex-start; flex-wrap: wrap; }
    .hero-text .eyebrow { text-transform: uppercase; letter-spacing: 0.08em; font-size: 11px; opacity: 0.85; margin-bottom: 8px; }
    .hero-text h1 { font-size: 30px; margin-bottom: 8px; }
    .hero-text .subtitle { font-size: 15px; color: rgba(255,255,255,0.88); max-width: 520px; }
    .hero-actions { display: flex; gap: 12px; flex-wrap: wrap; }

    .btn-link { display: inline-flex; align-items: center; justify-content: center; padding: 10px 20px; border-radius: 999px; font-weight: 600; text-decoration: none; border: 1px solid rgba(255,255,255,0.5); transition: transform 0.2s ease, box-shadow 0.2s ease; }
    .btn-link.primary { background: #fff; color: #1f2a37; border-color: transparent; }
    .btn-link.secondary { color: #fff; }
    .btn-link:hover { transform: translateY(-1px); box-shadow: 0 8px 20px rgba(15, 23, 42, 0.25); }

    .card-header { display: flex; align-items: center; justify-content: space-between; gap: 12px; flex-wrap: wrap; margin-bottom: 12px; }
    .card-header span { color: var(--muted); font-size: 13px; }

    .table-wrapper { width: 100%; overflow-x: auto; border: 1px solid var(--border); border-radius: 14px; }
    table { width: 100%; border-collapse: collapse; min-width: 520px; }
    th, td { text-align: left; padding: 14px 16px; border-bottom: 1px solid var(--border); font-size: 14px; vertical-align: top; }
    th { text-transform: uppercase; font-size: 12px; letter-spacing: 0.05em; color: var(--muted); background: #f8fafc; }

    .btn { border: none; border-radius: 10px; padding: 8px 14px; font-size: 13px; font-weight: 600; cursor: pointer; transition: opacity 0.2s ease; color: #fff; }
    .btn-approve { background: #22c55e; }
    .btn-deny { background: #ef4444; }
    .btn-revoke { background: #f97316; }
    .btn:hover { opacity: 0.9; }

    .empty-state { text-align: center; padding: 28px; border: 1px dashed #d5d9eb; border-radius: 14px; color: var(--muted); background: #fff; }

    @media (max-width: 900px) {
      .hero-content { flex-direction: column; }
      .hero-actions { width: 100%; }
      .btn-link { flex: 1 1 auto; justify-content: center; }
    }

    @media (max-width: 720px) {
      body { padding: 18px; }
      .card { padding: 20px; }
      .hero-text h1 { font-size: 26px; }
      .table-wrapper { border: none; }
      table, thead, tbody, th, td, tr { display: block; width: 100%; }
      thead { display: none; }
      tr { background: #fff; border: 1px solid var(--border); border-radius: 14px; margin-bottom: 14px; padding: 12px; box-shadow: 0 8px 24px rgba(15, 23, 42, 0.06); }
      td { border: none; padding: 8px 0; display: flex; justify-content: space-between; gap: 12px; font-size: 13px; }
      td::before { content: attr(data-label); font-weight: 600; color: var(--muted); text-transform: uppercase; font-size: 11px; letter-spacing: 0.05em; }
      td:last-child { border-bottom: none; }
      .btn { width: 100%; justify-content: center; }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="card hero-card">
      <div class="hero-content">
        <div class="hero-text">
          <p class="eyebrow">SignalK Admin</p>
          <h1>Token Management</h1>
          <p class="subtitle">Approve, deny, or revoke SignalK access tokens for connected devices.</p>
        </div>
        <div class="hero-actions">
          <a href="/" class="btn-link primary">Dashboard</a>
          <a href="/admin" class="btn-link secondary">Admin Panel</a>
          <a href="/config" class="btn-link secondary">Network</a>
          <a href="/settings" class="btn-link secondary">Password</a>
          <a href="/hardware-settings" class="btn-link secondary">Hardware</a>
          <a href="/ap-settings" class="btn-link secondary">WiFi AP</a>
        </div>
      </div>
    </div>

    <div class="card">
      <div class="card-header">
        <div>
          <h2>Pending Requests</h2>
          <p class="muted">Authorize new devices requesting access.</p>
        </div>
        <span>Auto-refreshes every 5 seconds</span>
      </div>
      <div id="pending-requests"></div>
    </div>

    <div class="card">
      <div class="card-header">
        <div>
          <h2>Approved Tokens</h2>
          <p class="muted">Manage existing application tokens.</p>
        </div>
      </div>
      <div id="approved-tokens"></div>
    </div>
  </div>

  <script>
    function wrapTable(content) {
      return `<div class="table-wrapper"><table>${content}</table></div>`;
    }

    function renderEmpty(message) {
      return `<div class="empty-state">${message}</div>`;
    }

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
        el.innerHTML = renderEmpty('No pending requests');
        return;
      }

      const rows = requests.map(r => `
        <tr>
          <td data-label="Client ID"><strong>${r.clientId}</strong></td>
          <td data-label="Description">${r.description}</td>
          <td data-label="Permissions">${r.permissions}</td>
          <td data-label="Actions">
            <button class="btn btn-approve" onclick="approve('${r.requestId}')">Approve</button>
            <button class="btn btn-deny" onclick="deny('${r.requestId}')">Deny</button>
          </td>
        </tr>
      `).join('');

      el.innerHTML = wrapTable(`<thead><tr><th>Client ID</th><th>Description</th><th>Permissions</th><th>Actions</th></tr></thead><tbody>${rows}</tbody>`);
    }

    function renderApproved(tokens) {
      const el = document.getElementById('approved-tokens');
      if (!tokens || tokens.length === 0) {
        el.innerHTML = renderEmpty('No approved tokens');
        return;
      }

      const rows = tokens.map(t => `
        <tr>
          <td data-label="Client ID"><strong>${t.clientId}</strong></td>
          <td data-label="Description">${t.description}</td>
          <td data-label="Token"><code>${t.token}</code></td>
          <td data-label="Actions">
            <button class="btn btn-revoke" onclick="revoke('${t.token}')">Revoke</button>
          </td>
        </tr>
      `).join('');

      el.innerHTML = wrapTable(`<thead><tr><th>Client ID</th><th>Description</th><th>Token</th><th>Actions</th></tr></thead><tbody>${rows}</tbody>`);
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
    setInterval(loadData, 5000);
  </script>
</body>
</html>
)html";

#endif // ADMIN_HTML_H
