// Web remote served by the board. Texts are in the dictionary at the bottom of this
// file (Spanish and English); the language follows the browser, or use /?lang=es|en.
const char PAGE[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#14161a">
<title>Onkyo</title>
<style>
:root{--bg:#14161a;--card:#1e2127;--btn:#2a2e36;--txt:#e8eaee;--mut:#8b92a0;--acc:#4da3ff;--ok:#2ecc71;--bad:#e5484d;--line:#ffffff14}
@media (prefers-color-scheme:light){:root{--bg:#e9ebef;--card:#fff;--btn:#eef0f4;--txt:#1b1e24;--mut:#6b7280;--line:#00000014}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--txt);font-family:system-ui,-apple-system,sans-serif;display:flex;justify-content:center;min-height:100vh}
.remote{width:100%;max-width:380px;margin:16px;padding:20px 18px 22px;background:var(--card);border-radius:34px;box-shadow:0 8px 30px #0005;align-self:flex-start}
.brand{text-align:center;font-weight:700;font-size:14px;letter-spacing:.35em}
.brand small{display:block;letter-spacing:.15em;font-weight:500;color:var(--mut);font-size:11px;margin-top:3px}
.chips{display:flex;flex-wrap:wrap;justify-content:center;gap:6px;margin:14px 0 6px}
.chip{display:inline-flex;align-items:center;gap:6px;background:var(--btn);border-radius:12px;padding:5px 10px;font-size:12px;color:var(--txt)}
.dot{width:8px;height:8px;border-radius:50%;background:var(--mut)}
.dot.on{background:var(--ok)}
.bars{display:inline-flex;align-items:flex-end;gap:2px;height:12px}
.bars i{width:3px;background:var(--mut);opacity:.35;border-radius:1px}
.bars i:nth-child(1){height:4px}.bars i:nth-child(2){height:7px}.bars i:nth-child(3){height:10px}.bars i:nth-child(4){height:12px}
.bars.n1 i:nth-child(-n+1),.bars.n2 i:nth-child(-n+2),.bars.n3 i:nth-child(-n+3),.bars.n4 i:nth-child(-n+4){background:var(--ok);opacity:1}
.bars.n1 i:nth-child(-n+1){background:var(--bad)}.bars.n2 i:nth-child(-n+2){background:#f5a524}
h3{margin:20px 4px 8px;font-size:11px;font-weight:600;letter-spacing:.12em;text-transform:uppercase;color:var(--mut)}
button{border:0;border-radius:18px;background:var(--btn);color:var(--txt);font:inherit;font-size:15px;font-weight:600;padding:14px 0;cursor:pointer;transition:transform .08s,background .15s;touch-action:manipulation;-webkit-user-select:none;user-select:none}
button:active{transform:scale(.95);background:var(--acc);color:#fff}
.pwr{display:flex;gap:12px}
.pwr button{flex:1;height:60px;border-radius:30px}
.pwr .on{color:var(--ok)}.pwr .off{color:var(--bad)}
.alt{width:100%;margin-top:10px;padding:10px 0;font-size:13px;line-height:1.3}
.alt small{display:block;font-weight:400;font-size:11px;color:var(--mut)}
.alt:active small{color:#fffc}
.inputs{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.inputs button{padding:10px 0;line-height:1.2}
.inputs small{display:block;font-weight:400;font-size:11px;color:var(--mut)}
.inputs button:active small{color:#fffc}
.vol{display:flex;align-items:center;justify-content:space-between;gap:10px}
.vol .v{width:84px;height:84px;border-radius:50%;font-size:36px;font-weight:300;padding:0}
.vol .m{flex:1;height:44px;font-size:13px;padding:0}
.vol .m.act{background:var(--bad);color:#fff}
.hint{font-size:11.5px;color:var(--mut);margin:8px 4px 0;line-height:1.4}
.auto{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-top:22px;padding-top:16px;border-top:1px solid var(--line)}
.auto b{font-size:14px;font-weight:600}
.auto p{margin:3px 0 0;font-size:11.5px;color:var(--mut);line-height:1.4}
.sw{flex:none;width:52px;height:30px;border-radius:15px;padding:0;position:relative}
.sw::after{content:"";position:absolute;top:3px;left:3px;width:24px;height:24px;border-radius:50%;background:var(--mut);transition:transform .2s,background .2s}
.sw.act{background:var(--ok)}.sw.act::after{transform:translateX(22px);background:#fff}
details{margin-top:14px;border-top:1px solid var(--line);padding-top:10px}
summary{cursor:pointer;font-size:13px;font-weight:600;color:var(--txt);list-style:none;display:flex;justify-content:space-between;align-items:center;padding:4px}
summary::-webkit-details-marker{display:none}
summary::after{content:"\25BE";color:var(--mut);transition:transform .2s}
details[open] summary::after{transform:rotate(180deg)}
table{width:100%;border-collapse:collapse;font-size:12px;margin-top:8px}
td{padding:5px 4px;border-bottom:1px solid var(--line)}td:first-child{color:var(--mut)}td:last-child{text-align:right;font-variant-numeric:tabular-nums}
.sec{display:flex;gap:8px;margin-top:10px}
.sec button{flex:1;padding:10px 0;font-size:13px}
pre{background:#0b0d10;color:#9fe6a0;border-radius:12px;padding:10px;height:190px;overflow:auto;font-size:11px;line-height:1.45;margin:8px 0 4px;white-space:pre-wrap;word-break:break-word;-webkit-user-select:text;user-select:text}
.foot{text-align:center;font-size:11px;color:var(--mut);margin:18px 0 0}
.foot a{color:var(--acc);text-decoration:none}
#toast{position:fixed;bottom:22px;left:50%;transform:translateX(-50%);background:#000d;color:#fff;padding:8px 16px;border-radius:14px;font-size:13px;opacity:0;transition:opacity .2s;pointer-events:none;white-space:nowrap}
</style></head><body>
<div class="remote">
  <div class="brand">ONKYO<small><span id="model"></span> &middot; <span data-i="subtitle"></span></small></div>
  <div class="chips">
    <span class="chip"><i class="dot" id="dotTv"></i><span id="tvTxt">&hellip;</span></span>
    <span class="chip"><span class="bars" id="bars"><i></i><i></i><i></i><i></i></span><span id="wifiTxt">&hellip;</span></span>
    <span class="chip"><i class="dot" id="dotSync"></i><span id="syncTxt">&hellip;</span></span>
  </div>

  <h3 data-i="power"></h3>
  <div class="pwr">
    <button class="on" data-i="on" onclick="send('on','on')"></button>
    <button class="off" data-i="off" onclick="send('off','off')"></button>
  </div>
  <button class="alt" onclick="send('off_any_input','offAny')"><span data-i="offAny"></span><small data-i="offAnySub"></small></button>
  <p class="hint" data-h="powerHint"></p>

  <h3 data-i="input"></h3>
  <div class="inputs">
    <button onclick="send('in_tv','inTv')">TV<small data-i="tvSub"></small></button>
    <button onclick="send('in_dvd','inDvd')">DVD<small>&nbsp;</small></button>
    <button onclick="send('in_cd','inCd')">CD<small>&nbsp;</small></button>
    <button onclick="send('in_tape','inTape')">TAPE<small>&nbsp;</small></button>
  </div>

  <h3 data-i="volume"></h3>
  <div class="vol">
    <button class="v" data-c="vol_down" data-a="volDown">&minus;</button>
    <button class="m" id="mute" onclick="toggleMute()">MUTE</button>
    <button class="v" data-c="vol_up" data-a="volUp">+</button>
  </div>
  <p class="hint" data-h="volHint"></p>

  <div class="auto">
    <div><b data-i="auto"></b><p id="autoTxt"></p></div>
    <button class="sw" id="sw" data-a="auto" onclick="toggleAuto()"></button>
  </div>

  <details id="dTech">
    <summary data-i="tech"></summary>
    <table>
      <tr><td data-i="rSsid"></td><td id="tSsid">&ndash;</td></tr>
      <tr><td data-i="rSignal"></td><td id="tRssi">&ndash;</td></tr>
      <tr><td data-i="rChannel"></td><td id="tCh">&ndash;</td></tr>
      <tr><td data-i="rIp"></td><td id="tIp">&ndash;</td></tr>
      <tr><td>MAC</td><td id="tMac">&ndash;</td></tr>
      <tr><td data-i="rTvIp"></td><td id="tTvIp">&ndash;</td></tr>
      <tr><td data-i="rTv"></td><td id="tTv">&ndash;</td></tr>
      <tr><td data-i="rAction"></td><td id="tAct">&ndash;</td></tr>
      <tr><td data-i="rUptime"></td><td id="tUp">&ndash;</td></tr>
      <tr><td data-i="rHeap"></td><td id="tHeap">&ndash;</td></tr>
      <tr><td>Firmware</td><td id="tFw">&ndash;</td></tr>
    </table>
    <div class="sec"><button data-i="restart" onclick="restart()"></button></div>
  </details>

  <details id="dLog">
    <summary data-i="log"></summary>
    <pre id="lg"></pre>
    <div class="hint" data-i="logHint"></div>
  </details>

  <p class="foot"><a href="https://github.com/lhupyn/onkyo-ri-wifi" target="_blank" rel="noopener" data-i="github"></a> &middot; <span id="footFw"></span></p>
</div>
<div id="toast"></div>
<script>
const TEXT = {
  en: {
    subtitle: 'RI over WiFi', power: 'Power', on: 'Power on', off: 'Power off',
    offAny: 'Power off from any input', offAnySub: 'switches to TV (V3), then standby',
    powerHint: '<b>Power off</b> puts the receiver in standby, like its STANDBY button; it does not cut the mains. The remote cannot know whether the receiver is on.',
    input: 'Input', tvSub: 'V3 input', inTv: 'TV input', inDvd: 'DVD input', inCd: 'CD input', inTape: 'TAPE input',
    volume: 'Volume', volDown: 'Volume down', volUp: 'Volume up',
    volHint: 'Hold + or − to repeat. Volume, mute and power off only respond while the <b>TV (V3)</b> input is selected.',
    auto: 'Follow the TV', autoOn: 'When the TV turns on or off, the receiver turns on (TV input) or off by itself.',
    autoOff: 'Off: the TV does not affect the receiver. Use it to listen to music without the TV.',
    tech: 'Technical details', rSsid: 'WiFi network', rSignal: 'Signal', rChannel: 'Channel', rIp: 'Board IP',
    rTvIp: 'TV IP', rTv: 'TV', rAction: 'Last automatic action', rUptime: 'Uptime', rHeap: 'Free memory',
    restart: 'Restart the board', confirmRestart: 'Restart the board? It takes about 15 seconds to come back.',
    restarting: 'Restarting… wait 15 s',
    log: 'Event log', logHint: 'Last 60 events. Older ones are overwritten and everything is cleared on restart. Volume steps are not logged.',
    noEvents: 'No events yet.', github: 'Code and manual on GitHub',
    tvOn: 'TV on', tvOff: 'TV off', tvUnknown: 'TV ?', sent: 'Sent: ', error: 'Could not send', offline: 'No connection to the board',
    signal: 'Signal ', sq: ['excellent', 'good', 'fair', 'weak', 'very weak'], followsTv: 'Follows TV', manual: 'Manual only',
    never: 'none since boot', ago: ' ago', units: ['d', 'h', 'min']
  },
  es: {
    subtitle: 'RI por WiFi', power: 'Encendido', on: 'Encender', off: 'Apagar',
    offAny: 'Apagar desde cualquier entrada', offAnySub: 'pasa a TV (V3) y entra en standby',
    powerHint: '<b>Apagar</b> pone el ampli en espera (standby), como su botón STANDBY; no corta la corriente. El mando no sabe si el ampli está encendido.',
    input: 'Entrada', tvSub: 'entrada V3', inTv: 'Entrada TV', inDvd: 'Entrada DVD', inCd: 'Entrada CD', inTape: 'Entrada TAPE',
    volume: 'Volumen', volDown: 'Bajar volumen', volUp: 'Subir volumen',
    volHint: 'Mantén pulsado + o − para repetir. El volumen, el mute y el apagado solo responden con la entrada <b>TV (V3)</b> seleccionada.',
    auto: 'Automático con la tele', autoOn: 'Al encender o apagar la tele, el ampli se enciende en TV o se apaga solo.',
    autoOff: 'Desactivado: la tele no afecta al ampli. Úsalo para escuchar música sin la tele.',
    tech: 'Detalles técnicos', rSsid: 'Red WiFi', rSignal: 'Señal', rChannel: 'Canal', rIp: 'IP de la placa',
    rTvIp: 'IP de la tele', rTv: 'Tele', rAction: 'Última acción automática', rUptime: 'Tiempo encendida', rHeap: 'Memoria libre',
    restart: 'Reiniciar la placa', confirmRestart: '¿Reiniciar la placa? Tardará unos 15 segundos en volver.',
    restarting: 'Reiniciando… espera 15 s',
    log: 'Registro de eventos', logHint: 'Últimos 60 eventos. Los más antiguos se sustituyen solos y se borran al reiniciar. Los cambios de volumen no se anotan.',
    noEvents: 'Sin eventos todavía.', github: 'Código y manual en GitHub',
    tvOn: 'Tele encendida', tvOff: 'Tele apagada', tvUnknown: 'Tele ?', sent: 'Enviado: ', error: 'No se pudo enviar', offline: 'Sin conexión con la placa',
    signal: 'Señal ', sq: ['excelente', 'buena', 'regular', 'débil', 'muy débil'], followsTv: 'Auto con la tele', manual: 'Solo manual',
    never: 'ninguna desde el arranque', ago: 'hace ', units: ['d', 'h', 'min']
  }
};
const $ = id => document.getElementById(id);
const wanted = (new URLSearchParams(location.search).get('lang') || navigator.language || 'en').toLowerCase();
const lang = wanted.startsWith('es') ? 'es' : 'en';
const t = k => TEXT[lang][k] !== undefined ? TEXT[lang][k] : TEXT.en[k];
document.documentElement.lang = lang;
document.querySelectorAll('[data-i]').forEach(e => e.textContent = t(e.dataset.i));
document.querySelectorAll('[data-h]').forEach(e => e.innerHTML = t(e.dataset.h));
document.querySelectorAll('[data-a]').forEach(e => e.setAttribute('aria-label', t(e.dataset.a)));

let toastTimer;
function toast(msg){ const e = $('toast'); e.textContent = msg; e.style.opacity = 1; clearTimeout(toastTimer); toastTimer = setTimeout(() => e.style.opacity = 0, 1200); }
async function send(cmd, name, quiet){
  try{
    if(navigator.vibrate) navigator.vibrate(15);
    const r = await fetch('/cmd?c=' + cmd);
    if(!quiet) toast(r.ok ? t('sent') + (name ? t(name) : '') : t('error'));
    return r.ok;
  }catch(e){ toast(t('offline')); return false; }
}
async function toggleMute(){ if(await send('mute_toggle', '', true)) refresh(); }
async function toggleAuto(){ await fetch('/auto?v=' + ($('sw').classList.contains('act') ? 0 : 1)); refresh(); }
async function restart(){
  if(!confirm(t('confirmRestart'))) return;
  try{ await fetch('/reboot'); }catch(e){}
  toast(t('restarting'));
}
function duration(s){
  const d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60), u = t('units');
  return (d ? d + ' ' + u[0] + ' ' : '') + (h || d ? h + ' ' + u[1] + ' ' : '') + m + ' ' + u[2];
}
function quality(rssi){ return rssi > -55 ? 0 : rssi > -65 ? 1 : rssi > -75 ? 2 : rssi > -85 ? 3 : 4; }
async function refresh(){
  try{
    const s = await (await fetch('/status')).json();
    const q = quality(s.rssi), qName = t('sq')[q];
    $('model').textContent = s.model;
    $('tvTxt').textContent = s.tv === 'on' ? t('tvOn') : s.tv === 'off' ? t('tvOff') : t('tvUnknown');
    $('dotTv').classList.toggle('on', s.tv === 'on');
    $('bars').className = 'bars n' + (4 - q);
    $('wifiTxt').textContent = t('signal') + qName;
    $('dotSync').classList.toggle('on', s.sync);
    $('syncTxt').textContent = s.sync ? t('followsTv') : t('manual');
    $('sw').classList.toggle('act', s.sync);
    $('autoTxt').textContent = s.sync ? t('autoOn') : t('autoOff');
    $('mute').classList.toggle('act', s.mute);
    $('footFw').textContent = 'firmware ' + s.fw;
    if($('dTech').open){
      $('tSsid').textContent = s.ssid; $('tRssi').textContent = s.rssi + ' dBm (' + qName + ')';
      $('tCh').textContent = s.channel; $('tIp').textContent = s.ip; $('tMac').textContent = s.mac;
      $('tTvIp').textContent = s.tvIp; $('tTv').textContent = s.tv;
      $('tAct').textContent = s.ago < 0 ? t('never') : lang === 'es' ? t('ago') + duration(s.ago) : duration(s.ago) + t('ago');
      $('tUp').textContent = duration(s.uptime); $('tHeap').textContent = Math.round(s.heap / 1024) + ' KB';
      $('tFw').textContent = s.fw;
    }
    if($('dLog').open) $('lg').textContent = (await (await fetch('/log')).text()) || t('noEvents');
  }catch(e){ $('tvTxt').textContent = t('offline'); }
}
$('dTech').addEventListener('toggle', refresh);
$('dLog').addEventListener('toggle', async () => { await refresh(); const l = $('lg'); l.scrollTop = l.scrollHeight; });
// Holding + or - repeats the volume step every 0.4 s
document.querySelectorAll('.v').forEach(b => {
  let timer;
  const stop = () => clearInterval(timer);
  b.addEventListener('pointerdown', e => { e.preventDefault(); send(b.dataset.c, '', true); timer = setInterval(() => send(b.dataset.c, '', true), 400); });
  ['pointerup', 'pointerleave', 'pointercancel'].forEach(ev => b.addEventListener(ev, stop));
});
refresh(); setInterval(refresh, 4000);
</script>
</body></html>
)rawliteral";
