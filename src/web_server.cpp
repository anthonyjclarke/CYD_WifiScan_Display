#include "web_server.h"
#include "debug.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>

// ============================================================
// Statics — set once via initWebServer()
// ============================================================
static AsyncWebServer  server(80);
static NetworkEntry*        g_nets       = nullptr;
static volatile int*        g_count      = nullptr;
static SemaphoreHandle_t*   g_mutex      = nullptr;
static volatile uint32_t*   g_scanCount  = nullptr;
static volatile bool*       g_scanning   = nullptr;
static volatile uint32_t*   g_lastScanMs = nullptr;
static void (*g_triggerScan)()           = nullptr;

// ============================================================
// Embedded HTML dashboard
// Served from RAM — ~7 KB. Use PROGMEM on ESP8266 if porting.
// ============================================================
static const char INDEX_HTML[] = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WiFi Scanner</title>
<style>
:root{--bg:#0a0f1a;--card:#111827;--border:#1f2d3d;--text:#e2e8f0;--muted:#64748b;
      --cyan:#06b6d4;--green:#22c55e;--yellow:#eab308;--red:#ef4444;--purple:#a855f7}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--text);font-family:'Segoe UI',system-ui,sans-serif;
     font-size:14px;padding:1rem;max-width:900px;margin:0 auto}
h1{color:var(--cyan);font-size:1.4rem;margin-bottom:.2rem}
.sub{color:var(--muted);font-size:.8rem;margin-bottom:1rem;display:flex;align-items:center;gap:.5rem}
.dot{width:8px;height:8px;background:var(--green);border-radius:50%;
     animation:pulse 2s infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:1rem;margin-bottom:1rem}
@media(max-width:550px){.grid{grid-template-columns:1fr}}
.card{background:var(--card);border:1px solid var(--border);border-radius:8px;padding:.8rem 1rem}
.card h2{font-size:.75rem;color:var(--muted);text-transform:uppercase;letter-spacing:.08em;margin-bottom:.6rem}
.stat{display:flex;justify-content:space-between;padding:.2rem 0}
.sv{color:var(--cyan);font-weight:600}
.btn{background:#1d4ed8;color:#fff;border:none;padding:.45rem 1.1rem;border-radius:6px;
     cursor:pointer;font-size:.85rem;transition:background .2s}
