// onkyo-ri-wifi: ESP8266 firmware that
//  - sends Onkyo RI (Remote Interactive) codes to a receiver over its RI jack,
//  - follows a Samsung (Tizen) TV: TV on -> receiver on, TV off -> receiver off,
//  - serves a phone-friendly web remote and accepts OTA updates over WiFi.
// https://github.com/lhupyn/onkyo-ri-wifi

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>

#include "config.h"   // copy config.example.h to config.h and fill it in
#include "webpage.h"  // the web remote (PAGE)

const char* HOSTNAME   = "onkyo-ri";
const char* FW_VERSION = "1.2.0";

// =========================================================================
// 1. HARDWARE
// =========================================================================
const int PIN_RI_OUT = D2;  // NodeMCU D2 (GPIO4) -> RI jack tip. GND -> RI jack sleeve.

// =========================================================================
// 2. RI CODES. These are specific to the receiver model.
//    Verified on an Onkyo TX-SR600E, where the TV is on the V3 (GAME/TV) input.
//    On this model power off, volume and mute are only accepted while that
//    input is selected, and only CD, TAPE, DVD and V3 can be selected over RI.
//    Never use 0x420-0x43F: they are test/service codes on several Onkyo models.
// =========================================================================
const uint16_t RI_POWER_ON    = 0x1AF;  // power on + select the TV input
const uint16_t RI_POWER_OFF   = 0x1AE;  // power off (only while the TV input is selected)
const uint16_t RI_SELECT_TV   = 0x1A0;  // select the TV input (V3)
const uint16_t RI_SELECT_DVD  = 0x120;
const uint16_t RI_SELECT_CD   = 0x020;
const uint16_t RI_SELECT_TAPE = 0x070;
const uint16_t RI_VOLUME_UP   = 0x1A2;
const uint16_t RI_VOLUME_DOWN = 0x1A3;
const uint16_t RI_MUTE        = 0x1A4;
const uint16_t RI_UNMUTE      = 0x1A5;

// Commands the web page can send by name: /cmd?c=NAME
struct NamedCode { const char* name; uint16_t code; };
const NamedCode NAMED_CODES[] = {
  {"in_tv", RI_SELECT_TV}, {"in_dvd", RI_SELECT_DVD}, {"in_cd", RI_SELECT_CD}, {"in_tape", RI_SELECT_TAPE},
  {"vol_up", RI_VOLUME_UP}, {"vol_down", RI_VOLUME_DOWN},
};

// =========================================================================
// 3. BEHAVIOUR
// =========================================================================
const unsigned long TV_POLL_MS   = 2500;   // how often the TV is asked for its power state
const unsigned long COOLDOWN_MS  = 15000;  // minimum time between two automatic actions (avoids loops)
const unsigned long WIFI_RETRY_MS = 60000; // force a reconnect after this long without WiFi

bool tvOn = false;             // last TV state we acted on
bool firstReading = true;      // the first reading never triggers an action
bool followTv = true;          // false = manual mode: the TV does not touch the receiver
bool muted = false;            // the receiver does not report its state, so we track it
unsigned long lastPoll = 0;
unsigned long lastAction = 0;  // millis() of the last automatic action (0 = none yet)
unsigned long lastWifiOk = 0;
int tvFailures = 0;

ESP8266WebServer server(80);

// --- Circular event log, shown at /log and on the serial port ---
const int LOG_LINES = 60;
String logBuf[LOG_LINES];
int logPos = 0;

void logLine(const String& msg) {
  String line = String(millis() / 1000) + "s  " + msg;
  Serial.println(line);
  logBuf[logPos] = line;
  logPos = (logPos + 1) % LOG_LINES;
}

// --- Onkyo RI protocol: 12-bit code, MSB first, sent twice ---
void sendHeader() {
  digitalWrite(PIN_RI_OUT, HIGH);
  delayMicroseconds(3000);
  digitalWrite(PIN_RI_OUT, LOW);
  delayMicroseconds(1000);
}

void sendBit(bool bitValue) {
  digitalWrite(PIN_RI_OUT, HIGH);
  delayMicroseconds(1000);
  digitalWrite(PIN_RI_OUT, LOW);
  delayMicroseconds(bitValue ? 2000 : 1000);
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
    for (int i = 11; i >= 0; i--) sendBit((command >> i) & 0x01);
    sendTrailer();
    delay(10);
  }
}

void powerOn() {
  muted = false;
  sendOnkyoRI(RI_POWER_ON);
}

