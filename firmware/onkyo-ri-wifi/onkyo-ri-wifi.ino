#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>

// =========================================================================
// 1. CONFIGURACIÓN: copia secrets.example.h como secrets.h y rellénalo
//    (WIFI_SSID, WIFI_PASSWORD, TV_IP y CLAVE_ADMIN)
// =========================================================================
#include "secrets.h"

const char* HOSTNAME = "onkyo-ri";

// =========================================================================
// 2. PIN Y CÓDIGOS ONKYO RI (TX-SR600E)
// =========================================================================
const int PIN_RI_OUT = D2; // Pin D2 del NodeMCU V3 (GPIO4)

// La tele va por el óptico 1, asignado a la entrada V3 del ampli: 0x1AF = encender + V3.
// En el SR600E solo CD (0x020), TAPE (0x070), DVD (0x120) y V3 (0x1A0) se pueden
// seleccionar por RI, y solo en V3 funcionan bien apagado (0x1AE) y volumen (0x1A2/3).
// OJO: 0x12E y 0x420 NO apagan el SR600E; 0x02F es "encender + CD".
uint16_t ONKYO_CMD_ON  = 0x1AF;
uint16_t ONKYO_CMD_OFF = 0x1AE;
const uint16_t ONKYO_CMD_INPUT_VIDEO3 = 0x1A0;
const unsigned long COOLDOWN_ACCION = 15000; // mín. entre dos acciones, evita bucles ON/OFF
unsigned long ultimaAccion = 0;

bool tvEncendidaAnterior = false;
bool primeraLectura = true;
bool silenciado = false;     // el ampli no informa de su estado: lo seguimos nosotros
bool sincronizarConTV = true; // false = modo manual: la tele no toca el ampli
unsigned long ultimoChequeo = 0;
int fallosConsecutivos = 0;
unsigned long ultimoWifiOk = 0;

ESP8266WebServer server(80);

// Comandos con nombre disponibles desde la web (/cmd?c=NOMBRE). Verificados en un TX-SR600E.
struct Cmd { const char* nombre; uint16_t codigo; };
const Cmd CMDS[] = {
  {"on_dvd", 0x12F}, {"on_cd", 0x02F}, {"on_tape", 0x07F}, {"on_v3", 0x1AF},
  {"in_cd", 0x020}, {"in_tape", 0x070}, {"in_dvd", 0x120}, {"in_v3", 0x1A0},
  {"vol_up", 0x1A2}, {"vol_down", 0x1A3},
};

// --- Registro circular, visible en /log y por serie ---
const int LOG_LINEAS = 40;
String logBuf[LOG_LINEAS];
int logPos = 0;

void logln(const String& msg) {
  String l = String(millis() / 1000) + "s  " + msg;
  Serial.println(l);
  logBuf[logPos] = l;
  logPos = (logPos + 1) % LOG_LINEAS;
}

// --- Protocolo nativo Onkyo RI para ESP8266 ---
void sendHeader() {
  digitalWrite(PIN_RI_OUT, HIGH);
  delayMicroseconds(3000);
  digitalWrite(PIN_RI_OUT, LOW);
  delayMicroseconds(1000);
}

void sendBit(bool bitVal) {
  digitalWrite(PIN_RI_OUT, HIGH);
  delayMicroseconds(1000);
  digitalWrite(PIN_RI_OUT, LOW);
  if (bitVal) {
    delayMicroseconds(2000);
  } else {
    delayMicroseconds(1000);
  }
}

void sendTrailer() {
  digitalWrite(PIN_RI_OUT, HIGH);
  delayMicroseconds(1000);
  digitalWrite(PIN_RI_OUT, LOW);
  delay(20);
}

void sendOnkyoRI(uint16_t command) {
  for (int repeat = 0; repeat < 2; repeat++) {
    sendHeader();
    for (int i = 11; i >= 0; i--) {
      bool bitVal = (command >> i) & 0x01;
      sendBit(bitVal);
    }
    sendTrailer();
    delay(10);
  }
}

