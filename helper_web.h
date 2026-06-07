#ifndef HELPER_WEB_H
#define HELPER_WEB_H

/*
 * Web Interface for ENTSO-E Price Monitor
 * 
 * Provides a web dashboard with live prices, settings, and OTA update.
 * Accessible at http://pricemonitor.lan/ (or http://esp-ip/)
 * after initial configuration.
 * 
 * Pages:
 *   /           → Dashboard (price chart + status)
 *   /settings   → Edit WiFi, API key, bidding zone, timezone
 *   /update     → OTA firmware upload
 *   /reset      → Factory reset
 */

#include <Arduino.h>
#include <ESP8266WebServer.h>

#include "settings.h"
#include "helper_entsoe.h"

extern ESP8266WebServer server;
extern Config config;
extern struct entsoe_prices PRICES;
extern int getHoursOfDay();

bool checkWebAuth();

// Dashboard HTML page
const char dashboardHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ENTSO-E Price Monitor Dashboard</title>
  <link rel="icon" href="data:,">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; padding: 20px; min-height: 100vh; display: flex; align-items: center; justify-content: center; }
    .dashboard { max-width: 800px; width: 100%; }
    h1 { text-align: center; color: #00d4aa; font-size: 1.4em; margin-bottom: 4px; }
    .subtitle { text-align: center; color: #aaa; font-size: 0.85em; margin-bottom: 24px; }
    .card { background: #16213e; border: 1px solid #00d4aa33; border-radius: 12px; padding: 20px; margin-bottom: 16px; }
    .card h2 { font-size: 1em; color: #00d4aa; margin-bottom: 12px; }
    .bar-chart { display: flex; align-items: flex-end; gap: 6px; height: 200px; padding: 10px 0; }
    .bar-item { flex: 1; display: flex; flex-direction: column; align-items: center; }
    .bar { width: 100%; border-radius: 4px 4px 0 0; min-height: 4px; transition: height 0.3s; }
    .bar-label { font-size: 0.65em; color: #888; margin-top: 6px; text-align: center; }
    .bar-price { font-size: 0.7em; color: #aaa; margin-top: 2px; text-align: center; }
    .stats { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-top: 12px; }
    .stat { text-align: center; padding: 8px; background: #1a1a2e; border-radius: 8px; }
    .stat-value { font-size: 1.4em; font-weight: 700; color: #00d4aa; }
    .stat-label { font-size: 0.7em; color: #888; margin-top: 2px; }
    .status-bar { display: flex; gap: 8px; flex-wrap: wrap; align-items: center; justify-content: center; margin-bottom: 16px; }
    .status-badge { padding: 6px 14px; border-radius: 20px; font-size: 0.75em; background: #1a1a2e; border: 1px solid #333; }
    .status-badge.online { border-color: #00b894; color: #00b894; }
    .status-badge.offline { border-color: #d63031; color: #d63031; }
    .status-actions { display: flex; gap: 8px; margin-left: auto; }
    .status-settings { padding: 6px 10px; border-radius: 6px; background: #333; color: #eee; font-size: 0.75em; font-weight: 600; text-decoration: none; }
    .status-settings:hover { background: #444; }
    .update-alert { display: none; align-items: center; justify-content: space-between; gap: 12px; background: #fdcb6e; color: #1a1a2e; padding: 12px 14px; margin-bottom: 16px; border-radius: 8px; font-size: 0.8em; font-weight: 600; }
    .update-alert.visible { display: flex; }
    .update-alert a { color: #1a1a2e; background: #fff; padding: 7px 10px; border-radius: 6px; text-decoration: none; white-space: nowrap; }
    .nav { display: flex; gap: 8px; justify-content: center; flex-wrap: wrap; }
    .btn { padding: 10px 20px; border: none; border-radius: 8px; font-size: 0.85em; font-weight: 600; cursor: pointer; text-decoration: none; display: inline-block; }
    .btn-primary { background: #00d4aa; color: #1a1a2e; }
    .btn-primary:hover { background: #00b894; }
    .btn-secondary { background: #333; color: #eee; }
    .btn-secondary:hover { background: #444; }
    .btn-danger { background: #d63031; color: #fff; }
    .btn-danger:hover { background: #b71c1c; }
    .level-1 { background: #00b894; }
    .level-2 { background: #55c74d; }
    .level-3 { background: #fdcb6e; }
    .level-4 { background: #e17055; }
    .level-5 { background: #d63031; }
    .legend { display: grid; grid-template-columns: repeat(5, 1fr); gap: 6px; margin-top: 12px; }
    .legend-item { display: flex; align-items: center; justify-content: center; gap: 4px; font-size: 0.62em; color: #aaa; white-space: nowrap; }
    .legend-dot { width: 10px; height: 10px; border-radius: 2px; display: inline-block; flex: 0 0 auto; }
    .footer { text-align: center; font-size: 0.65em; color: #555; margin-top: 20px; }
    .refresh-note { text-align: center; font-size: 0.7em; color: #666; margin-top: 8px; }
    @media (max-width: 520px) { .legend { grid-template-columns: repeat(2, 1fr); } .status-actions { width: 100%; justify-content: center; margin-left: 0; } }
  </style>
</head>
<body>
  <div class="dashboard">
    <h1>⚡ ENTSO-E Price Monitor</h1>
    <p class="subtitle">Day-ahead electricity prices — next 8 hours</p>
    
    <div class="status-bar">
      <span class="status-badge online" id="wifiBadge">WiFi Connected</span>
      <span class="status-badge online" id="apiBadge">Prices Loaded</span>
      <span class="status-badge online" id="zoneBadge">Zone: NL</span>
      <span class="status-badge online" id="timeBadge">--:--</span>
      <div class="status-actions">
        <a href="/diagnostics" class="status-settings">🔎 Diagnostics</a>
        <a href="/settings" class="status-settings">⚙️ Settings</a>
      </div>
    </div>

    <div class="update-alert" id="updateAlert">
      <span id="updateMessage">A new firmware version is available.</span>
      <a href="/settings">Open Settings</a>
    </div>
    
    <div class="card">
      <h2>📊 Hourly Prices</h2>
      <div class="bar-chart" id="chart"></div>
      <div class="legend">
        <div class="legend-item"><span class="legend-dot level-1"></span>Very cheap · &lt; 0.005 euro</div>
        <div class="legend-item"><span class="legend-dot level-2"></span>Cheap · 0.005-0.050</div>
        <div class="legend-item"><span class="legend-dot level-3"></span>Normal · 0.050-0.100</div>
        <div class="legend-item"><span class="legend-dot level-4"></span>Expensive · 0.100-0.150</div>
        <div class="legend-item"><span class="legend-dot level-5"></span>Very expensive · &ge; 0.150 euro</div>
      </div>
    </div>
    
    <div class="card">
      <h2>📈 Summary</h2>
      <div class="stats" id="stats">
        <div class="stat"><div class="stat-value" id="minPrice">--</div><div class="stat-label">Min euro/kWh</div></div>
        <div class="stat"><div class="stat-value" id="maxPrice">--</div><div class="stat-label">Max euro/kWh</div></div>
        <div class="stat"><div class="stat-value" id="avgPrice">--</div><div class="stat-label">Avg euro/kWh</div></div>
        <div class="stat"><div class="stat-value" id="currentPrice">--</div><div class="stat-label">Current euro/kWh</div></div>
      </div>
    </div>

    <p class="refresh-note">Auto-refreshes every 60 seconds</p>
    <div class="footer">ENTSO-E Price Monitor <span id="firmwareVersion">--</span> · 8×8 LED Matrix</div>
  </div>
  
  <script>
    const levelColors = { 1: '#00b894', 2: '#55c74d', 3: '#fdcb6e', 4: '#e17055', 5: '#d63031' };
    const levelLabels = { 1: 'Very Cheap', 2: 'Cheap', 3: 'Normal', 4: 'Expensive', 5: 'Very Expensive' };
    let currentFirmwareVersion = '';
    const latestReleaseApi = 'https://api.github.com/repos/scmaxsr/ENTSOE_Price_Monitor_Led/releases/latest';

    function versionParts(version) {
      return version.replace(/^v/i, '').split('.').map(part => parseInt(part, 10) || 0);
    }

    function isNewerVersion(latest, current) {
      const latestParts = versionParts(latest);
      const currentParts = versionParts(current);
      const length = Math.max(latestParts.length, currentParts.length);
      for (let i = 0; i < length; i++) {
        const latestPart = latestParts[i] || 0;
        const currentPart = currentParts[i] || 0;
        if (latestPart > currentPart) return true;
        if (latestPart < currentPart) return false;
      }
      return false;
    }

    async function checkLatestVersion() {
      if (!currentFirmwareVersion) return;
      try {
        const res = await fetch(latestReleaseApi, { cache: 'no-store' });
        if (!res.ok) return;
        const release = await res.json();
        if (!release.tag_name || !isNewerVersion(release.tag_name, currentFirmwareVersion)) return;
        document.getElementById('updateMessage').textContent =
          'Firmware ' + release.tag_name + ' is available. Installed: ' + currentFirmwareVersion + '.';
        document.getElementById('updateAlert').classList.add('visible');
      } catch(err) {
        // Keep the dashboard quiet when GitHub is temporarily unavailable.
      }
    }
    
    async function loadPrices() {
      try {
        const res = await fetch('/api/prices');
        const data = await res.json();
        updateDashboard(data);
      } catch(err) {
        document.getElementById('chart').innerHTML = '<p style="color:#d63031;">Error loading prices</p>';
      }
    }

    function formatEuroFromCtKwh(value) {
      return (value / 100).toFixed(3) + ' euro';
    }

    function updateDashboard(data) {
      const chart = document.getElementById('chart');
      chart.innerHTML = '';
      
      let total = 0, count = 0;
      let maxH = 0;
      
      data.prices.forEach(p => {
        if (p.height > maxH) maxH = p.height;
      });
      
      data.prices.forEach((p, i) => {
        if (!p.null) {
          total += p.priceCtKwh;
          count++;
        }
        
        const div = document.createElement('div');
        div.className = 'bar-item';
        
        const priceSpan = document.createElement('div');
        priceSpan.className = 'bar-price';
        priceSpan.textContent = p.null ? '--' : formatEuroFromCtKwh(p.priceCtKwh);
        
        const bar = document.createElement('div');
        bar.className = 'bar level-' + p.level;
        const h = p.null ? 4 : Math.max(8, (p.height / 8) * 180);
        bar.style.height = h + 'px';
        
        if (i === 0) {
          bar.style.border = '2px solid #fff';
          bar.style.boxShadow = '0 0 8px rgba(0,212,170,0.5)';
        }
        
        const label = document.createElement('div');
        label.className = 'bar-label';
        label.textContent = p.label || '--:--';
        
        div.appendChild(priceSpan);
        div.appendChild(bar);
        div.appendChild(label);
        chart.appendChild(div);
      });
      
      const avg = count > 0 ? (total / count) : 0;
      
      document.getElementById('minPrice').textContent = formatEuroFromCtKwh(data.minCtKwh);
      document.getElementById('maxPrice').textContent = formatEuroFromCtKwh(data.maxCtKwh);
      document.getElementById('avgPrice').textContent = formatEuroFromCtKwh(avg);
      document.getElementById('currentPrice').textContent = formatEuroFromCtKwh(data.currentCtKwh);
      
      document.getElementById('zoneBadge').textContent = 'Zone: ' + data.zone;
      document.getElementById('timeBadge').textContent = data.time;
      currentFirmwareVersion = data.firmwareVersion || '';
      document.getElementById('firmwareVersion').textContent = currentFirmwareVersion || '--';
      checkLatestVersion();
    }
    
    // Auto-refresh every 60 seconds
    loadPrices();
    setInterval(loadPrices, 60000);
    setInterval(checkLatestVersion, 3600000);
  </script>
</body>
</html>
)rawliteral";

const char diagnosticsHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Diagnostics - ENTSO-E Price Monitor</title>
  <link rel="icon" href="data:,">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; padding: 20px; min-height: 100vh; display: flex; align-items: flex-start; justify-content: center; }
    .page { max-width: 980px; width: 100%; }
    .header { text-align: center; margin-bottom: 10px; }
    h1 { color: #00d4aa; font-size: 1.3em; }
    .management-actions { display: flex; justify-content: flex-end; gap: 8px; flex-wrap: wrap; margin: 0 0 16px; }
    .action-btn { display: flex; align-items: center; justify-content: center; height: 32px; padding: 0 11px; border: none; border-radius: 6px; background: #333; color: #eee; font-size: 0.78em; font-weight: 600; text-decoration: none; cursor: pointer; }
    .action-btn:hover { background: #444; }
    .diagnostics-grid { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 16px; align-items: stretch; }
    .card { background: #16213e; border: 1px solid #00d4aa33; border-radius: 12px; padding: 20px; min-width: 0; }
    .section-title { display: flex; align-items: center; gap: 8px; color: #00d4aa; font-size: 0.95em; font-weight: 700; margin-bottom: 4px; }
    .section-note { color: #888; font-size: 0.75em; line-height: 1.35; margin-bottom: 14px; }
    .diagnostic { display: flex; align-items: center; justify-content: space-between; gap: 12px; min-height: 38px; padding: 8px 0; border-bottom: 1px solid #ffffff0d; }
    .diagnostic:last-child { border-bottom: none; }
    .label { color: #aaa; font-size: 0.78em; }
    .value { color: #eee; font-size: 0.85em; font-weight: 700; text-align: right; overflow-wrap: anywhere; }
    .status { color: #888; font-size: 0.75em; text-align: center; margin-top: 14px; }
    .error { color: #ff7675; }
    @media (max-width: 760px) { .page { max-width: 520px; } .management-actions { justify-content: center; } .diagnostics-grid { grid-template-columns: 1fr; } }
  </style>
</head>
<body>
  <main class="page">
    <div class="header">
      <h1>🔎 Diagnostics</h1>
    </div>
    <div class="management-actions">
      <a href="/" class="action-btn">📊 Dashboard</a>
      <a href="/settings" class="action-btn">⚙️ Settings</a>
    </div>
    <div class="diagnostics-grid">
      <section class="card">
        <div class="section-title">📊 Price Data</div>
        <div class="section-note">Current source and availability of the rolling price window.</div>
        <div class="diagnostic"><div class="label">Data source</div><div class="value" id="source">--</div></div>
        <div class="diagnostic"><div class="label">Last success</div><div class="value" id="success">--</div></div>
        <div class="diagnostic"><div class="label">Data age</div><div class="value" id="age">--</div></div>
        <div class="diagnostic"><div class="label">Available price hours</div><div class="value" id="hours">--</div></div>
      </section>
      <section class="card">
        <div class="section-title">🌐 API Status</div>
        <div class="section-note">Latest response from the active electricity price endpoint.</div>
        <div class="diagnostic"><div class="label">HTTP status</div><div class="value" id="http">--</div></div>
        <div class="diagnostic"><div class="label">Firmware</div><div class="value" id="firmware">--</div></div>
      </section>
      <section class="card">
        <div class="section-title">🧠 System Memory</div>
        <div class="section-note">ESP8266 heap availability and fragmentation after the last update.</div>
        <div class="diagnostic"><div class="label">Free heap</div><div class="value" id="heap">--</div></div>
        <div class="diagnostic"><div class="label">Largest free block</div><div class="value" id="block">--</div></div>
        <div class="diagnostic"><div class="label">Heap fragmentation</div><div class="value" id="fragmentation">--</div></div>
      </section>
    </div>
    <div class="status" id="status">Auto-refreshes every 60 seconds</div>
  </main>
  <script>
    function formatAge(seconds) {
      if (seconds < 0) return 'Unknown';
      if (seconds < 60) return seconds + ' sec';
      if (seconds < 3600) return Math.floor(seconds / 60) + ' min';
      return Math.floor(seconds / 3600) + ' h ' + Math.floor((seconds % 3600) / 60) + ' min';
    }

    async function loadDiagnostics() {
      const status = document.getElementById('status');
      try {
        const response = await fetch('/api/prices');
        if (!response.ok) throw new Error('HTTP ' + response.status);
        const data = await response.json();
        document.getElementById('source').textContent = data.diagnosticSource || '--';
        document.getElementById('success').textContent = data.diagnosticLastSuccess || '--';
        document.getElementById('age').textContent = formatAge(data.diagnosticAgeSeconds);
        document.getElementById('http').textContent = data.diagnosticHttp;
        document.getElementById('hours').textContent = data.diagnosticHours;
        document.getElementById('heap').textContent = data.diagnosticFreeHeap + ' B';
        document.getElementById('block').textContent = data.diagnosticMaxBlock + ' B';
        document.getElementById('fragmentation').textContent = data.diagnosticFragmentation + '%';
        document.getElementById('firmware').textContent = data.firmwareVersion || '--';
        status.className = 'status';
        status.textContent = 'Auto-refreshes every 60 seconds';
      } catch (error) {
        status.className = 'status error';
        status.textContent = 'Unable to load diagnostics: ' + error.message;
      }
    }

    loadDiagnostics();
    setInterval(loadDiagnostics, 60000);
  </script>
</body>
</html>
)rawliteral";

// Settings HTML page (inline, simple form)
const char settingsHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Settings - ENTSO-E Price Monitor</title>
  <link rel="icon" href="data:,">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; padding: 20px; min-height: 100vh; display: flex; align-items: flex-start; justify-content: center; }
    .settings { max-width: 980px; width: 100%; }
    .settings-header { text-align: center; margin-bottom: 10px; }
    h1 { color: #00d4aa; font-size: 1.3em; }
    .settings-grid { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 16px; align-items: stretch; }
    .settings-grid .card { min-height: 230px; }
    .card { background: #16213e; border: 1px solid #00d4aa33; border-radius: 12px; padding: 20px; margin-bottom: 0; min-width: 0; }
    .section-title { display: flex; align-items: center; gap: 8px; color: #00d4aa; font-size: 0.95em; font-weight: 700; margin-bottom: 4px; }
    .section-note { color: #888; font-size: 0.75em; line-height: 1.35; margin-bottom: 12px; }
    label { display: block; margin-top: 14px; margin-bottom: 5px; font-weight: 600; font-size: 0.85em; color: #ccc; }
    input, select { width: 100%; padding: 10px 12px; border: 1px solid #333; border-radius: 8px; background: #1a1a2e; color: #eee; font-size: 0.9em; }
    input:focus, select:focus { outline: none; border-color: #00d4aa; }
    input[type=range] { padding: 0; accent-color: #00d4aa; }
    .info { font-size: 0.75em; color: #888; margin-top: 3px; }
    .field-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
    .range-row { display: grid; grid-template-columns: 1fr 52px; align-items: center; gap: 12px; margin-top: 8px; }
    .range-value { text-align: center; padding: 8px 6px; background: #1a1a2e; border: 1px solid #333; border-radius: 8px; color: #00d4aa; font-weight: 700; font-size: 0.85em; }
    .btn { width: 100%; padding: 12px; margin-top: 20px; background: #00d4aa; color: #1a1a2e; border: none; border-radius: 8px; font-size: 1em; font-weight: 700; cursor: pointer; }
    .btn:hover { background: #00b894; }
    .management-actions { display: flex; justify-content: flex-end; gap: 8px; flex-wrap: wrap; margin: 0 0 16px; }
    .action-btn { display: flex; align-items: center; justify-content: center; height: 32px; padding: 0 11px; border: none; border-radius: 6px; background: #333; color: #eee; font-size: 0.78em; font-weight: 600; text-decoration: none; cursor: pointer; }
    .action-btn:hover { background: #444; }
    .status { margin-top: 12px; padding: 10px; border-radius: 6px; font-size: 0.85em; display: none; }
    .status.success { display: block; background: #00b89422; border: 1px solid #00b894; color: #00d4aa; }
    .status.error { display: block; background: #d6303122; border: 1px solid #d63031; color: #ff7675; }
    .back { text-align: center; margin-top: 12px; }
    .back a { color: #00d4aa; font-size: 0.85em; text-decoration: none; }
    .back a:hover { text-decoration: underline; }
    .ssid-row { display: flex; gap: 8px; }
    .ssid-row select { flex: 1; }
    .ssid-row button { flex: 0 0 auto; padding: 10px 12px; border: 1px solid #333; border-radius: 8px; background: #1a1a2e; color: #eee; cursor: pointer; }
    .ssid-row button:hover { border-color: #00d4aa; }
    .ssid-row button:disabled { opacity: 0.5; cursor: not-allowed; }
    #manualSsidGroup { margin-top: 8px; padding-top: 8px; border-top: 1px dashed #333; }
    #manualSsidGroup label { margin-top: 0; font-size: 0.8em; color: #00d4aa; }
    @media (max-width: 760px) { .settings { max-width: 520px; } .management-actions { justify-content: center; } .settings-grid { grid-template-columns: 1fr; } .settings-grid .card { min-height: 0; } .field-grid { grid-template-columns: 1fr; } }
  </style>
</head>
<body>
  <div class="settings">
    <div class="settings-header">
      <h1>⚙️ Settings</h1>
    </div>
    <div class="management-actions">
      <a href="/" class="action-btn">📊 Dashboard</a>
      <button type="button" class="action-btn" id="otaBtn" onclick="installLatestOta()">📦 Install Latest Release</button>
      <a href="/reset" class="action-btn">🔁 Factory Reset</a>
    </div>
    <div class="status" id="otaStatus"></div>
    <form id="settingsForm">
      <div class="settings-grid">
        <div class="card">
          <div class="section-title">📶 Network</div>
          <div class="section-note">WiFi connection used by the monitor after restart.</div>
          <label>WiFi SSID</label>
          <div class="ssid-row">
            <select id="ssid" required>
              <option value="">-- Scanning networks... --</option>
            </select>
            <button type="button" id="scanBtn" onclick="scanNetworks()">Scan</button>
          </div>
          <div class="info" id="scanInfo">Scanning for WiFi networks...</div>
          <div id="manualSsidGroup" style="display:none;">
            <label>Manual SSID</label>
            <input type="text" id="manualSsid" placeholder="Type network name manually">
          </div>
          
          <label>WiFi Password</label>
          <input type="password" id="password" placeholder="Leave empty to keep current password">
          <div class="info">Enter a new WiFi password only if you want to change it.</div>
        </div>

        <div class="card">
          <div class="section-title">⚡ ENTSO-E Data</div>
          <div class="section-note">Market area and API settings for day-ahead price data.</div>
          <label>ENTSO-E API Key</label>
          <input type="text" id="apiKey" placeholder="Leave empty to keep current key">
          <div class="info" id="apiKeyInfo">Enter a new key only if you want to change it.</div>

          <div class="field-grid">
            <div>
              <label>Bidding Zone (EIC Code)</label>
              <input type="text" id="biddingZone" placeholder="e.g. 10YNL----------L">
              <div class="info">NL: 10YNL----------L · BE: 10YBE----------2</div>
            </div>
            <div>
              <label>Timezone (POSIX)</label>
              <input type="text" id="timezone" placeholder="e.g. CET-1CEST,M3.5.0,M10.5.0/3">
              <div class="info">Amsterdam: CET-1CEST,M3.5.0,M10.5.0/3</div>
            </div>
          </div>
        </div>

        <div class="card">
          <div class="section-title">🔐 Web Access</div>
          <div class="section-note">Login used to protect the dashboard, settings, API and OTA pages.</div>
          <div class="field-grid">
            <div>
              <label>Web Username</label>
              <input type="text" id="webUser" placeholder="Username for this web interface">
            </div>
            <div>
              <label>Web Password</label>
              <input type="password" id="webPass" placeholder="Leave empty to keep current password">
              <div class="info" id="webPassInfo">Used to protect the STA-mode web interface.</div>
            </div>
          </div>
        </div>

        <div class="card">
          <div class="section-title">💡 Display</div>
          <div class="section-note">Matrix brightness. Lower values reduce glare and power use.</div>
          <label>Matrix Brightness</label>
          <div class="range-row">
            <input type="range" id="ledBrightness" min="1" max="100" step="1" value="20">
            <div class="range-value"><span id="brightnessValue">20</span>%</div>
          </div>
        </div>
      </div>

      <button type="submit" class="btn">Save Settings & Restart</button>
      <div class="status" id="status"></div>
    </form>
  </div>
  <script>
    const form = document.getElementById('settingsForm');
    const status = document.getElementById('status');
    const ssidSelect = document.getElementById('ssid');
    const scanInfo = document.getElementById('scanInfo');
    const scanBtn = document.getElementById('scanBtn');
    const manualSsidGroup = document.getElementById('manualSsidGroup');
    const manualSsidInput = document.getElementById('manualSsid');
    const ledBrightness = document.getElementById('ledBrightness');
    const brightnessValue = document.getElementById('brightnessValue');
    let currentSsid = '';

    function setSelectedSsid(value) {
      if (!value) return;
      let found = false;
      Array.from(ssidSelect.options).forEach(opt => {
        if (opt.value === value) found = true;
      });
      if (!found) {
        const opt = document.createElement('option');
        opt.value = value;
        opt.textContent = value + ' (saved)';
        ssidSelect.insertBefore(opt, ssidSelect.firstChild);
      }
      ssidSelect.value = value;
      scanInfo.textContent = 'Selected: ' + value;
    }

    async function scanNetworks(refresh = true) {
      scanInfo.textContent = 'Scanning for WiFi networks...';
      scanBtn.disabled = true;
      ssidSelect.innerHTML = '<option value="">-- Scanning networks... --</option>';
      try {
        const res = await fetch(refresh ? '/scan?refresh=1' : '/scan');
        const text = await res.text();
        if (!res.ok) throw new Error(text || 'Scan failed');
        if (text === 'scanning') {
          setTimeout(() => scanNetworks(false), 1000);
          return;
        }
        const networks = JSON.parse(text);
        ssidSelect.innerHTML = '<option value="">-- Select a network --</option>';
        networks.sort((a,b)=>b.rssi-a.rssi).forEach(net => {
          const opt = document.createElement('option');
          opt.value = net.ssid;
          opt.textContent = `${net.ssid} (${net.rssi} dBm)`;
          ssidSelect.appendChild(opt);
        });
        const otherOpt = document.createElement('option');
        otherOpt.value = '__manual__';
        otherOpt.textContent = 'Other (type manually)';
        ssidSelect.appendChild(otherOpt);
        if (currentSsid) {
          setSelectedSsid(currentSsid);
        } else {
          scanInfo.textContent = networks.length === 0 ? 'No networks found yet. Try again.' : `Found ${networks.length} networks.`;
        }
      } catch(e) {
        scanInfo.textContent = 'Error scanning: ' + e.message;
        ssidSelect.innerHTML = '<option value="__manual__">Other (type manually)</option>';
        ssidSelect.value = '__manual__';
        manualSsidGroup.style.display = 'block';
        manualSsidInput.required = true;
      } finally {
        scanBtn.disabled = false;
      }
    }

    ssidSelect.addEventListener('change', () => {
      if (ssidSelect.value === '__manual__') {
        manualSsidGroup.style.display = 'block';
        manualSsidInput.required = true;
        scanInfo.textContent = 'Type your network name manually below.';
      } else {
        manualSsidGroup.style.display = 'none';
        manualSsidInput.required = false;
        scanInfo.textContent = ssidSelect.value ? 'Selected: ' + ssidSelect.value : 'Select a network from the list.';
      }
    });

    ledBrightness.addEventListener('input', () => {
      brightnessValue.textContent = ledBrightness.value;
    });

    async function installLatestOta() {
      if (!confirm('Download and install the latest firmware release from GitHub?')) return;
      const btn = document.getElementById('otaBtn');
      const otaStatus = document.getElementById('otaStatus');
      btn.disabled = true;
      btn.textContent = 'Downloading update...';
      otaStatus.className = 'status success';
      otaStatus.textContent = 'The device is downloading the latest release. Keep it powered on.';
      try {
        const res = await fetch('/api/ota/latest', { method: 'POST' });
        const data = await res.json();
        if (!res.ok) throw new Error(data.message || 'OTA update could not start');
        otaStatus.textContent = data.message;
      } catch(e) {
        otaStatus.className = 'status error';
        otaStatus.textContent = 'Error: ' + e.message;
        btn.disabled = false;
        btn.textContent = '📦 Install Latest Release';
      }
    }
    
    // Load current settings
    async function loadSettings() {
      try {
        const res = await fetch('/api/config');
        const data = await res.json();
        currentSsid = data.ssid || '';
        document.getElementById('apiKey').value = '';
        document.getElementById('apiKeyInfo').textContent = data.hasApiKey ? 'Current key: ' + data.apiKey : 'No API key saved yet.';
        document.getElementById('webUser').value = data.webUser || '';
        document.getElementById('webPass').value = '';
        document.getElementById('webPassInfo').textContent = data.hasWebPassword ? 'Current password is saved. Leave empty to keep it.' : 'No web password saved yet.';
        document.getElementById('biddingZone').value = data.biddingZone || '';
        document.getElementById('timezone').value = data.timezone || '';
        ledBrightness.value = data.ledBrightness || 20;
        brightnessValue.textContent = ledBrightness.value;
        await scanNetworks();
      } catch(e) {
        status.className = 'status error';
        status.textContent = 'Error: ' + e.message;
      }
    }
    
    form.addEventListener('submit', async (e) => {
      e.preventDefault();
      const btn = form.querySelector('.btn');
      btn.disabled = true;
      btn.textContent = 'Saving...';
      status.className = 'status';
      status.textContent = '';

      let finalSsid = ssidSelect.value;
      if (finalSsid === '__manual__') {
        finalSsid = manualSsidInput.value;
      }
      
      const data = new URLSearchParams();
      data.append('ssid', finalSsid);
      data.append('password', document.getElementById('password').value);
      data.append('apiKey', document.getElementById('apiKey').value);
      data.append('webUser', document.getElementById('webUser').value);
      data.append('webPass', document.getElementById('webPass').value);
      data.append('biddingZone', document.getElementById('biddingZone').value);
      data.append('timezone', document.getElementById('timezone').value);
      data.append('ledBrightness', ledBrightness.value);
      
      try {
        const res = await fetch('/api/config', { method: 'POST', body: data });
        const text = await res.text();
        if (res.ok) {
          status.className = 'status success';
          status.textContent = '✅ ' + text;
          setTimeout(() => { window.location.href = '/'; }, 3000);
        } else {
          status.className = 'status error';
          status.textContent = '❌ ' + text;
          btn.disabled = false;
          btn.textContent = 'Save Settings & Restart';
        }
      } catch(e) {
        status.className = 'status error';
        status.textContent = '❌ ' + e.message;
        btn.disabled = false;
        btn.textContent = 'Save Settings & Restart';
      }
    });
    
    loadSettings();
  </script>
</body>
</html>
)rawliteral";

// Reset confirmation page
const char resetHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Reset - ENTSO-E Price Monitor</title>
  <link rel="icon" href="data:,">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; padding: 20px; min-height: 100vh; display: flex; align-items: center; justify-content: center; }
    .reset-box { max-width: 400px; width: 100%; text-align: center; }
    h1 { color: #d63031; font-size: 1.3em; margin-bottom: 16px; }
    .card { background: #16213e; border: 1px solid #d6303144; border-radius: 12px; padding: 24px; }
    p { font-size: 0.9em; color: #bbb; margin-bottom: 20px; line-height: 1.5; }
    .btn { padding: 12px 24px; border: none; border-radius: 8px; font-size: 0.95em; font-weight: 700; cursor: pointer; margin: 4px; }
    .btn-danger { background: #d63031; color: #fff; }
    .btn-danger:hover { background: #b71c1c; }
    .btn-secondary { background: #333; color: #eee; }
    .btn-secondary:hover { background: #444; }
    .back { margin-top: 16px; }
    .back a { color: #00d4aa; font-size: 0.85em; text-decoration: none; }
  </style>
</head>
<body>
  <div class="reset-box">
    <h1>🔁 Factory Reset</h1>
    <div class="card">
      <p>This will erase all configuration (WiFi, API key, bidding zone, timezone) and restart the device. The config portal will appear on next boot.</p>
      <button class="btn btn-danger" onclick="doReset()">Yes, Reset Everything</button>
      <a href="/" class="btn btn-secondary" style="text-decoration:none;display:inline-block;">Cancel</a>
      <div id="resetStatus" style="margin-top:12px;font-size:0.85em;color:#888;"></div>
    </div>
    <div class="back"><a href="/">&larr; Back to Dashboard</a></div>
  </div>
  <script>
    async function doReset() {
      const btn = event.target;
      btn.disabled = true;
      btn.textContent = 'Resetting...';
      document.getElementById('resetStatus').textContent = 'Clearing configuration...';
      try {
        const res = await fetch('/api/reset', { method: 'POST' });
        const text = await res.text();
        document.getElementById('resetStatus').textContent = '✅ ' + text;
        setTimeout(() => { window.location.href = '/'; }, 3000);
      } catch(e) {
        document.getElementById('resetStatus').textContent = '❌ Error: ' + e.message;
        btn.disabled = false;
        btn.textContent = 'Yes, Reset Everything';
      }
    }
  </script>
</body>
</html>
)rawliteral";

// Prototypes for web API implementations (in helper_web.cpp)
void handleApiPrices();
void handleApiConfig();
void handleApiSaveConfig();
void handleApiReset();
void handleApiLatestOta();
void initWebInterface();

#endif // HELPER_WEB_H
