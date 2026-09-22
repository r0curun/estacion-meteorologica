/*
  ============================================================
  ESTACION METEOROLOGICA - ESP32-S3 (placa ES3C28P, LCDWIKI)
  Pantalla: 2.8" IPS ILI9341V 240x320, SPI 4 hilos
  Datos de clima: Open-Meteo (gratis, sin API key)
  Hora: NTP (Internet)

  Repositorio: ver README.md para instalacion completa.
  ============================================================

  ANTES DE SUBIR EL SKETCH, REVISA ESTO:
   1) En "config.h" pon tu WiFi y ubicacion
      (config.h esta en .gitignore, nunca se sube a GitHub)
   2) Que hayas configurado TFT_eSPI con el archivo
      "Setup_ES3C28P.h" que viene junto a este sketch
      (instrucciones en el README.md)
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "FS.h"
#include "SD_MMC.h"
#include "config.h" // tus datos privados: ver config.h


// Cada cuanto se actualiza el clima (en milisegundos)
const unsigned long WEATHER_UPDATE_MS = 10UL * 60UL * 1000UL; // 10 minutos
// ---------------------------------------------------------

TFT_eSPI tft = TFT_eSPI();

// Pin del backlight (respaldo por si TFT_eSPI no lo enciende solo)
#define PIN_BACKLIGHT 45

// Pines de la microSD (bus SDIO de 4 bits de la placa ES3C28P)
#define SD_MMC_CLK 38
#define SD_MMC_CMD 40
#define SD_MMC_D0  39
#define SD_MMC_D1  41
#define SD_MMC_D2  48
#define SD_MMC_D3  47

bool sdReady = false;

// Paleta de colores (tema oscuro, como en la imagen de referencia)
#define COL_BG      TFT_BLACK
#define COL_CARD    COL_BG      // fondo negro, igual al fondo de los iconos (sin "costura" visible)
#define COL_CARD_BORDER TFT_WHITE
#define COL_TEXT    TFT_WHITE
#define COL_SUBTEXT 0x8410   // gris medio
#define COL_SUN     TFT_YELLOW
#define COL_WIND    TFT_GREEN
#define COL_HUMID   0x867F   // celeste suave 

struct WeatherNow {
  float temp = 0;
  int   humidity = 0;
  float wind = 0;
  int   code = 0;
};

struct ForecastDay {
  float  tmax = 0, tmin = 0;
  int    code = 0;
  time_t date = 0;
};

WeatherNow   current;
ForecastDay  forecast[3];

unsigned long lastWeatherUpdate = 0;
int lastMinuteDrawn = -1;
bool weatherReady = false;

const char* diasSemana[7] = {"Domingo","Lunes","Martes","Miercoles","Jueves","Viernes","Sabado"};
const char* mesesAnio[12] = {"enero","febrero","marzo","abril","mayo","junio","julio",
                             "agosto","septiembre","octubre","noviembre","diciembre"};
const char* diasAbrev[7]  = {"Dom","Lun","Mar","Mie","Jue","Vie","Sab"};

// IMPORTANTE: este enum se declara aqui arriba (antes de cualquier funcion)
// para evitar el clasico bug de Arduino IDE donde los prototipos
// auto-generados quedan ubicados antes de que el tipo exista.
enum IconType { ICON_SUN, ICON_PARTLY, ICON_CLOUD, ICON_RAIN, ICON_STORM, ICON_SNOW, ICON_FOG };

// ============================================================
// MICROSD: montaje y lectura de iconos BMP (24 bits)
// ============================================================
void initSDCard() {
  if (!SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0, SD_MMC_D1, SD_MMC_D2, SD_MMC_D3)) {
    Serial.println("No se pudieron configurar los pines de la microSD");
    sdReady = false;
    return;
  }
  // "false" = bus de 4 bits (asi esta cableada esta placa)
  if (!SD_MMC.begin("/sdcard", false)) {
    Serial.println("No se pudo montar la microSD (revisa que este insertada y formateada en FAT32)");
    sdReady = false;
    return;
  }
  sdReady = true;
  Serial.println("microSD montada correctamente");
}

uint16_t bmpRead16(fs::File &f) {
  uint8_t lo = f.read();
  uint8_t hi = f.read();
  return (uint16_t)lo | ((uint16_t)hi << 8);
}

uint32_t bmpRead32(fs::File &f) {
  uint32_t b0 = f.read();
  uint32_t b1 = f.read();
  uint32_t b2 = f.read();
  uint32_t b3 = f.read();
  return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

#define MAX_ICON_PX 64 // tamano maximo (en pixeles) de icono que soportamos

// Dibuja un BMP de 24 bits sin comprimir, centrado en (cx, cy).
// Devuelve false si el archivo no existe o el formato no es compatible
// (en ese caso el llamador debe usar el icono vectorial de respaldo).
bool drawBmpCentered(const char* path, int cx, int cy) {
  if (!sdReady) return false;

  fs::File bmpFile = SD_MMC.open(path);
  if (!bmpFile) return false;

  if (bmpRead16(bmpFile) != 0x4D42) { // firma 'BM'
    bmpFile.close();
    return false;
  }

  bmpRead32(bmpFile);                    // tamano de archivo (no se usa)
  bmpRead32(bmpFile);                    // reservado
  uint32_t dataOffset = bmpRead32(bmpFile);
  bmpRead32(bmpFile);                    // tamano de cabecera DIB
  int32_t bmpWidth  = (int32_t)bmpRead32(bmpFile);
  int32_t bmpHeight = (int32_t)bmpRead32(bmpFile);
  bmpRead16(bmpFile);                    // planos de color
  uint16_t depth = bmpRead16(bmpFile);
  uint32_t compression = bmpRead32(bmpFile);

  bool ok = (depth == 24) && (compression == 0) &&
            (bmpWidth > 0) && (bmpWidth <= MAX_ICON_PX) &&
            (abs((int)bmpHeight) <= MAX_ICON_PX);
  if (!ok) {
    bmpFile.close();
    return false; // solo soportamos BMP de 24 bits, sin comprimir
  }

  bool flip = true; // BMP normal: filas guardadas de abajo hacia arriba
  if (bmpHeight < 0) { bmpHeight = -bmpHeight; flip = false; }

  int x0 = cx - bmpWidth / 2;
  int y0 = cy - bmpHeight / 2;

  uint32_t rowSize = (bmpWidth * 3 + 3) & ~3; // filas alineadas a 4 bytes
  uint8_t  lineBuffer[MAX_ICON_PX * 3];
  uint16_t pixelBuffer[MAX_ICON_PX];

  for (int row = 0; row < bmpHeight; row++) {
    int srcRow = flip ? (bmpHeight - 1 - row) : row;
    bmpFile.seek(dataOffset + (uint32_t)srcRow * rowSize);
    bmpFile.read(lineBuffer, bmpWidth * 3);

    for (int col = 0; col < bmpWidth; col++) {
      uint8_t b = lineBuffer[col * 3 + 0];
      uint8_t g = lineBuffer[col * 3 + 1];
      uint8_t r = lineBuffer[col * 3 + 2];
      pixelBuffer[col] = tft.color565(r, g, b);
    }
    tft.pushImage(x0, y0 + row, bmpWidth, 1, pixelBuffer);
  }

  bmpFile.close();
  return true;
}

// ============================================================
// ICONOS DE CLIMA (dibujados con formas, no necesitan imagenes)
// Sirven de respaldo si la microSD no esta lista o falta un archivo
// ============================================================

IconType codeToIcon(int code) {
  if (code == 0) return ICON_SUN;
  if (code == 1 || code == 2) return ICON_PARTLY;
  if (code == 3) return ICON_CLOUD;
  if (code == 45 || code == 48) return ICON_FOG;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return ICON_RAIN;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return ICON_SNOW;
  if (code == 95 || code == 96 || code == 99) return ICON_STORM;
  return ICON_CLOUD;
}

String codeToText(int code) {
  if (code == 0) return "Despejado";
  if (code == 1) return "Mayormente despejado";
  if (code == 2) return "Parcialmente nublado";
  if (code == 3) return "Nublado";
  if (code == 45 || code == 48) return "Niebla";
  if (code >= 51 && code <= 57) return "Llovizna";
  if (code >= 61 && code <= 67) return "Lluvia";
  if (code >= 71 && code <= 77) return "Nieve";
  if (code >= 80 && code <= 82) return "Chubascos";
  if (code == 85 || code == 86) return "Chubascos de nieve";
  if (code >= 95) return "Tormenta";
  return "Variable";
}

void drawSun(int x, int y, int r, uint16_t color) {
  tft.fillCircle(x, y, r, color);
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4.0;
    int x1 = x + cos(a) * (r + 4);
    int y1 = y + sin(a) * (r + 4);
    int x2 = x + cos(a) * (r + 10);
    int y2 = y + sin(a) * (r + 10);
    tft.drawLine(x1, y1, x2, y2, color);
  }
}

void drawCloud(int x, int y, int scale, uint16_t color) {
  tft.fillCircle(x - scale, y, scale * 0.7, color);
  tft.fillCircle(x, y - scale * 0.4, scale * 0.9, color);
  tft.fillCircle(x + scale, y, scale * 0.7, color);
  tft.fillRect(x - scale, y, scale * 2, scale * 0.7 + 1, color);
}

void drawIcon(IconType icon, int x, int y, int size) {
  switch (icon) {
    case ICON_SUN:
      drawSun(x, y, size, COL_SUN);
      break;
    case ICON_PARTLY:
      drawSun(x - size * 0.4, y - size * 0.3, size * 0.7, COL_SUN);
      drawCloud(x + size * 0.25, y + size * 0.25, size * 0.7, TFT_WHITE);
      break;
    case ICON_CLOUD:
      drawCloud(x, y, size * 0.9, TFT_WHITE);
      break;
    case ICON_FOG:
      drawCloud(x, y - size * 0.3, size * 0.8, COL_SUBTEXT);
      for (int i = 0; i < 3; i++)
        tft.drawFastHLine(x - size, y + size * 0.4 + i * 6, size * 2, COL_SUBTEXT);
      break;
    case ICON_RAIN:
      drawCloud(x, y - size * 0.3, size * 0.8, TFT_LIGHTGREY);
      for (int i = -1; i <= 1; i++)
        tft.drawLine(x + i * size * 0.5, y + size * 0.35,
                     x + i * size * 0.5 - 3, y + size * 0.75, TFT_CYAN);
      break;
    case ICON_STORM: {
      drawCloud(x, y - size * 0.3, size * 0.8, TFT_LIGHTGREY);
      int bx = x, by = y + size * 0.2;
      tft.fillTriangle(bx, by, bx + 6, by, bx - 2, by + 10, COL_SUN);
      tft.fillTriangle(bx - 2, by + 10, bx + 4, by + 10, bx - 6, by + 20, COL_SUN);
      break;
    }
    case ICON_SNOW:
      drawCloud(x, y - size * 0.3, size * 0.8, TFT_WHITE);
      for (int i = -1; i <= 1; i++)
        tft.fillCircle(x + i * size * 0.5, y + size * 0.6, 2, TFT_WHITE);
      break;
  }
}

const char* iconFileName(IconType icon) {
  switch (icon) {
    case ICON_SUN:    return "sun.bmp";
    case ICON_PARTLY: return "partly.bmp";
    case ICON_CLOUD:  return "cloud.bmp";
    case ICON_RAIN:   return "rain.bmp";
    case ICON_STORM:  return "storm.bmp";
    case ICON_SNOW:   return "snow.bmp";
    case ICON_FOG:    return "fog.bmp";
  }
  return "cloud.bmp";
}

// Intenta dibujar el icono como imagen BMP desde la microSD
// (carpeta "big" para el icono principal, "small" para las tarjetas).
// Si la SD no esta lista o falta el archivo, dibuja el icono vectorial.
void drawWeatherIcon(IconType icon, int x, int y, int vectorSize, bool big) {
  char path[48];
  snprintf(path, sizeof(path), "/icons/%s/%s", big ? "big" : "small", iconFileName(icon));
  if (!drawBmpCentered(path, x, y)) {
    drawIcon(icon, x, y, vectorSize);
  }
}

// Icono de viento: 3 lineas de distinto largo con un pequeno rizo en las
// puntas (arriba y abajo). Todo escala con "size", asi que ahora si se
// achica correctamente en ambas dimensiones (antes el alto quedaba fijo).
void drawWindIcon(int x, int y, int size, uint16_t color) {
  int len1 = size * 2.0;  // linea superior (corta)
  int len2 = size * 2.6;  // linea del medio (la mas larga)
  int len3 = size * 1.6;  // linea inferior (corta)
  int gap  = max(2, (int)(size * 0.7)); // separacion vertical entre lineas
  int r    = max(1, size / 3);          // radio del rizo en las puntas

  tft.drawFastHLine(x - len1 / 2, y - gap, len1, color);
  tft.drawFastHLine(x - len2 / 2, y,       len2, color);
  tft.drawFastHLine(x - len3 / 2, y + gap, len3, color);

  tft.drawCircle(x + len1 / 2 - r, y - gap - r, r, color);
  tft.drawCircle(x + len3 / 2 - r, y + gap + r, r, color);
}

// Icono de gota de humedad
void drawDropIcon(int x, int y, int r, uint16_t color) {
  tft.fillTriangle(x - r * 0.7, y + r * 0.1, x + r * 0.7, y + r * 0.1, x, y - r, color);
  tft.fillCircle(x, y + r * 0.3, r * 0.7, color);
}

// ============================================================
// DIBUJO DE PANTALLA
// ============================================================
// Fecha y hora (columna izquierda). Se llama cada vez que cambia el minuto,
// asi que solo limpia y redibuja ESA zona (no toda la pantalla) para
// reducir el parpadeo.
void drawDateTime() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  tft.fillRect(0, 0, 158, 112, COL_BG);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(4);
  tft.setCursor(8, 4);
  tft.print(diasSemana[timeinfo.tm_wday]);

  char fecha[40];
  snprintf(fecha, sizeof(fecha), "%d de %s, %d",
           timeinfo.tm_mday, mesesAnio[timeinfo.tm_mon], timeinfo.tm_year + 1900);
  tft.setTextColor(COL_SUBTEXT, COL_BG);
  tft.setTextFont(2);
  tft.setCursor(8, 34);
  tft.print(fecha);

  char hora[6];
  snprintf(hora, sizeof(hora), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(7);
  tft.setCursor(8, 60);
  tft.print(hora);
}

// Icono, temperatura, condicion, viento y humedad (columna derecha).
// Solo se llama cuando llegan datos nuevos del clima (cada ~10 min), no
// cada minuto, para minimizar el parpadeo.
void drawWeatherInfo() {
  tft.fillRect(158, 0, 162, 152, COL_BG);

  IconType icon = codeToIcon(current.code);
  drawWeatherIcon(icon, 268, 40, 27, true);

  char t[8];
  snprintf(t, sizeof(t), "%.1f", current.temp);
  tft.setTextFont(4); // fuente mas chica que antes (antes Font6)
  tft.setTextColor(COL_TEXT, COL_BG);
  int wtxt = tft.drawString(t, 165, 56);
  tft.drawCircle(165 + wtxt + 5, 60, 3, COL_TEXT); // simbolo de grados

  tft.setTextFont(2);
  tft.setTextColor(COL_HUMID, COL_BG); // celeste pastel (en vez de gris)
  tft.setCursor(165, 84);
  tft.print(codeToText(current.code));

  drawWindIcon(172, 108, 5, COL_WIND);
  tft.setTextColor(COL_WIND, COL_BG);
  char w[16];
  snprintf(w, sizeof(w), "%.1f km/h", current.wind);
  tft.setCursor(192, 102);
  tft.print(w);

  drawDropIcon(172, 128, 6, COL_HUMID);
  tft.setTextColor(COL_HUMID, COL_BG);
  char h[8];
  snprintf(h, sizeof(h), "%d%%", current.humidity);
  tft.setCursor(192, 122);
  tft.print(h);
}

void drawForecastCards() {
  const int top = 154, height = 80, gap = 6, margin = 6;
  const int cardW = (320 - margin * 2 - gap * 2) / 3;

  tft.fillRect(0, top - 2, 320, height + 4, COL_BG);

  for (int i = 0; i < 3; i++) {
    int x = margin + i * (cardW + gap);
    tft.fillRoundRect(x, top, cardW, height, 8, COL_CARD);
    tft.drawRoundRect(x, top, cardW, height, 8, COL_CARD_BORDER);

    IconType icon = codeToIcon(forecast[i].code);
    drawWeatherIcon(icon, x + cardW / 2, top + 22, 13, false);

    struct tm ti;
    localtime_r(&forecast[i].date, &ti);

    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%.0f/%.0f", forecast[i].tmax, forecast[i].tmin);
    tft.setTextFont(2);
    tft.setTextColor(COL_TEXT, COL_CARD);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(tmp, x + cardW / 2, top + 50);

    char dia[16];
    snprintf(dia, sizeof(dia), "%s %d", diasAbrev[ti.tm_wday], ti.tm_mday);
    tft.setTextColor(COL_SUBTEXT, COL_CARD);
    tft.drawString(dia, x + cardW / 2, top + 68);
  }
  tft.setTextDatum(TL_DATUM);
}

// ============================================================
// RED / WIFI / CLIMA
// ============================================================
void showMessage(const String &msg) {
  tft.fillScreen(COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setCursor(10, 10);
  tft.println(msg);
}

void connectWiFi() {
  showMessage("Conectando a WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(400);
    tft.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    tft.println("\nConectado!");
    Serial.println(WiFi.localIP());
  } else {
    tft.println("\nNo se pudo conectar. Reintentando...");
  }
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure(); // omite verificacion de certificado (simplifica el proyecto)

  HTTPClient http;
  char url[300];
  snprintf(url, sizeof(url),
    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min"
    "&timezone=auto&forecast_days=4",
    LATITUDE, LONGITUDE);

  http.begin(client, url);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("HTTP error: %d\n", code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("Error JSON: ");
    Serial.println(err.c_str());
    return false;
  }

  current.temp     = doc["current"]["temperature_2m"] | 0.0;
  current.humidity = doc["current"]["relative_humidity_2m"] | 0;
  current.wind     = doc["current"]["wind_speed_10m"] | 0.0;
  current.code     = doc["current"]["weather_code"] | 0;

  for (int i = 0; i < 3; i++) {
    int idx = i + 1; // saltamos el dia de hoy (indice 0)
    forecast[i].tmax = doc["daily"]["temperature_2m_max"][idx] | 0.0;
    forecast[i].tmin = doc["daily"]["temperature_2m_min"][idx] | 0.0;
    forecast[i].code = doc["daily"]["weather_code"][idx] | 0;

    const char* dstr = doc["daily"]["time"][idx];
    struct tm ti = {};
    if (dstr) strptime(dstr, "%Y-%m-%d", &ti);
    forecast[i].date = mktime(&ti);
  }

  weatherReady = true;
  return true;
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_BACKLIGHT, OUTPUT);
  digitalWrite(PIN_BACKLIGHT, HIGH);

  tft.init();
  tft.setRotation(1); // si la imagen sale al reves, prueba con 3
  tft.invertDisplay(true); // corrige colores invertidos (fondo blanco/texto negro)
  tft.setSwapBytes(true);  // corrige colores mezclados en los iconos BMP (sol azul, nube verde)
  tft.fillScreen(COL_BG);

  initSDCard(); // si falla o no hay tarjeta, se usan los iconos vectoriales

  connectWiFi();
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");

  showMessage("Obteniendo datos del clima...");
  fetchWeather();
  lastWeatherUpdate = millis();

  tft.fillScreen(COL_BG);
  drawDateTime();
  drawWeatherInfo();
  drawForecastCards();
}

void loop() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  // Redibuja SOLO fecha/hora cuando cambia el minuto (evita parpadeo
  // innecesario en el resto de la pantalla)
  if (timeinfo.tm_min != lastMinuteDrawn) {
    lastMinuteDrawn = timeinfo.tm_min;
    drawDateTime();
  }

  // Actualiza el clima cada WEATHER_UPDATE_MS (esto SI redibuja icono,
  // temperatura y pronostico, pero solo ocurre cada varios minutos)
  if (millis() - lastWeatherUpdate > WEATHER_UPDATE_MS) {
    if (fetchWeather()) {
      drawWeatherInfo();
      drawForecastCards();
    }
    lastWeatherUpdate = millis();
  }

  // Reconecta si se cae el WiFi
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    tft.fillScreen(COL_BG);
    lastMinuteDrawn = -1; // fuerza redibujo de fecha/hora en el proximo ciclo
    drawWeatherInfo();
    drawForecastCards();
  }

  delay(1000);
}
