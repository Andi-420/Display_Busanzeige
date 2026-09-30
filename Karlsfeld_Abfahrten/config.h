#pragma once

// ================== WLAN ==================
// Beliebig viele WLANs eintragen. Das Board verbindet sich automatisch mit
// dem stärksten erreichbaren Netz aus dieser Liste.
struct WifiCredentials { const char *ssid; const char *password; };
static const WifiCredentials WIFI_NETWORKS[] = {
  { "DEIN_WLAN_NAME",   "DEIN_WLAN_PASSWORT" },
  { "ZWEITES_WLAN",     "PASSWORT_2" },
  // { "HANDY_HOTSPOT", "PASSWORT_3" },
};

// ================== Haltestellen ==================
// Die Abfahrten aller Haltestellen werden gemeinsam, nach Zeit sortiert angezeigt.
//   query    : Suchbegriff für die MVG-Haltestellensuche
//   match    : Teil des Haltestellennamens, um den richtigen Treffer auszuwählen
//   tag      : kurzes Kürzel, das in jeder Zeile vor dem Ziel steht
//   globalId : optional feste ID (z.B. "de:09174:1234"), dann entfällt die Suche.
//              Die gefundenen IDs stehen beim Start im Seriellen Monitor.
struct StationConfig { const char *query; const char *match; const char *tag; const char *globalId; };
static const StationConfig STATIONS[] = {
  { "Karlsfeld Rathausstraße",   "Rathaus", "R", "" },
  { "Karlsfeld Einkaufsmärkte",  "Einkauf", "E", "" },
};

// Überschrift auf dem Display
#define STATION_TITLE "Karlsfeld"

// ================== Filter (optional) ==================
// Nur bestimmte Linien anzeigen, kommagetrennt, z.B. "710,711". Leer = alle.
#define LINE_FILTER ""

// Abfahrten ausblenden, die früher als X Minuten fahren (z.B. Fußweg zur Haltestelle).
#define WALK_MINUTES 0

// ================== Aktualisierung ==================
#define REFRESH_SECONDS 30   // wie oft die Daten neu geladen werden
#define MAX_DEPARTURES  12   // wie viele Abfahrten pro Haltestelle geholt werden

// ================== Helligkeitsregelung ==================
// Lichtsensor (LDR) auf dem Board steuert die Hintergrundbeleuchtung.
// false = immer volle Helligkeit.
#define AUTO_BRIGHTNESS true

// Helligkeit der Hintergrundbeleuchtung (0..255)
#define BRIGHTNESS_MIN  15   // bei Dunkelheit (0 = ganz aus)
#define BRIGHTNESS_MAX  255  // bei hellem Umgebungslicht

// Kalibrierung des Sensors: Rohwerte aus dem Seriellen Monitor ablesen
// ("LDR roh: ..."), einmal im hellen Raum, einmal abgedunkelt.
// Beim CYD gilt: viel Licht = kleiner Wert, Dunkelheit = großer Wert.
#define LDR_BRIGHT      0    // Rohwert bei hellem Licht
#define LDR_DARK        300  // Rohwert bei Dunkelheit

// Sensorwerte alle 2 s im Seriellen Monitor ausgeben (zum Kalibrieren)
#define LDR_DEBUG       true

// ================== Nachtabschaltung ==================
// Display ist zwischen NIGHT_START und NIGHT_END (volle Stunden) aus.
// Antippen schaltet es für NIGHT_WAKE_SECONDS wieder ein.
#define NIGHT_MODE          true
#define NIGHT_START         23
#define NIGHT_END           6
#define NIGHT_WAKE_SECONDS  30
