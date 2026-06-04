/*
 * WiFi Configuration Portal for ENTSOE Price Monitor
 * 
 * On first boot (or when no saved config is found), this creates a WiFi access point
 * with a captive portal where the user can configure:
 *   - WiFi SSID + Password
 *   - ENTSO-E API Key
 *   - Bidding Zone
 * 
 * Configuration is saved to SPIFFS and loaded on subsequent boots.
 */

#ifndef HELPER_WIFI_PORTAL_H
#define HELPER_WIFI_PORTAL_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include "helper_led.h"
#include "settings.h"

// These globals are defined in the main sketch; declare them here.
extern DNSServer dnsServer;
extern ESP8266WebServer server;
extern Config config;

// Forward declarations
void startWiFiScan();
#define DNS_PORT 53
#include "helper_config.h"

// Note: SPIFFS must be mounted (SPIFFS.begin()) in setup().

// Note: config load/save are implemented in helper_config.cpp

// Start WiFi scan (non-blocking, results fetched later via /scan)
void startWiFiScan();

// HTML page for the configuration portal
const char configHTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ENTSO-E Price Monitor Configuration</title>
  <link rel="icon" href="data:,">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; min-height: 100vh; display: flex; align-items: center; justify-content: center; padding: 20px; }
    .container { max-width: 960px; width: 100%; }
    .layout { display: flex; gap: 24px; flex-wrap: wrap; justify-content: center; }
    .col-form { flex: 1 1 380px; max-width: 480px; min-width: 300px; display: flex; flex-direction: column; }
    .col-help { flex: 1 1 380px; max-width: 480px; min-width: 300px; display: flex; flex-direction: column; }
    .col-form .card, .col-help .card { flex: 1; display: flex; flex-direction: column; }
    .help-panel { flex: 1; background: #16213e; border: 1px solid #00d4aa33; border-radius: 10px; padding: 20px; font-size: 0.78em; color: #bbb; line-height: 1.7; }
    .help-panel h2 { font-size: 1.1em; color: #00d4aa; margin-bottom: 14px; }
    .help-panel code { background: #00d4aa22; padding: 1px 5px; border-radius: 3px; color: #00d4aa; font-size: 0.92em; }
    .help-panel hr { border: none; border-top: 1px solid #333; margin: 12px 0; }
    .help-panel .tip { margin-bottom: 10px; }
    .help-panel .tip-icon { color: #00d4aa; margin-right: 4px; }
    .help-badge { display: inline-block; background: #00d4aa22; color: #00d4aa; font-size: 0.7em; padding: 2px 8px; border-radius: 10px; margin-top: 10px; }
    h1 { font-size: 1.4em; margin-bottom: 4px; color: #00d4aa; text-align: center; }
    .subtitle { font-size: 0.85em; color: #aaa; margin-bottom: 20px; text-align: center; }
    label { display: block; margin-top: 14px; margin-bottom: 5px; font-weight: 600; font-size: 0.85em; color: #ccc; }
    input { width: 100%; padding: 11px 14px; border: 1px solid #333; border-radius: 8px; background: #16213e; color: #eee; font-size: 0.95em; transition: border 0.2s; }
    input:focus { outline: none; border-color: #00d4aa; }
    input::placeholder { color: #555; }
    .info { font-size: 0.75em; color: #888; margin-top: 3px; }
    .btn { width: 100%; padding: 12px; margin-top: 22px; background: #00d4aa; color: #1a1a2e; border: none; border-radius: 8px; font-size: 1em; font-weight: 700; cursor: pointer; transition: background 0.2s; }
    .btn:hover { background: #00b894; }
    .btn:disabled { opacity: 0.5; cursor: not-allowed; }
    .status { margin-top: 15px; padding: 10px; border-radius: 6px; font-size: 0.85em; display: none; }
    .status.success { display: block; background: #00b89422; border: 1px solid #00b894; color: #00d4aa; }
    .status.error { display: block; background: #d6303122; border: 1px solid #d63031; color: #ff7675; }
    .footer { text-align: center; font-size: 0.7em; color: #555; border-top: 1px solid #2a2a4e; padding-top: 14px; margin-top: auto; }
    select { width: 100%; padding: 11px 14px; border: 1px solid #333; border-radius: 8px; background: #16213e; color: #eee; font-size: 0.95em; transition: border 0.2s; appearance: auto; }
    select:focus { outline: none; border-color: #00d4aa; }
    .ssid-row { display: flex; gap: 8px; }
    .ssid-row select { flex: 1; }
    .ssid-row button { flex: 0 0 auto; padding: 11px 14px; border: 1px solid #333; border-radius: 8px; background: #16213e; color: #eee; font-size: 0.95em; cursor: pointer; transition: border 0.2s; }
    .ssid-row button:hover { border-color: #00d4aa; }
    #manualSsidGroup { margin-top: 8px; padding-top: 8px; border-top: 1px dashed #333; }
    #manualSsidGroup label { margin-top: 0; font-size: 0.8em; color: #00d4aa; }
    @media (max-width: 700px) { .col-help { flex: 1 1 100%; } }
    /* Help panel */
    .help-panel strong { color: #eee; }
    .help-panel .tip { margin-bottom: 10px; }
    .help-panel .tip-icon { color: #00d4aa; margin-right: 4px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>⚡ ENTSO-E Price Monitor</h1>
    <p class="subtitle">Configure WiFi and ENTSO-E API to fetch electricity prices</p>
    
    <div class="layout">
      <!-- Left column: Configuration form -->
      <div class="col-form">
        <div class="card">
        <form id="configForm">
          <label>WiFi SSID</label>
          <div class="ssid-row">
            <select id="ssid" required>
              <option value="">-- Scanning networks... --</option>
            </select>
            <button type="button" id="scanBtn" onclick="scanNetworks()" title="Scan again">🔄 Scan</button>
          </div>
          <div class="info" id="scanInfo">Scanning for WiFi networks...</div>
          
          <div id="manualSsidGroup" style="display:none;">
            <label>Manual SSID</label>
            <input type="text" id="manualSsid" placeholder="Type network name manually">
          </div>
          
          <label>WiFi Password</label>
          <input type="password" id="password" placeholder="WiFi password">
          
          <label>ENTSO-E API Key</label>
          <input type="text" id="apiKey" placeholder="e.g. 1d9f2b3c-..." required>
          <label>Web Username</label>
          <input type="text" id="webUser" placeholder="Username for STA web interface" required>
          <label>Web Password</label>
          <input type="password" id="webPass" placeholder="Password for STA web interface" required>
          <div class="info">Required when the device is connected to your WiFi network.</div>
          <div class="info">Get it at transparency.entsoe.eu → My Account</div>
          
          <label>Country / Bidding Zone</label>
          <select id="biddingZone" onchange="updateTimezone()">
            <option value="10YNL----------L">🇳🇱 Netherlands (NL)</option>
            <option value="10YBE----------2">🇧🇪 Belgium (BE)</option>
            <option value="10Y1001A1001A82H">🇩🇪 Germany (DE/LU)</option>
            <option value="10YFR-RTE------C">🇫🇷 France (FR)</option>
            <option value="10YGB----------A">🇬🇧 United Kingdom (UK)</option>
            <option value="10Y1001A1001A83F">🇩🇰 Denmark (DK)</option>
            <option value="10YSE-1--------K">🇸🇪 Sweden (SE)</option>
            <option value="10YNO-0--------C">🇳🇴 Norway (NO)</option>
            <option value="10YPL-AREA-----S">🇵🇱 Poland (PL)</option>
            <option value="10YES-REE------0">🇪🇸 Spain (ES)</option>
            <option value="10YPT-REN------W">🇵🇹 Portugal (PT)</option>
            <option value="10YIT-GRTN-----B">🇮🇹 Italy (IT)</option>
            <option value="10YAT-APG------L">🇦🇹 Austria (AT)</option>
            <option value="10YCH-SWISSGRIDZ">🇨🇭 Switzerland (CH)</option>
            <option value="10YCZ-CEPS-----N">🇨🇿 Czech Republic (CZ)</option>
            <option value="10YSK-SEPS-----K">🇸🇰 Slovakia (SK)</option>
            <option value="10YHU-MAVIR----U">🇭🇺 Hungary (HU)</option>
            <option value="10YSI-ELES-----O">🇸🇮 Slovenia (SI)</option>
            <option value="10YHR-HEP------M">🇭🇷 Croatia (HR)</option>
            <option value="10YRO-TEL------P">🇷🇴 Romania (RO)</option>
            <option value="10YBG-ELECTR--E">🇧🇬 Bulgaria (BG)</option>
            <option value="10YGR-HTSO-----Y">🇬🇷 Greece (GR)</option>
            <option value="10YIE-1001A00010">🇮🇪 Ireland (IE)</option>
            <option value="10YFI-1--------U">🇫🇮 Finland (FI)</option>
            <option value="10YLT-1001A0008Q">🇱🇹 Lithuania (LT)</option>
            <option value="10YLV-1001A00074">🇱🇻 Latvia (LV)</option>
            <option value="10YEE-1001A00078">🇪🇪 Estonia (EE)</option>
            <option value="10YNL----------L" style="color:#00d4aa;">✏️ Other (type EIC code)...</option>
          </select>
          <div class="info">Bidding zone: <span id="zoneCodeDisplay">10YNL----------L</span></div>
          
          <div id="manualZoneGroup" style="display:none; margin-top:8px; padding-top:8px; border-top:1px dashed #333;">
            <label style="margin-top:0;font-size:0.8em;color:#00d4aa;">Custom Bidding Zone EIC Code</label>
            <input type="text" id="manualZone" placeholder="e.g. 10YNL----------L">
          </div>
          
          <label>Timezone <span id="tzAutoLabel" style="font-size:0.75em;color:#00d4aa;">(auto-set from country)</span></label>
          <select id="timezone" onchange="tzChangedManually()">
            <option value="CET-1CEST,M3.5.0,M10.5.0/3">🇳🇱/🇧🇪/🇩🇪/🇫🇷 CET/CEST - UTC+1/+2</option>
            <option value="GMT0">🇬🇧/🇮🇪 GMT/BST - UTC+0/+1</option>
            <option value="CET-2CEST,M3.5.0,M10.5.0/3">🇸🇪/🇩🇰/🇳🇴/🇫🇮 CET/CEST - UTC+2/+3</option>
            <option value="EET-2EEST,M3.5.0/3,M10.5.0/4">🇬🇷/🇧🇬/🇷🇴 EET/EEST - UTC+2/+3</option>
          </select>
          
          <button type="submit" class="btn">Save & Connect</button>
          <div class="status" id="status"></div>
        </form>
        </div>
      </div>
      <div class="col-help">
        <div class="help-panel">
          <h2>How to configure</h2>
          <p>Select your WiFi network, enter the password and your ENTSO-E API key, then save. If your network does not appear, choose the manual SSID option.</p>
        </div>
      </div>
    </div>
  </div>
  <script>
    const ssidSelect = document.getElementById('ssid');
    const scanInfo = document.getElementById('scanInfo');
    const scanBtn = document.getElementById('scanBtn');
    const manualSsidGroup = document.getElementById('manualSsidGroup');
    const manualSsidInput = document.getElementById('manualSsid');
    const zoneSelect = document.getElementById('biddingZone');
    const timezoneSelect = document.getElementById('timezone');
    const form = document.getElementById('configForm');
    const status = document.getElementById('status');

    function updateTimezone() {
      const mapping = {
        '10YNL----------L': 'CET-1CEST,M3.5.0,M10.5.0/3',
        '10YBE----------2': 'CET-1CEST,M3.5.0,M10.5.0/3',
        '10Y1001A1001A82H': 'CET-1CEST,M3.5.0,M10.5.0/3',
        '10YFR-RTE------C': 'CET-1CEST,M3.5.0,M10.5.0/3',
        '10YGB----------A': 'GMT0',
        '10Y1001A1001A83F': 'CET-1CEST,M3.5.0,M10.5.0/3',
        '10YSE-1--------K': 'CET-2CEST,M3.5.0,M10.5.0/3',
        '10YNO-0--------C': 'CET-2CEST,M3.5.0,M10.5.0/3',
        '10YFI-1--------U': 'CET-2CEST,M3.5.0,M10.5.0/3',
        '10YGR-HTSO-----Y': 'EET-2EEST,M3.5.0/3,M10.5.0/4',
        '10YBG-ELECTR--E': 'EET-2EEST,M3.5.0/3,M10.5.0/4',
        '10YRO-TEL------P': 'EET-2EEST,M3.5.0/3,M10.5.0/4'
      };
      if (zoneSelect.value in mapping) {
        timezoneSelect.value = mapping[zoneSelect.value];
      }
    }

    function tzChangedManually() {
      // Keep the user's manual timezone selection.
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
        scanInfo.textContent = networks.length === 0 ? 'No networks found yet. Try again.' : `Found ${networks.length} networks.`;
      } catch (err) {
        scanInfo.textContent = 'Error scanning: ' + err.message;
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
        scanInfo.textContent = ssidSelect.value ? `Selected: ${ssidSelect.value}` : 'Select a network from the list.';
      }
    });

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
      const isOther = (zoneSelect.selectedIndex === zoneSelect.options.length - 1);
      const biddingZone = isOther ? document.getElementById('manualZone').value : zoneSelect.value;
      const timezone = timezoneSelect.value;

      const data = new URLSearchParams();
      data.append('ssid', finalSsid);
      data.append('password', document.getElementById('password').value);
      data.append('apiKey', document.getElementById('apiKey').value);
      data.append('webUser', document.getElementById('webUser').value);
      data.append('webPass', document.getElementById('webPass').value);
      data.append('biddingZone', biddingZone);
      data.append('timezone', timezone);

      try {
        const res = await fetch('/save', { method: 'POST', body: data });
        const text = await res.text();
        if (res.ok) {
          status.className = 'status success';
          status.textContent = text;
          setTimeout(() => { window.location.href = '/'; }, 3000);
        } else {
          status.className = 'status error';
          status.textContent = text;
          btn.disabled = false;
          btn.textContent = 'Save & Connect';
        }
      } catch (err) {
        status.className = 'status error';
        status.textContent = err.message;
        btn.disabled = false;
        btn.textContent = 'Save & Connect';
      }
    });

    scanNetworks();
  </script>
</body>
</html>
)rawliteral";

// Scan WiFi networks and return JSON list
void handleScan();

// Root page - show config form
void handleRoot();

// Handle save POST request
void handleSave();

// Handle not-found (captive portal redirect)
void handleNotFound();

// Start the configuration portal in AP mode
void startConfigPortal();

// Try to connect to WiFi with saved config
bool connectWithStoredConfig();

// Main initialization - call from setup()
bool initWiFi();

// Disconnect WiFi to save power
void disconnectWiFi();

// Getter functions for config values
const char* getApiKey();
const char* getBiddingZone();
const char* getTimezone();

#endif // HELPER_WIFI_PORTAL_H

