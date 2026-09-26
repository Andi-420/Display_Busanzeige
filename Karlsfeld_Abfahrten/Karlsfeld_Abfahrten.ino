/*
  Abfahrtsmonitor Karlsfeld Rathausstraße
  Hardware: ESP32-2432S028R ("Cheap Yellow Display"), ILI9341 320x240, XPT2046 Touch

  Datenquelle: MVG-API (deckt das gesamte MVV-Gebiet inkl. Regionalbusse ab)
    Haltestellensuche: https://www.mvg.de/api/bgw-pt/v3/locations?query=...
    Abfahrten:         https://www.mvg.de/api/bgw-pt/v3/departures?globalId=...

  Benötigte Bibliotheken (Arduino Bibliotheksverwalter):
    - TFT_eSPI (Bodmer)            -> User_Setup.h aus dem Ordner TFT_eSPI_Setup verwenden!
    - XPT2046_Touchscreen (Paul Stoffregen)
    - ArduinoJson (Benoit Blanchon), Version 7.x

  Board: "ESP32 Dev Module"
*/

#define ARDUINOJSON_USE_LONG_LONG 1

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <time.h>

#include "config.h"

// ---------- Touch-Pins des CYD (eigener SPI-Bus) ----------
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// ---------- Layout ----------
#define SCREEN_W   320
#define SCREEN_H   240
#define HEADER_H   32
#define FOOTER_H   18
#define ROW_H      27
#define ROWS       ((SCREEN_H - HEADER_H - FOOTER_H) / ROW_H)   // = 7

// ---------- Farben ----------
#define COL_BG        TFT_BLACK
#define COL_HEADER    0x032F   // MVV-Blau
#define COL_TEXT      TFT_WHITE
#define COL_DIM       0x8410   // grau
#define COL_ONTIME    0x07E0   // grün
#define COL_DELAY     0xFD20   // orange
#define COL_CANCEL    TFT_RED
#define COL_ROWLINE   0x2104

const char *TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";
const char *API_BASE = "https://www.mvg.de/api/bgw-pt/v3";

TFT_eSPI tft;
TFT_eSprite rowSprite(&tft);
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen touch(XPT2046_CS, XPT2046_IRQ);

struct Departure {
  char   line[8];
  char   destination[48];
  char   type[16];
  time_t departure;    // Echtzeit (falls vorhanden), sonst Plan
  int    delay;        // Minuten
  bool   realtime;
  bool   cancelled;
};

Departure departures[MAX_DEPARTURES];
int  departureCount = 0;
String stationId = STATION_GLOBAL_ID;
String lastError = "";
time_t lastSuccess = 0;
unsigned long lastFetchMs = 0;
unsigned long lastDrawMs  = 0;
bool forceFetch = true;

// =====================================================================
// Hilfsfunktionen
// =====================================================================

// Umlaute ersetzen, da die eingebauten Fonts nur ASCII können.
String asciiFy(const char *in) {
  String out;
  for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
    if (*p == 0xC3 && p[1]) {
      p++;
      switch (*p) {
        case 0xA4: out += "ae"; break;
        case 0xB6: out += "oe"; break;
        case 0xBC: out += "ue"; break;
        case 0x84: out += "Ae"; break;
        case 0x96: out += "Oe"; break;
        case 0x9C: out += "Ue"; break;
        case 0x9F: out += "ss"; break;
        default:   out += '?';  break;
      }
    } else if (*p < 0x80) {
      out += (char)*p;
    }
    // andere Multibyte-Zeichen werden ignoriert
  }
  return out;
}

String urlEncode(const char *s) {
  const char *hex = "0123456789ABCDEF";
  String out;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
      out += (char)*p;
    } else {
      out += '%';
      out += hex[*p >> 4];
      out += hex[*p & 0x0F];
    }
  }
  return out;
}

bool lineAllowed(const char *line) {
  const char *filter = LINE_FILTER;
  if (!filter[0]) return true;
  String f = String(",") + filter + ",";
  f.replace(" ", "");
  return f.indexOf(String(",") + line + ",") >= 0;
}