void encenderOnkyo() {
  silenciado = false;
  sendOnkyoRI(ONKYO_CMD_ON);
}

// 0x1AE solo apaga si el ampli está en V3 (la entrada de la tele)
void apagarOnkyo() {
  silenciado = false;
  sendOnkyoRI(ONKYO_CMD_OFF);
  delay(500);
  sendOnkyoRI(ONKYO_CMD_OFF); // reintento (apagar es idempotente)
}

// Si el ampli está en otra entrada, primero hay que pasar a V3
void apagarOnkyoForzado() {
  sendOnkyoRI(ONKYO_CMD_INPUT_VIDEO3);
  delay(500);
  apagarOnkyo();
}

// --- Consulta el estado REAL de la pantalla Samsung Q70R (Puerto 8001) ---
// Devuelve: 1 = Pantalla ENCENDIDA ("on"), 0 = Pantalla en STANDBY o Apagada, -1 = sin WiFi
int comprobarEstadoSamsungTV() {
  if (WiFi.status() != WL_CONNECTED) return -1;

  WiFiClient client;
  HTTPClient http;
  client.setTimeout(1200); // Timeout rápido de 1.2 segundos

  String url = String("http://") + TV_IP + ":8001/api/v2/";
  if (http.begin(client, url)) {
    http.setTimeout(1200);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      http.end();
      // Si la tele está encendida con imagen, Tizen devuelve "PowerState":"on"
      if (payload.indexOf("\"PowerState\":\"on\"") != -1) {
        fallosConsecutivos = 0;
        return 1; // TV ENCENDIDA REAL
      }
      // Si la tele acaba de apagarse al mando o está en actualización nocturna, devuelve "standby"
      if (payload.indexOf("\"PowerState\":\"standby\"") != -1) {
        fallosConsecutivos = 2;
        return 0; // TV EN STANDBY INMEDIATO
      }
    }
    http.end();
  }

  // Si no responde al puerto 8001, contamos 2 intentos fallidos (4 seg) para confirmar apagado
  fallosConsecutivos++;
  if (fallosConsecutivos >= 2) {
    return 0; // TV APAGADA
  }
  return tvEncendidaAnterior ? 1 : 0;
}

