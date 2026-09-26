#pragma once

// ================== WLAN ==================
#define WIFI_SSID     "DEIN_WLAN_NAME"
#define WIFI_PASSWORD "DEIN_WLAN_PASSWORT"

// ================== Haltestelle ==================
// Suchbegriff für die MVG-Haltestellensuche (wird beim Start aufgelöst).
#define STATION_QUERY "Karlsfeld Rathausstraße"

// Optional: feste globalId der Haltestelle (z.B. "de:09174:1234").
// Wenn leer, wird die ID automatisch über STATION_QUERY gesucht.
// Die gefundene ID wird im Seriellen Monitor ausgegeben.
#define STATION_GLOBAL_ID ""

// Überschrift auf dem Display
#define STATION_TITLE "Rathausstrasse"

// ================== Filter (optional) ==================
// Nur bestimmte Linien anzeigen, kommagetrennt, z.B. "710,711". Leer = alle.
#define LINE_FILTER ""

// Abfahrten ausblenden, die früher als X Minuten fahren (z.B. Fußweg zur Haltestelle).
#define WALK_MINUTES 0

// ================== Aktualisierung ==================
#define REFRESH_SECONDS 30   // wie oft die Daten neu geladen werden
#define MAX_DEPARTURES  12   // wie viele Abfahrten von der API geholt werden
