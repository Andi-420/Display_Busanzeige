# Abfahrtsmonitor Karlsfeld (ESP32-2432S028R)

Zeigt die nächsten Abfahrten der Haltestellen **Karlsfeld, Rathausstraße** und
**Karlsfeld, Einkaufsmärkte** gemeinsam auf dem
2,8"-Display (ILI9341, 320×240) des ESP32-2432S028R an. Die Echtzeitdaten kommen von der
MVG-API, die das gesamte MVV-Gebiet abdeckt, also auch die Regionalbusse in Karlsfeld.

```
┌──────────────────────────────────────┐
│ Karlsfeld                      14:32 │
├──────────────────────────────────────┤
│ [710] R Dachau Bahnhof           3'  │  grün   = Echtzeit, pünktlich
│ [711] E Karlsfeld S-Bahnhof +2   7'  │  orange = verspätet
│ [710] R Moosach Bahnhof         12'  │  weiß   = nur Fahrplan
│ ...                                  │
├──────────────────────────────────────┤
│ Stand 14:32:05   Tippen = aktualisieren
└──────────────────────────────────────┘
```

Die Liniennummern in der Skizze sind nur Beispiele. Angezeigt wird, was die API liefert.

## 1. Arduino IDE einrichten

1. **ESP32-Boardpaket installieren**
   Datei → Einstellungen → „Zusätzliche Boardverwalter-URLs“:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   Danach unter Werkzeuge → Board → Boardverwalter das Paket „esp32“ von Espressif installieren.
2. **Board wählen:** Werkzeuge → Board → *ESP32 Dev Module*
3. **Bibliotheken installieren** (Sketch → Bibliothek einbinden → Bibliotheken verwalten):
   - `TFT_eSPI` von Bodmer
   - `XPT2046_Touchscreen` von Paul Stoffregen
   - `ArduinoJson` von Benoit Blanchon (Version 7.x)

## 2. TFT_eSPI konfigurieren (wichtig!)

TFT_eSPI liest die Pinbelegung aus einer Datei im Bibliotheksordner. Kopiere
`TFT_eSPI_Setup/User_Setup.h` aus diesem Repository nach

- Windows: `Dokumente\Arduino\libraries\TFT_eSPI\User_Setup.h`
- macOS/Linux: `~/Arduino/libraries/TFT_eSPI/User_Setup.h`

und überschreibe die vorhandene Datei. Nach einem Update von TFT_eSPI musst du das wiederholen.

## 3. WLAN eintragen

Öffne `Karlsfeld_Abfahrten/Karlsfeld_Abfahrten.ino` in der Arduino IDE. Trage im Tab
`config.h` unter `WIFI_NETWORKS` deine WLANs ein. Du kannst beliebig viele eintragen. Das
Board verbindet sich mit dem stärksten erreichbaren Netz. Bricht die Verbindung ab, wechselt
es automatisch zu einem anderen bekannten Netz. Das Board braucht ein **2,4-GHz-WLAN**.

```cpp
static const WifiCredentials WIFI_NETWORKS[] = {
  { "Zuhause",      "passwort1" },
  { "Buero",        "passwort2" },
  { "Handy",        "passwort3" },
};
```

In `config.h` kannst du außerdem Folgendes einstellen:

| Einstellung | Bedeutung |
|---|---|
| `LINE_FILTER` | nur bestimmte Linien anzeigen, z. B. `"710,711"` |
| `WALK_MINUTES` | Abfahrten ausblenden, die du zu Fuß nicht mehr erreichst |
| `REFRESH_SECONDS` | Abrufintervall (Standard: 30 s) |
| `STATIONS` | Liste der Haltestellen (Suchbegriff, Kürzel, optional feste ID) |

## 4. Hochladen

Board per USB anschließen, Port auswählen und hochladen. Falls der Upload nicht startet:
beim Erscheinen von „Connecting…“ die **BOOT**-Taste gedrückt halten.

Öffne den Seriellen Monitor mit 115200 Baud. Dort steht, welche Haltestelle gefunden wurde,
z. B. `Gefunden [R]: Karlsfeld, Rathausstraße -> de:09174:xxxx`. Diese ID kannst du in
`STATIONS` als vierten Wert eintragen. Dann entfällt die Suche beim Start.

## Mehrere Haltestellen

In `config.h` stehen die Haltestellen in der Liste `STATIONS`:

```cpp
static const StationConfig STATIONS[] = {
  { "Karlsfeld Rathausstraße",   "Rathaus", "R", "" },
  { "Karlsfeld Einkaufsmärkte",  "Einkauf", "E", "" },
};
```

Die Abfahrten aller Haltestellen werden zusammen nach Zeit sortiert angezeigt. Das Kürzel
(`R`, `E`) steht grau vor dem Ziel und zeigt, an welcher Haltestelle der Bus abfährt. Weitere
Haltestellen kannst du einfach als neue Zeile ergänzen.

## Bedienung

- Die Anzeige lädt die Daten alle 30 Sekunden neu. Der Minuten-Countdown läuft dazwischen weiter.
- Ein **Tipp auf das Display** lädt die Daten sofort neu (nachts: Display kurz einschalten).
- Ab 60 Minuten wird die Uhrzeit statt der Minuten angezeigt. Ausfälle erscheinen durchgestrichen mit „faellt aus“.

## Helligkeitsregelung

Der Lichtsensor (LDR) auf dem Board regelt die Hintergrundbeleuchtung automatisch und blendet
sanft über. Einstellungen in `config.h`:

- `BRIGHTNESS_MIN` / `BRIGHTNESS_MAX`: Helligkeit bei Dunkelheit bzw. hellem Licht (0–255)
- `LDR_BRIGHT` / `LDR_DARK`: Kalibrierung. Im Seriellen Monitor erscheint alle 2 s
  `LDR roh: ...`. Notiere den Wert im hellen Raum und bei abgedecktem Sensor und trage ihn ein.
- `AUTO_BRIGHTNESS false` schaltet die Regelung ab. `LDR_DEBUG false` schaltet die Ausgabe ab.

Der Sensor sitzt auf der Vorderseite neben dem Display. Bei manchen Boards reagiert er nur
schwach. Ändert sich der Rohwert kaum, liegt das an der Hardware.

## Nachtabschaltung

Von `NIGHT_START` (23 Uhr) bis `NIGHT_END` (6 Uhr) ist die Beleuchtung aus, und es werden
keine Daten abgerufen. Ein Tipp auf das Display schaltet es für `NIGHT_WAKE_SECONDS`
(30 s) ein und lädt aktuelle Abfahrten. `NIGHT_MODE false` schaltet die Funktion ab.

## Fehlerbehebung

| Problem | Lösung |
|---|---|
| Bildschirm bleibt weiß/schwarz | `User_Setup.h` wurde nicht korrekt kopiert |
| Farben invertiert / Bild gespiegelt | In `User_Setup.h` `ILI9341_DRIVER` statt `ILI9341_2_DRIVER` bzw. `TFT_INVERSION_ON` probieren (es gibt mehrere Hardware-Varianten) |
| Kompilierfehler in TFT_eSPI | neueste TFT_eSPI-Version installieren; hilft das nicht, ESP32-Boardpaket 2.0.17 verwenden |
| „HTTP-Fehler -1“ | WLAN/Internet prüfen |
| Falsche Haltestelle | Zeilen `Gefunden [..]` im Seriellen Monitor prüfen und die richtige ID in `STATIONS` eintragen |

Die Umlaute werden als ae/oe/ue/ss dargestellt, weil die eingebauten Schriftarten nur ASCII
unterstützen.