.btn:hover{background:#2563eb}
.btn:disabled{background:#374151;cursor:wait}
.net-hdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:.5rem}
.net-hdr h2{margin:0}
.controls{display:flex;gap:.5rem;align-items:center}
.age{color:var(--muted);font-size:.8rem}
table{width:100%;border-collapse:collapse}
th{background:#1e293b;color:var(--muted);font-size:.7rem;text-transform:uppercase;
   letter-spacing:.06em;padding:.4rem .5rem;text-align:left;white-space:nowrap}
tr:nth-child(even) td{background:#0d1624}
td{padding:.4rem .5rem;border-bottom:1px solid #1a2535;vertical-align:middle}
.ssid{font-weight:500;max-width:200px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.badge{display:inline-block;padding:.1rem .35rem;border-radius:4px;
       font-size:.68rem;font-weight:700;white-space:nowrap}
.b24{background:rgba(6,182,212,.15);color:var(--cyan);border:1px solid rgba(6,182,212,.35)}
.b5{background:rgba(168,85,247,.15);color:var(--purple);border:1px solid rgba(168,85,247,.35)}
.bars{display:inline-flex;align-items:flex-end;gap:2px;height:14px;vertical-align:middle}
.bar{width:4px;border-radius:1px}
.rssi-n{font-size:.8rem;margin-left:3px;vertical-align:middle}
select{background:#1e293b;color:var(--text);border:1px solid var(--border);
       padding:.3rem .5rem;border-radius:4px;font-size:.85rem}
.dbg-row{display:flex;gap:.5rem;align-items:center;flex-wrap:wrap}
</style>
</head>
<body>
<h1>&#x1F4E1; WiFi Scanner</h1>
<div class="sub">
  <span class="dot"></span>
  <span>ESP32 CYD &nbsp;&bull;&nbsp; <span id="hdr-ip">--</span> &nbsp;&bull;&nbsp; Auto-refresh 10 s</span>
</div>

<div class="grid">
  <div class="card">
    <h2>Device Status</h2>
    <div class="stat"><span>IP Address</span><span class="sv" id="s-ip">--</span></div>
    <div class="stat"><span>Free Heap</span><span class="sv" id="s-heap">--</span></div>
    <div class="stat"><span>Uptime</span><span class="sv" id="s-up">--</span></div>
    <div class="stat"><span>Total Scans</span><span class="sv" id="s-scans">--</span></div>
    <div class="stat"><span>Status</span><span class="sv" id="s-state">--</span></div>
  </div>
  <div class="card">
    <h2>Debug Control</h2>
    <div class="dbg-row">
      <label>Level:</label>
      <select id="dbg-sel">
        <option value="0">0 — Off</option>
        <option value="1">1 — Error</option>
        <option value="2">2 — Warn</option>
        <option value="3" selected>3 — Info</option>
        <option value="4">4 — Verbose</option>
      </select>
      <button class="btn" onclick="setDbg()">Apply</button>
    </div>
    <p style="color:var(--muted);font-size:.75rem;margin-top:.6rem">
      Changes serial output level on the device in real-time.
    </p>
  </div>
</div>

<div class="card">
  <div class="net-hdr">
    <h2>Networks &nbsp;<span id="net-cnt" style="color:var(--cyan)">--</span></h2>
    <div class="controls">
      <span class="age" id="scan-age"></span>
      <button class="btn" id="scan-btn" onclick="doScan()">Scan Now</button>
    </div>
  </div>
  <div style="overflow-x:auto">
    <table>
      <thead>
        <tr>
          <th>#</th><th>SSID</th><th>Band</th><th>Ch</th>
          <th>Signal</th><th>dBm</th><th>Sec</th>
        </tr>
      </thead>
      <tbody id="tbody">
        <tr><td colspan="7" style="text-align:center;color:var(--muted);padding:2rem">Loading…</td></tr>
      </tbody>
    </table>
  </div>
</div>

<script>
let lastScanTs = 0;

function bars(rssi){
  const lit = rssi > -50 ? 5 : rssi > -65 ? 4 : rssi > -75 ? 2 : 1;
  const col = rssi > -50 ? '#22c55e' : rssi > -65 ? '#06b6d4' : rssi > -75 ? '#eab308' : '#ef4444';
  let h = '<span class="bars">';
  for(let i=1;i<=5;i++){
    const ht = 4+i*2;
    h += `<span class="bar" style="height:${ht}px;background:${i<=lit?col:'#2d3748'}"></span>`;
  }
  return h+'</span>';
}
function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}
function fmtHeap(b){return(b/1024).toFixed(1)+' KB';}
function fmtUp(ms){
  const s=Math.floor(ms/1000);
  if(s<60) return s+'s';
  if(s<3600) return Math.floor(s/60)+'m '+s%60+'s';
  return Math.floor(s/3600)+'h '+Math.floor(s%3600/60)+'m';
}

async function fetchNets(){
  try{
    const d=await(await fetch('/api/networks')).json();
    lastScanTs=d.scanTime;
    document.getElementById('net-cnt').textContent=d.count;
    if(!d.count){
      document.getElementById('tbody').innerHTML=
        '<tr><td colspan="7" style="text-align:center;color:var(--muted);padding:2rem">No networks found</td></tr>';
      return;
    }
    document.getElementById('tbody').innerHTML=d.networks.map((n,i)=>`
      <tr>
        <td>${i+1}</td>
        <td class="ssid">${esc(n.ssid||'&lt;hidden&gt;')}</td>
        <td><span class="badge ${n.band==='5GHz'?'b5':'b24'}">${n.band}</span></td>
        <td>${n.channel}</td>
        <td>${bars(n.rssi)}<span class="rssi-n" style="color:${n.rssi>-50?'#22c55e':n.rssi>-65?'#06b6d4':n.rssi>-75?'#eab308':'#ef4444'}">${n.rssi}</span></td>
        <td style="color:${n.rssi>-50?'#22c55e':n.rssi>-65?'#06b6d4':n.rssi>-75?'#eab308':'#ef4444'}">${n.rssi}</td>
        <td>${n.secure?'<span style="color:#f97316">&#x1F512;</span>':'<span style="color:#22c55e;font-size:.7rem">OPEN</span>'}</td>
      </tr>`).join('');
  }catch(e){console.error(e);}
}

async function fetchStatus(){
  try{
    const d=await(await fetch('/api/status')).json();
    document.getElementById('s-ip').textContent=d.ip;
    document.getElementById('hdr-ip').textContent=d.ip;
    document.getElementById('s-heap').textContent=fmtHeap(d.heap);
    document.getElementById('s-up').textContent=fmtUp(d.uptime);
    document.getElementById('s-scans').textContent=d.scanCount;
    document.getElementById('s-state').textContent=d.scanning?'Scanning…':'Idle';
    document.getElementById('s-state').style.color=d.scanning?'#eab308':'#22c55e';
    document.getElementById('dbg-sel').value=d.debugLevel;
    if(lastScanTs>0){
      const age=Math.round((Date.now()/1000)-lastScanTs);
      document.getElementById('scan-age').textContent='Last: '+age+'s ago';
    }
  }catch(e){console.error(e);}
}

async function doScan(){
  const btn=document.getElementById('scan-btn');
  btn.disabled=true; btn.textContent='Scanning…';
  try{
    await fetch('/api/scan',{method:'POST'});
    setTimeout(async()=>{
      await fetchNets(); await fetchStatus();
      btn.disabled=false; btn.textContent='Scan Now';
    },5000);
  }catch(e){btn.disabled=false; btn.textContent='Scan Now';}
}

async function setDbg(){
  const lv=parseInt(document.getElementById('dbg-sel').value);
  await fetch('/api/debug',{method:'POST',
    headers:{'Content-Type':'application/json'},
    body:JSON.stringify({level:lv})});
}

fetchNets(); fetchStatus();
setInterval(fetchNets, 10000);
setInterval(fetchStatus, 5000);
</script>
</body>
</html>
)rawhtml";

// ============================================================
// Route handlers
// ============================================================

static void handleRoot(AsyncWebServerRequest* req)
{
    req->send(200, "text/html", INDEX_HTML);
}

static void handleNetworks(AsyncWebServerRequest* req)
{
    StaticJsonDocument<4096> doc;
    doc["count"]    = *g_count;
    doc["scanTime"] = *g_lastScanMs / 1000;  // epoch-like seconds (millis/1000)
    doc["scanning"] = *g_scanning;

    JsonArray arr = doc.createNestedArray("networks");

    xSemaphoreTake(*g_mutex, pdMS_TO_TICKS(200));
    for (int i = 0; i < *g_count; i++) {
        JsonObject o = arr.createNestedObject();
        o["ssid"]    = g_nets[i].ssid;
        o["rssi"]    = g_nets[i].rssi;
        o["channel"] = g_nets[i].channel;
        o["secure"]  = g_nets[i].secure;
        o["band"]    = g_nets[i].is5GHz ? "5GHz" : "2.4GHz";
    }
    xSemaphoreGive(*g_mutex);

    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
}

static void handleStatus(AsyncWebServerRequest* req)
{
    StaticJsonDocument<256> doc;
    doc["ip"]         = WiFi.localIP().toString();
    doc["heap"]       = ESP.getFreeHeap();
    doc["uptime"]     = millis();
    doc["scanCount"]  = *g_scanCount;
    doc["scanning"]   = *g_scanning;
    doc["debugLevel"] = debugLevel;

    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
}

static void handleScanPost(AsyncWebServerRequest* req)
{
    if (g_triggerScan && !*g_scanning) {
        g_triggerScan();
        DBG_INFO("Web: scan triggered");
        req->send(202, "application/json", "{\"status\":\"scan_started\"}");
    } else {
        req->send(409, "application/json", "{\"status\":\"scan_already_running\"}");
    }
}

static void handleDebugGet(AsyncWebServerRequest* req)
{
    StaticJsonDocument<64> doc;
    doc["level"] = debugLevel;
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
}

// ============================================================
// Init
// ============================================================

void initWebServer(
    NetworkEntry*       nets,
    volatile int*       netCount,
    SemaphoreHandle_t*  mutex,
    volatile uint32_t*  scanCount,
    volatile bool*      scanning,
    volatile uint32_t*  lastScanMs,
    void (*triggerScanCb)())
{
    g_nets        = nets;
    g_count       = netCount;
    g_mutex       = mutex;
    g_scanCount   = scanCount;
    g_scanning    = scanning;
    g_lastScanMs  = lastScanMs;
    g_triggerScan = triggerScanCb;

    server.on("/",             HTTP_GET,  handleRoot);
    server.on("/api/networks", HTTP_GET,  handleNetworks);
    server.on("/api/status",   HTTP_GET,  handleStatus);
    server.on("/api/scan",     HTTP_POST, handleScanPost);
    server.on("/api/debug",    HTTP_GET,  handleDebugGet);

    // POST /api/debug  body: {"level": N}
    // Use the 3-handler form so we can read the raw body bytes.
    server.on("/api/debug", HTTP_POST,
        [](AsyncWebServerRequest* req) {},   // request complete handler (unused)
        nullptr,                             // upload handler (unused)
        [](AsyncWebServerRequest* req, uint8_t* data, size_t len,
           size_t /*index*/, size_t /*total*/) {
            StaticJsonDocument<64> doc;
            if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
                return;
            }
            int lv = doc["level"] | -1;
            if (lv < 0 || lv > 4) {
                req->send(400, "application/json", "{\"error\":\"invalid level\"}");
                return;
            }
            debugLevel = static_cast<uint8_t>(lv);
            DBG_INFO("Web: debug level set to %d", debugLevel);
            req->send(200, "application/json", "{\"status\":\"ok\"}");
        });

    server.onNotFound([](AsyncWebServerRequest* req) {
        req->send(404, "text/plain", "Not found");
    });

    server.begin();
    DBG_INFO("Web server started on port 80");
}
