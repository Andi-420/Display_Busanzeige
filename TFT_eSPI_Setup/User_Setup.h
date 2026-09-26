// TFT_eSPI Konfiguration für ESP32-2432S028R ("Cheap Yellow Display")
// Diese Datei ersetzt die User_Setup.h im Bibliotheksordner:
//   Dokumente/Arduino/libraries/TFT_eSPI/User_Setup.h

#define USER_SETUP_INFO "ESP32-2432S028R"

// Treiber. Falls Farben/Bild falsch aussehen, statt ILI9341_2_DRIVER
// "ILI9341_DRIVER" probieren bzw. TFT_INVERSION_ON aktivieren.
#define ILI9341_2_DRIVER
// #define ILI9341_DRIVER
// #define TFT_INVERSION_ON

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Display-Pins (HSPI)
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1

// Hintergrundbeleuchtung
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

#define USE_HSPI_PORT

// Fonts
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY       55000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000