uint16_t colorForType(const char *type) {
  if (!strcmp(type, "SBAHN"))  return 0x05A6;   // S-Bahn grün
  if (!strcmp(type, "UBAHN"))  return 0x0339;   // U-Bahn blau
  if (!strcmp(type, "TRAM"))   return 0xE8E4;   // Tram rot
  if (!strcmp(type, "BAHN"))   return 0x7BEF;   // Regionalzug grau
  return 0x03AE;                                // Bus / Regionalbus petrol
}

bool timeValid() {
  return time(nullptr) > 1700000000;
}

String clockString(time_t t, bool withSeconds = false) {
  struct tm tmv;
  localtime_r(&t, &tmv);
  char buf[12];
  strftime(buf, sizeof(buf), withSeconds ? "%H:%M:%S" : "%H:%M", &tmv);
  return String(buf);
}

// =====================================================================
// HTTP / API
// =====================================================================

bool httpGetJson(const String &url, JsonDocument &doc, JsonDocument &filter) {
  WiFiClientSecure client;
  client.setInsecure();   // Zertifikat wird nicht geprüft (einfacher, reicht hier)

  HTTPClient http;
  http.useHTTP10(true);   // verhindert "chunked" Antworten -> direktes Stream-Parsing
  http.setTimeout(15000);
  if (!http.begin(client, url)) {
    lastError = "Verbindung fehlgeschlagen";
    return false;
  }
  http.setUserAgent("ESP32-Abfahrtsmonitor/1.0");
  http.addHeader("Accept", "application/json");

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    lastError = "HTTP-Fehler " + String(code);
    Serial.printf("GET %s -> %d\n", url.c_str(), code);
    http.end();
    return false;
  }

  DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    lastError = String("JSON: ") + err.c_str();
    Serial.println(lastError);
    return false;
  }
  return true;
}

bool findStation() {
  String url = String(API_BASE) + "/locations?query=" + urlEncode(STATION_QUERY) +
               "&locationTypes=STATION";

  JsonDocument filter;
  filter[0]["type"]     = true;
  filter[0]["name"]     = true;
  filter[0]["place"]    = true;
  filter[0]["globalId"] = true;

  JsonDocument doc;
  if (!httpGetJson(url, doc, filter)) return false;

  JsonArray arr = doc.as<JsonArray>();
  String fallback = "";
  for (JsonObject loc : arr) {
    const char *type = loc["type"] | "";
    const char *id   = loc["globalId"] | "";
    const char *name = loc["name"] | "";
    const char *place = loc["place"] | "";
    if (strcmp(type, "STATION") != 0 || !id[0]) continue;

    Serial.printf("Gefunden: %s, %s -> %s\n", place, name, id);
    if (fallback == "") fallback = id;
    if (strstr(place, "Karlsfeld") && strstr(name, "Rathaus")) {
      stationId = id;
      return true;
    }
  }
  if (fallback != "") {
    stationId = fallback;
    return true;
  }
  lastError = "Haltestelle nicht gefunden";
  return false;
}

bool fetchDepartures() {
  String url = String(API_BASE) + "/departures?globalId=" + urlEncode(stationId.c_str()) +
               "&limit=" + String(MAX_DEPARTURES) +
               "&offsetInMinutes=" + String(WALK_MINUTES);

  JsonDocument filter;
  filter[0]["label"]                 = true;
  filter[0]["destination"]           = true;
  filter[0]["transportType"]         = true;
  filter[0]["plannedDepartureTime"]  = true;
  filter[0]["realtimeDepartureTime"] = true;
  filter[0]["delayInMinutes"]        = true;
  filter[0]["realtime"]              = true;
  filter[0]["cancelled"]             = true;

  JsonDocument doc;
  if (!httpGetJson(url, doc, filter)) return false;

  int n = 0;
  for (JsonObject d : doc.as<JsonArray>()) {
    if (n >= MAX_DEPARTURES) break;
    const char *line = d["label"] | "?";
    if (!lineAllowed(line)) continue;

    Departure &dep = departures[n];
    strlcpy(dep.line, line, sizeof(dep.line));
    strlcpy(dep.destination, asciiFy(d["destination"] | "").c_str(), sizeof(dep.destination));
    strlcpy(dep.type, d["transportType"] | "BUS", sizeof(dep.type));

    long long planned  = d["plannedDepartureTime"]  | 0LL;
    long long realtime = d["realtimeDepartureTime"] | 0LL;
    dep.realtime  = d["realtime"] | false;
    dep.cancelled = d["cancelled"] | false;
    dep.delay     = d["delayInMinutes"] | 0;
    dep.departure = (time_t)(((dep.realtime && realtime > 0) ? realtime : planned) / 1000LL);
    n++;
  }

  // nach Abfahrtszeit sortieren
  for (int i = 1; i < n; i++) {
    Departure tmp = departures[i];
    int j = i - 1;
    while (j >= 0 && departures[j].departure > tmp.departure) {
      departures[j + 1] = departures[j];
      j--;
    }
    departures[j + 1] = tmp;
  }

  departureCount = n;
  lastSuccess = time(nullptr);
  lastError = "";
  Serial.printf("%d Abfahrten geladen\n", n);
  return true;
}