// =========================================================================
// 3. WEB DE CONTROL (usuario "admin" + CLAVE_ADMIN)
// =========================================================================
const char PAGINA[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#14161a">
<title>Onkyo</title>
<style>
:root{--bg:#14161a;--card:#1e2127;--btn:#2a2e36;--txt:#e8eaee;--mut:#8b92a0;--acc:#4da3ff;--ok:#2ecc71;--bad:#e5484d}
@media (prefers-color-scheme:light){:root{--bg:#e9ebef;--card:#fff;--btn:#eef0f4;--txt:#1b1e24;--mut:#6b7280}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent;-webkit-user-select:none;user-select:none}
body{margin:0;background:var(--bg);color:var(--txt);font-family:system-ui,-apple-system,sans-serif;display:flex;justify-content:center;min-height:100vh}
.mando{width:100%;max-width:360px;margin:16px;padding:22px 18px 26px;background:var(--card);border-radius:36px;box-shadow:0 8px 30px #0005;align-self:flex-start}
.marca{text-align:center;letter-spacing:.4em;font-weight:700;font-size:13px;color:var(--mut)}
.estado{text-align:center;font-size:12px;color:var(--mut);margin:6px 0 18px;min-height:16px}
button{border:0;border-radius:18px;background:var(--btn);color:var(--txt);font:inherit;font-size:15px;font-weight:600;padding:14px 0;cursor:pointer;transition:transform .08s,background .15s;touch-action:manipulation}
button:active{transform:scale(.94);background:var(--acc);color:#fff}
.pwr{display:flex;gap:12px}
.pwr button{flex:1;height:64px;border-radius:32px;font-size:16px}
.pwr .on{color:var(--ok)}.pwr .off{color:var(--bad)}
.entradas{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin:18px 0 22px}
.vol{display:flex;align-items:center;justify-content:space-between;gap:10px}
.vol .v{width:88px;height:88px;border-radius:50%;font-size:38px;font-weight:300;padding:0}
.vol .m{flex:1;height:44px;font-size:13px;padding:0}
.vol .m.act{background:var(--bad);color:#fff}
.auto{display:flex;align-items:center;justify-content:space-between;margin:24px 4px 0;font-size:14px}
.sw{width:52px;height:30px;border-radius:15px;padding:0;position:relative}
.sw::after{content:"";position:absolute;top:3px;left:3px;width:24px;height:24px;border-radius:50%;background:var(--mut);transition:transform .2s,background .2s}
.sw.act{background:var(--ok)}.sw.act::after{transform:translateX(22px);background:#fff}
.nota{font-size:11.5px;line-height:1.5;color:var(--mut);margin:16px 4px 0}
.nota b{color:var(--txt);font-weight:600}
.nota .link{all:unset;color:var(--acc);cursor:pointer;text-decoration:underline}
#toast{position:fixed;bottom:22px;left:50%;transform:translateX(-50%);background:#000c;color:#fff;padding:8px 16px;border-radius:14px;font-size:13px;opacity:0;transition:opacity .2s;pointer-events:none}
</style></head><body>
<div class="mando">
  <div class="marca">ONKYO</div>
  <div class="estado" id="st">Conectando...</div>

  <div class="pwr">
    <button class="on" onclick="c('on')">Encender</button>
    <button class="off" onclick="c('off')">Apagar</button>
  </div>

  <div class="entradas">
    <button onclick="c('in_v3')">V3 &middot; TV</button>
    <button onclick="c('in_dvd')">DVD</button>
    <button onclick="c('in_cd')">CD</button>
    <button onclick="c('in_tape')">TAPE</button>
  </div>

  <div class="vol">
    <button class="v" data-c="vol_down">&minus;</button>
    <button class="m" id="mute" onclick="mute()">MUTE</button>
    <button class="v" data-c="vol_up">+</button>
  </div>

  <div class="auto">
    <span>Sincronizar con la tele</span>
    <button class="sw" id="sw" onclick="auto()" aria-label="Sincronizar con la tele"></button>
  </div>

  <p class="nota">
    <b>Avisos.</b> Al encender la tele, el ampli cambia a la entrada <b>V3</b>; desactiva la
    sincronización para escuchar otras fuentes sin interrupciones. Si el ampli est&aacute; en
    otra entrada no se apaga solo: <button class="link" onclick="c('off_forzado')">apagar desde cualquier entrada</button>.
    El mando no sabe si el ampli est&aacute; encendido.
  </p>
</div>
<div id="toast"></div>
<script>
const $ = id => document.getElementById(id);
let tt;
function toast(t){ const e=$('toast'); e.textContent=t; e.style.opacity=1; clearTimeout(tt); tt=setTimeout(()=>e.style.opacity=0,1100); }
async function c(n, callado){
  try{
    if(navigator.vibrate) navigator.vibrate(15);
    const r = await fetch('/cmd?c='+n);
    if(!callado) toast(r.ok ? 'Enviado' : 'Error');
    return r.ok;
  }catch(e){ toast('Sin conexión con la placa'); return false; }
}
async function mute(){ if(await c('mute_toggle', true)) estado(); }
async function auto(){ await fetch('/auto?v='+($('sw').classList.contains('act')?0:1)); estado(); }
async function estado(){
  try{
    const s = await (await fetch('/status')).json();
    const wifi = s.rssi > -60 ? 'buena' : s.rssi > -75 ? 'regular' : 'débil';
    $('st').textContent = 'Tele ' + s.tv + ' · WiFi ' + wifi;
    $('sw').classList.toggle('act', s.auto);
    $('mute').classList.toggle('act', s.mute);
  }catch(e){ $('st').textContent = 'Sin conexión con la placa'; }
}
// Mantener pulsado +/- repite cada 0,4 s
document.querySelectorAll('.v').forEach(b => {
  let t;
  const parar = () => clearInterval(t);
  b.addEventListener('pointerdown', e => { e.preventDefault(); c(b.dataset.c, true); t=setInterval(()=>c(b.dataset.c, true), 400); });
  ['pointerup','pointerleave','pointercancel'].forEach(ev => b.addEventListener(ev, parar));
});
estado(); setInterval(estado, 4000);
</script>
</body></html>
)rawliteral";

bool autorizado() {
  if (!server.authenticate("admin", CLAVE_ADMIN)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

void handleRoot() {
  if (!autorizado()) return;
  server.send_P(200, "text/html; charset=utf-8", PAGINA);
}

void handleStatus() {
  if (!autorizado()) return;
  String tv = primeraLectura ? "?" : (tvEncendidaAnterior ? "encendida" : "apagada");
  String j = String("{\"tv\":\"") + tv + "\",\"auto\":" + (sincronizarConTV ? "true" : "false") +
             ",\"ip\":\"" + WiFi.localIP().toString() + "\",\"rssi\":" + WiFi.RSSI() +
             ",\"mute\":" + (silenciado ? "true" : "false") +
             ",\"uptime\":" + (millis() / 1000) + "}";
  server.send(200, "application/json", j);
}

void handleLog() {
  if (!autorizado()) return;
  String out;
  for (int i = 0; i < LOG_LINEAS; i++) {
    const String& l = logBuf[(logPos + i) % LOG_LINEAS];
    if (l.length()) out += l + "\n";
  }
  server.send(200, "text/plain; charset=utf-8", out);
}

void handleCmd() {
  if (!autorizado()) return;
  String nombre = server.arg("c");
  if (nombre == "on") {
    logln("WEB: encender (0x1AF)");
    encenderOnkyo();
    server.send(200, "text/plain", "OK on");
    return;
  }
  if (nombre == "off") {
    logln("WEB: apagar (0x1AE)");
    apagarOnkyo();
    server.send(200, "text/plain", "OK off");
    return;
  }
  if (nombre == "mute_toggle") {
    silenciado = !silenciado;
    logln(String("WEB: ") + (silenciado ? "silenciar (0x1A4)" : "quitar silencio (0x1A5)"));
    sendOnkyoRI(silenciado ? 0x1A4 : 0x1A5);
    server.send(200, "text/plain", silenciado ? "mute=1" : "mute=0");
    return;
  }
  if (nombre == "off_forzado") {
    logln("WEB: apagar forzado (0x1A0 + 0x1AE)");
    apagarOnkyoForzado();
    server.send(200, "text/plain", "OK off_forzado");
    return;
  }
  for (const Cmd& cmd : CMDS) {
    if (nombre == cmd.nombre) {
      logln("WEB: " + nombre + " (0x" + String(cmd.codigo, HEX) + ")");
      sendOnkyoRI(cmd.codigo);
      server.send(200, "text/plain", "OK " + nombre);
      return;
    }
  }
  server.send(400, "text/plain", "Comando desconocido");
}

// /raw?code=1A0 -> manda un código en hex. Solo 0x001-0x1FF: los 0x4xx son códigos
// de test/servicio que en otros Onkyo borran ajustes.
void handleRaw() {
  if (!autorizado()) return;
  long codigo = strtol(server.arg("code").c_str(), NULL, 16);
  if (codigo < 0x001 || codigo > 0x1FF) {
    server.send(400, "text/plain", "Codigo fuera de rango (0x001-0x1FF)");
    return;
  }
  logln("WEB: raw 0x" + String(codigo, HEX));
  sendOnkyoRI((uint16_t)codigo);
  server.send(200, "text/plain", "OK raw 0x" + String(codigo, HEX));
}

void handleAuto() {
  if (!autorizado()) return;
  sincronizarConTV = server.arg("v") != "0";
  logln(sincronizarConTV ? "WEB: sincronizar con la tele ACTIVADO" : "WEB: sincronizar con la tele DESACTIVADO (modo manual)");
  server.send(200, "text/plain", sincronizarConTV ? "auto=1" : "auto=0");
}

void setup() {
  pinMode(PIN_RI_OUT, OUTPUT);
  digitalWrite(PIN_RI_OUT, LOW);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // En NodeMCU el LED va invertido (HIGH = apagado)

  Serial.begin(115200);
  delay(500);
  logln("=== CONTROL ONKYO RI POR WIFI (SAMSUNG Q70R) ===");

  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando al WiFi");
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 30) {
    delay(500);
    Serial.print(".");
    intentos++;
  }
  logln("WiFi " + String(WiFi.status() == WL_CONNECTED ? "conectado" : "SIN conexion") +
        ", IP del NodeMCU: " + WiFi.localIP().toString());
  ultimoWifiOk = millis();

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(CLAVE_ADMIN);
  ArduinoOTA.onStart([]() { logln("OTA: actualizando..."); });
  ArduinoOTA.onEnd([]() { logln("OTA: terminado, reiniciando"); });
  ArduinoOTA.onError([](ota_error_t e) { logln("OTA: error " + String((int)e)); });
  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/log", handleLog);
  server.on("/cmd", handleCmd);
  server.on("/raw", handleRaw);
  server.on("/auto", handleAuto);
  server.begin();
  logln("Web y OTA listas en http://" + String(HOSTNAME) + ".local/");
  if (strcmp(CLAVE_ADMIN, "change-me") == 0) {
    logln("AVISO: estas usando la clave por defecto. Cambiala en secrets.h");
  }
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  // Si el WiFi lleva más de 60 s caído, forzamos la reconexión
  if (WiFi.status() == WL_CONNECTED) {
    ultimoWifiOk = millis();
  } else if (millis() - ultimoWifiOk > 60000) {
    logln("WiFi caido, reconectando...");
    WiFi.reconnect();
    ultimoWifiOk = millis();
  }

  // 1. MODO PRUEBA MANUAL POR MONITOR SERIE (opcional, con cable)
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '1') {
      logln("Test 1: ON V3 (0x1AF) -> espera 4s -> apagado");
      encenderOnkyo(); delay(4000); apagarOnkyo();
    } else if (c == '2') {
      logln("Test 2: ON V3 (0x1AF)");
      encenderOnkyo();
    } else if (c == '3') {
      logln("Test 3: apagado (0x1AE)");
      apagarOnkyo();
    }
  }

  // 2. VIGILANCIA AUTOMÁTICA DE LA SAMSUNG Q70R CADA 2.5 SEGUNDOS
  if (millis() - ultimoChequeo >= 2500) {
    ultimoChequeo = millis();
    int estado = comprobarEstadoSamsungTV();

    if (estado != -1) {
      bool tvEncendidaAhora = (estado == 1);

      if (primeraLectura) {
        primeraLectura = false;
        tvEncendidaAnterior = tvEncendidaAhora;
        digitalWrite(LED_BUILTIN, tvEncendidaAhora ? LOW : HIGH);
        logln(tvEncendidaAhora ? "Estado inicial TV: ENCENDIDA" : "Estado inicial TV: APAGADA");
      }
      else if (tvEncendidaAhora != tvEncendidaAnterior &&
               (ultimaAccion == 0 || millis() - ultimaAccion >= COOLDOWN_ACCION)) {
        tvEncendidaAnterior = tvEncendidaAhora;
        ultimaAccion = millis();
        digitalWrite(LED_BUILTIN, tvEncendidaAhora ? LOW : HIGH);

        if (!sincronizarConTV) {
          logln(tvEncendidaAhora ? "TV encendida (modo manual: sin accion)" : "TV apagada (modo manual: sin accion)");
        } else if (tvEncendidaAhora) {
          logln(">>> TV ENCENDIDA DETECTADA -> Encendiendo Onkyo!");
          encenderOnkyo();
        } else {
          logln(">>> TV APAGADA DETECTADA -> Apagando Onkyo a Standby!");
          apagarOnkyo();
        }
      }
    }
  }
}