// The receiver ignores the off code unless the TV input is selected, so from any
// other input it stays on. That is on purpose: turning the TV off must not cut
// the music you are playing from another source.
void powerOff() {
  muted = false;
  sendOnkyoRI(RI_POWER_OFF);
  delay(500);
  sendOnkyoRI(RI_POWER_OFF);  // repeat once (power off is idempotent)
}

// Switch to the TV input first so the off code is accepted from any input.
void powerOffFromAnyInput() {
  sendOnkyoRI(RI_SELECT_TV);
  delay(500);
  powerOff();
}

// --- Samsung (Tizen) TV power state, from http://<tv>:8001/api/v2/ ---
// Returns 1 = on, 0 = standby or off, -1 = no WiFi.
int readTvState() {
  if (WiFi.status() != WL_CONNECTED) return -1;

  WiFiClient client;
  HTTPClient http;
  client.setTimeout(1200);

  String url = String("http://") + TV_IP + ":8001/api/v2/";
  if (http.begin(client, url)) {
    http.setTimeout(1200);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      http.end();
      if (payload.indexOf("\"PowerState\":\"on\"") != -1) {
        tvFailures = 0;
        return 1;
      }
      // Just switched off with the remote, or running a nightly update
      if (payload.indexOf("\"PowerState\":\"standby\"") != -1) {
        tvFailures = 2;
        return 0;
      }
    }
    http.end();
  }

  // No answer: after 2 failed polls in a row (about 5 s) we treat the TV as off
  tvFailures++;
  if (tvFailures >= 2) return 0;
  return tvOn ? 1 : 0;
}