// =====================================================================
// Anzeige
// =====================================================================

void drawHeader(bool full) {
  if (full) {
    tft.fillRect(0, 0, SCREEN_W, HEADER_H, COL_HEADER);
    tft.setTextColor(COL_TEXT, COL_HEADER);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(STATION_TITLE, 6, HEADER_H / 2, 4);
  }
  if (timeValid()) {
    tft.setTextColor(COL_TEXT, COL_HEADER);
    tft.setTextDatum(MR_DATUM);
    tft.setTextPadding(tft.textWidth("88:88", 4));
    tft.drawString(clockString(time(nullptr)), SCREEN_W - 6, HEADER_H / 2, 4);
    tft.setTextPadding(0);
  }
}

void drawFooter() {
  uint16_t bg = COL_BG;
  tft.setTextDatum(ML_DATUM);
  int y = SCREEN_H - FOOTER_H / 2;

  String text;
  uint16_t color = COL_DIM;
  if (WiFi.status() != WL_CONNECTED) {
    text = "WLAN getrennt - verbinde neu...";
    color = COL_CANCEL;
  } else if (lastError != "") {
    text = "Fehler: " + lastError;
    color = COL_CANCEL;
  } else if (lastSuccess > 0) {
    text = "Stand " + clockString(lastSuccess, true) + "   Tippen = aktualisieren";
    if (time(nullptr) - lastSuccess > 180) color = COL_DELAY;   // Daten veraltet
  }
  tft.setTextColor(color, bg);
  tft.setTextPadding(SCREEN_W - 4);   // überschreibt alten Text ohne Flackern
  tft.drawString(text, 4, y, 2);
  tft.setTextPadding(0);
}

void drawRow(int index, const Departure *dep, time_t now) {
  TFT_eSprite &s = rowSprite;
  s.fillSprite(COL_BG);
  s.drawFastHLine(0, ROW_H - 1, SCREEN_W, COL_ROWLINE);

  if (dep) {
    int mid = ROW_H / 2;

    // Linien-Badge
    s.fillRoundRect(4, 3, 46, ROW_H - 7, 4, colorForType(dep->type));
    s.setTextColor(COL_TEXT);
    s.setTextDatum(MC_DATUM);
    s.drawString(dep->line, 27, mid - 1, 2);

    // Zeit rechts
    long diffMin = (long)((dep->departure - now) / 60);
    String timeStr;
    uint16_t timeCol;
    int timeFont = 4;
    if (dep->cancelled) {
      timeStr = "faellt aus";
      timeCol = COL_CANCEL;
      timeFont = 2;
    } else {
      if (diffMin <= 0)       timeStr = "jetzt";
      else if (diffMin >= 60) timeStr = clockString(dep->departure);
      else                    timeStr = String(diffMin) + "'";
      if (!dep->realtime)     timeCol = COL_TEXT;
      else if (dep->delay > 0) timeCol = COL_DELAY;
      else                    timeCol = COL_ONTIME;
    }
    s.setTextDatum(MR_DATUM);
    s.setTextColor(timeCol);
    int timeW = s.textWidth(timeStr, timeFont);
    s.drawString(timeStr, SCREEN_W - 6, mid, timeFont);

    // Verspätung klein davor
    int rightEdge = SCREEN_W - 6 - timeW - 6;
    if (!dep->cancelled && dep->delay > 0) {
      String d = "+" + String(dep->delay);
      s.setTextColor(COL_DELAY);
      s.drawString(d, rightEdge, mid, 2);
      rightEdge -= s.textWidth(d, 2) + 6;
    }

    // Ziel (bei Bedarf gekürzt)
    String dest = dep->destination;
    int maxW = rightEdge - 58;
    if (s.textWidth(dest, 2) > maxW) {
      while (dest.length() > 1 && s.textWidth(dest + ".", 2) > maxW) {
        dest.remove(dest.length() - 1);
      }
      dest += ".";
    }
    s.setTextDatum(ML_DATUM);
    s.setTextColor(dep->cancelled ? COL_DIM : COL_TEXT);
    s.drawString(dest, 58, mid, 2);
    if (dep->cancelled) {
      s.drawFastHLine(58, mid, s.textWidth(dest, 2), COL_CANCEL);
    }
  }

  s.pushSprite(0, HEADER_H + index * ROW_H);
}

