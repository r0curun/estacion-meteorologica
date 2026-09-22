/*
  ============================================================
  ESTACION METEOROLOGICA - ESP32-S3 (placa ES3C28P, LCDWIKI)
  Pantalla: 2.8" IPS ILI9341V 240x320, SPI 4 hilos
  Datos de clima: Open-Meteo (gratis, sin API key)
  Hora: NTP (Internet)

  Repositorio: ver README.md para instalacion completa.
  ============================================================

  ANTES DE SUBIR EL SKETCH, REVISA ESTO:
   1) Entra en "config.h" y pon tu WiFi y ubicacion
      
   2) Que hayas configurado TFT_eSPI con el archivo
      "Setup_ES3C28P.h" que viene junto a este sketch
      (instrucciones en el README.md)
*/

#define USER_SETUP_ID 901   // id arbitrario, solo para identificar este setup
 
#define ILI9341_DRIVER
 
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
 
// --- Pines (bus SPI de la placa ES3C28P) ---
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_MISO 13
#define TFT_CS   10
#define TFT_DC   46
#define TFT_RST  -1   // sin pin de reset dedicado (va atado al EN de la placa)
#define TFT_BL   45
#define TFT_BACKLIGHT_ON HIGH
 
// --- Fuentes ---
// El sketch solo usa las fuentes clasicas 2, 4 y 7 (setTextFont), no
// fuentes suaves/TTF, asi que no hace falta SMOOTH_FONT ni LOAD_GFXFF.
#define LOAD_GLCD   // fuente 1, por defecto
#define LOAD_FONT2  // fuente 2
#define LOAD_FONT4  // fuente 4
#define LOAD_FONT7  // fuente 7 (7 segmentos, para la hora)
 
// --- Velocidad del bus SPI ---
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000

