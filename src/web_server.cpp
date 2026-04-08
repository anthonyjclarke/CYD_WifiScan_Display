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
static ChannelStat*         g_channels   = nullptr;
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
.chart-card{margin-bottom:1rem}
.chart-wrap{margin-top:.2rem}
.chart-svg{width:100%;height:220px;display:block}
.axis{stroke:#334155;stroke-width:1}
.gridline{stroke:#1e293b;stroke-width:1}
.chan-fill{fill:rgba(6,182,212,.14);stroke:rgba(6,182,212,.9);stroke-width:3}
.chan-dot{fill:#06b6d4;stroke:#0a0f1a;stroke-width:2}
.chan-label{fill:#94a3b8;font-size:12px;font-weight:600}
.chan-label.hot{fill:#e2e8f0}
.chan-count{fill:#e2e8f0;font-size:12px;font-weight:700}
.hint{color:var(--muted);font-size:.78rem;margin-top:.35rem}
.detail-card{margin-top:.9rem;padding:.8rem;border:1px solid #1a2535;border-radius:8px;background:#0d1624}
.detail-head{display:flex;justify-content:space-between;align-items:center;gap:1rem;margin-bottom:.5rem;flex-wrap:wrap}
.detail-title{font-size:.85rem;font-weight:700;color:#e2e8f0}
.detail-meta{font-size:.76rem;color:var(--muted)}
.detail-list{display:grid;gap:.45rem}
.detail-row{display:grid;grid-template-columns:minmax(0,1.5fr) 62px 52px 84px;gap:.6rem;align-items:center;font-size:.82rem}
.detail-row.head{color:var(--muted);text-transform:uppercase;letter-spacing:.05em;font-size:.68rem}
.detail-ssid{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.detail-group{margin-top:.65rem}
.detail-group-title{font-size:.72rem;color:var(--muted);text-transform:uppercase;letter-spacing:.06em;margin-bottom:.35rem}
.impact{display:inline-block;padding:.12rem .4rem;border-radius:999px;font-size:.72rem;font-weight:700;text-align:center}
.impact-3{background:rgba(239,68,68,.18);color:#fca5a5}
.impact-2{background:rgba(234,179,8,.18);color:#fde047}
.impact-1{background:rgba(34,197,94,.16);color:#86efac}
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
    <h2>Channel Congestion &nbsp;<span id="hot-ch" style="color:var(--cyan)">--</span></h2>
    <div class="controls">
      <span class="age">2.4 GHz overlap model</span>
    </div>
  </div>
  <div class="chart-wrap">
    <svg id="channel-chart" class="chart-svg" viewBox="0 0 760 220" preserveAspectRatio="none" aria-label="Channel congestion graph"></svg>
  </div>
  <p class="hint">Higher peaks mean more nearby AP overlap and stronger interference around that channel.</p>
  <div id="channel-detail"></div>
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
let lastScanAge = 0;
let lastNetworks = [];
let selectedChannel = 1;

function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}

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
function fmtHeap(b){return(b/1024).toFixed(1)+' KB';}
function fmtUp(ms){
  const s=Math.floor(ms/1000);
  if(s<60) return s+'s';
  if(s<3600) return Math.floor(s/60)+'m '+s%60+'s';
  return Math.floor(s/3600)+'h '+Math.floor(s%3600/60)+'m';
}

function overlapImpact(netChannel, targetChannel){
  const delta=Math.abs(netChannel-targetChannel);
  if(delta>2) return 0;
  return 3-delta;
}

function weightBand(weight){
  if(weight>=3) return {label:'High', cls:'impact-3'};
  if(weight===2) return {label:'Med', cls:'impact-2'};
  return {label:'Low', cls:'impact-1'};
}

function contributorRows(channel){
  return lastNetworks
    .map((n)=>{
      const impact=overlapImpact(n.channel, channel);
      if(!impact) return null;
      return {
        ssid:n.ssid || '<hidden>',
        channel:n.channel,
        rssi:n.rssi,
        secure:n.secure,
        impact,
        onChannel:n.channel===channel
      };
    })
    .filter(Boolean)
    .sort((a,b)=>Number(b.onChannel)-Number(a.onChannel) || b.impact-a.impact || b.rssi-a.rssi || a.channel-b.channel);
}

function contributorGroup(title, rows){
  if(!rows.length) return '';
  return `
    <div class="detail-group">
      <div class="detail-group-title">${title}</div>
      <div class="detail-list">
        <div class="detail-row head">
          <span>SSID</span><span>AP Ch</span><span>RSSI</span><span>Impact</span>
        </div>
        ${rows.map(row=>{
          const band=weightBand(row.impact);
          return `
            <div class="detail-row">
              <span class="detail-ssid">${esc(row.ssid)}</span>
              <span>${row.channel}</span>
              <span>${row.rssi}</span>
              <span><span class="impact ${band.cls}">${band.label} (${row.impact})</span></span>
            </div>`;
        }).join('')}
      </div>
    </div>`;
}

function renderChannelDetails(channel, channels){
  const host=document.getElementById('channel-detail');
  const stat=(channels||[]).find(c=>c.channel===channel);
  const contributors=contributorRows(channel);
  const onChannel=contributors.filter(row=>row.onChannel);
  const adjacent=contributors.filter(row=>!row.onChannel);

  if(!stat){
    host.innerHTML='<div class="detail-card"><div class="detail-title">Channel details unavailable</div></div>';
    return;
  }

  if(!contributors.length){
    host.innerHTML=`
      <div class="detail-card">
        <div class="detail-head">
          <div>
            <div class="detail-title">Channel ${channel} contributors</div>
            <div class="detail-meta">Score ${stat.score} · 0 contributing networks</div>
          </div>
        </div>
        <div class="detail-meta">No scanned networks overlap this channel.</div>
      </div>`;
    return;
  }

  host.innerHTML=`
    <div class="detail-card">
      <div class="detail-head">
        <div>
          <div class="detail-title">Channel ${channel} contributors</div>
          <div class="detail-meta">Score ${stat.score} · ${stat.count} APs on-channel · ${contributors.length} overlapping networks</div>
        </div>
        <div class="detail-meta">Tap another channel point to inspect it</div>
      </div>
      ${contributorGroup(`On-channel (${onChannel.length})`, onChannel)}
      ${contributorGroup(`Adjacent-channel (${adjacent.length})`, adjacent)}
    </div>`;
}

function renderChannelGraph(channels){
  const svg=document.getElementById('channel-chart');
  if(!channels.length){
    document.getElementById('hot-ch').textContent='--';
    svg.innerHTML='';
    document.getElementById('channel-detail').innerHTML='';
    return;
  }
  const width=760, height=220;
  const left=26, right=18, top=18, bottom=32;
  const graphW=width-left-right, graphH=height-top-bottom;
  const maxScore=Math.max(1,...channels.map(c=>c.score));
  const step=graphW/Math.max(1,channels.length-1);

  const pts=channels.map((c,i)=>{
    const x=left+i*step;
    const y=top+graphH-(c.score/maxScore)*graphH;
    return {x,y,c};
  });

  const area=[
    `M ${pts[0].x} ${top+graphH}`,
    ...pts.map(p=>`L ${p.x} ${p.y}`),
    `L ${pts[pts.length-1].x} ${top+graphH}`,
    'Z'
  ].join(' ');

  const line=['M',pts[0].x,pts[0].y,...pts.slice(1).flatMap(p=>['L',p.x,p.y])].join(' ');
  const hot=channels.reduce((best,c)=>c.score>best.score?c:best,channels[0]||{channel:'--',score:0});
  if(!channels.some(c=>c.channel===selectedChannel)) selectedChannel=hot.channel;
  document.getElementById('hot-ch').textContent=`Hot channel: ${hot.channel}`;

  let markup='';
  for(let i=0;i<4;i++){
    const y=top+graphH-(graphH*i/3);
    markup+=`<line class="gridline" x1="${left}" y1="${y}" x2="${width-right}" y2="${y}" />`;
  }
  markup+=`<line class="axis" x1="${left}" y1="${top+graphH}" x2="${width-right}" y2="${top+graphH}" />`;
  markup+=`<path class="chan-fill" d="${area}" />`;
  markup+=`<path d="${line}" fill="none" stroke="#22d3ee" stroke-width="3" />`;

  pts.forEach((p,i)=>{
    const hotClass=p.c.channel===hot.channel?'hot':'';
    const selected=p.c.channel===selectedChannel;
    markup+=`<circle class="chan-dot" cx="${p.x}" cy="${p.y}" r="4" />`;
    markup+=`<circle cx="${p.x}" cy="${p.y}" r="${selected?10:8}" fill="transparent" stroke="${selected?'#f8fafc':'transparent'}" stroke-width="2" style="cursor:pointer" onclick="selectChannel(${p.c.channel})" />`;
    if(p.c.count>0){
      markup+=`<text class="chan-count" x="${p.x}" y="${Math.max(top+10,p.y-8)}" text-anchor="middle">${p.c.count}</text>`;
    }
    markup+=`<text class="chan-label ${hotClass}" x="${p.x}" y="${height-10}" text-anchor="middle" style="cursor:pointer" onclick="selectChannel(${p.c.channel})">${i+1}</text>`;
  });

  svg.innerHTML=markup;
  renderChannelDetails(selectedChannel, channels);
}

function selectChannel(channel){
  selectedChannel=channel;
  renderChannelGraph(window.lastChannels || []);
}

async function fetchNets(){
  try{
    const d=await(await fetch('/api/networks')).json();
    lastScanAge=d.scanAge||0;
    lastNetworks=d.networks||[];
    window.lastChannels=d.channels||[];
    renderChannelGraph(d.channels||[]);
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
    if(lastScanAge>0){
      document.getElementById('scan-age').textContent='Last: '+lastScanAge+'s ago';
    }else{
      document.getElementById('scan-age').textContent='Last: --';
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
    StaticJsonDocument<6144> doc;
    doc["count"]    = *g_count;
    doc["scanAge"]  = (*g_lastScanMs == 0) ? 0 : ((millis() - *g_lastScanMs) / 1000);
    doc["scanning"] = *g_scanning;

    JsonArray arr = doc.createNestedArray("networks");
    JsonArray channelArr = doc.createNestedArray("channels");

    if (xSemaphoreTake(*g_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        req->send(503, "application/json", "{\"error\":\"data_busy\"}");
        return;
    }

    for (int i = 0; i < *g_count; i++) {
        JsonObject o = arr.createNestedObject();
        o["ssid"]    = g_nets[i].ssid;
        o["rssi"]    = g_nets[i].rssi;
        o["channel"] = g_nets[i].channel;
        o["secure"]  = g_nets[i].secure;
        o["band"]    = g_nets[i].is5GHz ? "5GHz" : "2.4GHz";
    }
    for (int i = 0; i < MAX_WIFI_CHANNELS; i++) {
        JsonObject c = channelArr.createNestedObject();
        c["channel"]  = g_channels[i].channel;
        c["count"]    = g_channels[i].apCount;
        c["peakRssi"] = g_channels[i].peakRssi;
        c["score"]    = g_channels[i].score;
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
    ChannelStat*        channels,
    SemaphoreHandle_t*  mutex,
    volatile uint32_t*  scanCount,
    volatile bool*      scanning,
    volatile uint32_t*  lastScanMs,
    void (*triggerScanCb)())
{
    g_nets        = nets;
    g_count       = netCount;
    g_channels    = channels;
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