// =========================================================================
// 4. WEB REMOTE (user "admin" + ADMIN_PASSWORD, unless WEB_AUTH is false)
// =========================================================================
bool isAuthorized() {
  if (!WEB_AUTH) return true;
  if (!server.authenticate("admin", ADMIN_PASSWORD)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

String jsonEscape(const String& text) {
  String out;
  for (unsigned int i = 0; i < text.length(); i++) {
    char ch = text[i];
    if (ch == '"' || ch == '\\') out += '\\';
    out += ch;
  }
  return out;
}

void handleRoot() {
  if (!isAuthorized()) return;
  server.send_P(200, "text/html; charset=utf-8", PAGE);
}

void handleStatus() {
  if (!isAuthorized()) return;
  String tv = firstReading ? "?" : (tvOn ? "on" : "off");
  long ago = lastAction == 0 ? -1 : (long)((millis() - lastAction) / 1000);
  String json = String("{\"tv\":\"") + tv + "\",\"sync\":" + (followTv ? "true" : "false") +
                ",\"mute\":" + (muted ? "true" : "false") +
                ",\"model\":\"" + jsonEscape(RECEIVER_NAME) + "\"" +
                ",\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\",\"rssi\":" + WiFi.RSSI() +
                ",\"channel\":" + WiFi.channel() + ",\"ip\":\"" + WiFi.localIP().toString() +
                "\",\"mac\":\"" + WiFi.macAddress() + "\",\"tvIp\":\"" + TV_IP + "\"" +
                ",\"ago\":" + ago + ",\"uptime\":" + (millis() / 1000) +
                ",\"heap\":" + ESP.getFreeHeap() + ",\"fw\":\"" + FW_VERSION + "\"}";
  server.send(200, "application/json", json);
}

void handleLog() {
  if (!isAuthorized()) return;
  String out;
  for (int i = 0; i < LOG_LINES; i++) {
    const String& line = logBuf[(logPos + i) % LOG_LINES];
    if (line.length()) out += line + "\n";
  }
  server.send(200, "text/plain; charset=utf-8", out);
}

void handleCmd() {
  if (!isAuthorized()) return;
  String name = server.arg("c");
  if (name == "on") {
    logLine("WEB: power on");
    powerOn();
  } else if (name == "off") {
    logLine("WEB: power off");
    powerOff();
  } else if (name == "off_any_input") {
    logLine("WEB: power off from any input");
    powerOffFromAnyInput();
  } else if (name == "mute_toggle") {
    muted = !muted;
    logLine(muted ? "WEB: mute" : "WEB: unmute");
    sendOnkyoRI(muted ? RI_MUTE : RI_UNMUTE);
  } else {
    for (const NamedCode& entry : NAMED_CODES) {
      if (name == entry.name) {
        if (!name.startsWith("vol_")) logLine("WEB: " + name + " (0x" + String(entry.code, HEX) + ")");
        sendOnkyoRI(entry.code);
        server.send(200, "text/plain", "OK " + name);
        return;
      }
    }
    server.send(400, "text/plain", "Unknown command");
    return;
  }
  server.send(200, "text/plain", "OK " + name);
}

// /raw?code=1A0 sends any code in hex, limited to 0x001-0x1FF: the 0x4xx codes are
// test/service codes that open menus, make noise or clear settings on some models.
void handleRaw() {
  if (!isAuthorized()) return;
  long code = strtol(server.arg("code").c_str(), NULL, 16);
  if (code < 0x001 || code > 0x1FF) {
    server.send(400, "text/plain", "Code out of range (0x001-0x1FF)");
    return;
  }
  logLine("WEB: raw 0x" + String(code, HEX));
  sendOnkyoRI((uint16_t)code);
  server.send(200, "text/plain", "OK raw 0x" + String(code, HEX));
}

void handleAuto() {
  if (!isAuthorized()) return;
  followTv = server.arg("v") != "0";
  logLine(followTv ? "WEB: follow the TV ON" : "WEB: follow the TV OFF (manual mode)");
  server.send(200, "text/plain", followTv ? "auto=1" : "auto=0");
}

void handleReboot() {
  if (!isAuthorized()) return;
  logLine("WEB: restarting the board");
  server.send(200, "text/plain", "OK restarting");
  delay(300);
  ESP.restart();
}

// =========================================================================
// 5. SETUP AND MAIN LOOP
// =========================================================================
void setup() {
  pinMode(PIN_RI_OUT, OUTPUT);
  digitalWrite(PIN_RI_OUT, LOW);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // the NodeMCU LED is inverted: HIGH = off

  Serial.begin(115200);
  delay(500);
  logLine(String("=== onkyo-ri-wifi ") + FW_VERSION + " ===");

  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  for (int attempt = 0; WiFi.status() != WL_CONNECTED && attempt < 30; attempt++) {
    delay(500);
    Serial.print(".");
  }
  logLine(String("WiFi ") + (WiFi.status() == WL_CONNECTED ? "connected" : "NOT connected") +
          ", board IP: " + WiFi.localIP().toString());
  lastWifiOk = millis();

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(ADMIN_PASSWORD);
  ArduinoOTA.onStart([]() { logLine("OTA: updating..."); });
  ArduinoOTA.onEnd([]() { logLine("OTA: done, restarting"); });
  ArduinoOTA.onError([](ota_error_t error) { logLine("OTA: error " + String((int)error)); });
  ArduinoOTA.begin();

  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/log", handleLog);
  server.on("/cmd", handleCmd);
  server.on("/raw", handleRaw);
  server.on("/auto", handleAuto);
  server.on("/reboot", handleReboot);
  server.begin();
  logLine("Web remote and OTA ready at http://" + String(HOSTNAME) + ".local/");
  if (strcmp(ADMIN_PASSWORD, "change-me") == 0) logLine("WARNING: default password in use, change it in config.h");
  if (!WEB_AUTH) logLine("WARNING: the web remote has no password (WEB_AUTH is false)");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  if (WiFi.status() == WL_CONNECTED) {
    lastWifiOk = millis();
  } else if (millis() - lastWifiOk > WIFI_RETRY_MS) {
    logLine("WiFi lost, reconnecting...");
    WiFi.reconnect();
    lastWifiOk = millis();
  }

  // Serial test commands (optional, with the USB cable): 1 = on, then off after 4 s; 2 = on; 3 = off
  if (Serial.available() > 0) {
    char key = Serial.read();
    if (key == '1') {
      logLine("Test 1: power on, then off after 4 s");
      powerOn(); delay(4000); powerOff();
    } else if (key == '2') {
      logLine("Test 2: power on");
      powerOn();
    } else if (key == '3') {
      logLine("Test 3: power off");
      powerOff();
    }
  }

  // Follow the TV: act only when its power state changes, and never twice within COOLDOWN_MS
  if (millis() - lastPoll >= TV_POLL_MS) {
    lastPoll = millis();
    int state = readTvState();
    if (state == -1) return;
    bool tvNow = (state == 1);

    if (firstReading) {
      firstReading = false;
      tvOn = tvNow;
      digitalWrite(LED_BUILTIN, tvNow ? LOW : HIGH);
      logLine(tvNow ? "Initial TV state: ON" : "Initial TV state: OFF");
    } else if (tvNow != tvOn && (lastAction == 0 || millis() - lastAction >= COOLDOWN_MS)) {
      tvOn = tvNow;
      lastAction = millis();
      digitalWrite(LED_BUILTIN, tvNow ? LOW : HIGH);

      if (!followTv) {
        logLine(tvNow ? "TV turned on (manual mode: no action)" : "TV turned off (manual mode: no action)");
      } else if (tvNow) {
        logLine(">>> TV turned on -> powering the receiver on");
        powerOn();
      } else {
        logLine(">>> TV turned off -> powering the receiver off");
        powerOff();
      }
    }
  }
}