void drawDepartures() {
  time_t now = time(nullptr);
  int row = 0;
  for (int i = 0; i < departureCount && row < ROWS; i++) {
    // bereits abgefahrene Busse ausblenden (1 Minute Kulanz)
    if (departures[i].departure < now - 60) continue;
    drawRow(row++, &departures[i], now);
  }
  if (row == 0) {
    drawRow(row++, nullptr, now);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.drawString(lastSuccess ? "Keine Abfahrten" : "Lade Abfahrten...",
                   SCREEN_W / 2, HEADER_H + ROW_H / 2, 2);
  }
  while (row < ROWS) drawRow(row++, nullptr, now);
}

void drawAll() {
  drawHeader(false);
  drawDepartures();
  drawFooter();
}

void bootMessage(const String &msg) {
  Serial.println(msg);
  tft.fillRect(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H, COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(msg, SCREEN_W / 2, SCREEN_H / 2, 2);
}

// =====================================================================
// WLAN
// =====================================================================

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  bootMessage("Verbinde mit WLAN \"" + String(WIFI_SSID) + "\"...");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    bootMessage("WLAN verbunden: " + WiFi.localIP().toString());
  } else {
    bootMessage("WLAN nicht erreichbar - versuche weiter...");
  }
}

// =====================================================================
// Setup / Loop
// =====================================================================

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);           // Querformat, USB-Anschluss rechts
  tft.fillScreen(COL_BG);
  rowSprite.setColorDepth(16);
  rowSprite.createSprite(SCREEN_W, ROW_H);

  touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touch.begin(touchSpi);
  touch.setRotation(1);

  drawHeader(true);
  connectWifi();

  configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
  bootMessage("Hole Uhrzeit...");
  unsigned long start = millis();
  while (!timeValid() && millis() - start < 15000) delay(200);

  if (stationId == "") {
    bootMessage("Suche Haltestelle...");
    while (!findStation()) {
      bootMessage("Haltestellensuche: " + lastError);
      delay(5000);
    }
  }
  Serial.println("Haltestellen-ID: " + stationId);

  tft.fillScreen(COL_BG);
  drawHeader(true);
  drawAll();
}

void loop() {
  unsigned long nowMs = millis();

  // Touch: sofort aktualisieren
  static unsigned long lastTouchMs = 0;
  if (touch.tirqTouched() && touch.touched() && nowMs - lastTouchMs > 1000) {
    lastTouchMs = nowMs;
    forceFetch = true;
    tft.fillRect(0, SCREEN_H - FOOTER_H, SCREEN_W, FOOTER_H, COL_BG);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString("Aktualisiere...", 4, SCREEN_H - FOOTER_H / 2, 2);
  }

  // Daten laden
  if (forceFetch || nowMs - lastFetchMs >= REFRESH_SECONDS * 1000UL) {
    forceFetch = false;
    lastFetchMs = nowMs;
    if (WiFi.status() == WL_CONNECTED) {
      if (stationId == "") findStation();
      if (stationId != "") fetchDepartures();
    } else {
      WiFi.reconnect();
    }
    drawAll();
    lastDrawMs = nowMs;
  }

  // Minuten-Countdown und Uhr zwischen den Abrufen weiterlaufen lassen
  if (nowMs - lastDrawMs >= 5000) {
    lastDrawMs = nowMs;
    drawAll();
  }

  delay(20);
}
