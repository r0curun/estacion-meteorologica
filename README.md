# 🌤️ Estación Meteorológica ESP32-S3

Firmware para convertir la placa **ES3C28P** (ESP32-S3 + pantalla IPS de
2.8", LCDWIKI) en una estación meteorológica de escritorio: fecha, hora,
clima actual y pronóstico de 3 días, con datos gratuitos y sin necesidad de
crear cuentas ni API keys.

![status](https://img.shields.io/badge/estado-funcional-brightgreen)
![platform](https://img.shields.io/badge/plataforma-ESP32--S3-blue)
![license](https://img.shields.io/badge/licencia-MIT-lightgrey)

<img width="539" height="324" alt="image" src="https://github.com/user-attachments/assets/e3b23c83-2307-4485-aac4-85ae8c1db25d" />


## ✨ Características

- 🕐 Hora exacta por NTP (Internet), sin necesidad de un módulo RTC aparte
- ☀️ Clima actual: temperatura, condición, viento y humedad
- 📅 Pronóstico de los próximos 3 días
- 🌎 Datos vía [Open-Meteo](https://open-meteo.com/) — gratis, sin API key
- 🎨 Iconos del clima como imágenes BMP cargadas desde microSD (con
  respaldo automático a iconos vectoriales si no hay tarjeta)
- 🌓 Tema oscuro, pensado para dejarlo prendido en un velador o escritorio
- 🔒 Credenciales WiFi separadas del código fuente (`config.h`, fuera de git)

## 🛠️ Hardware necesario

- Placa **ES3C28P** (ESP32-S3 + pantalla IPS ILI9341V 240x320, SPI)
- (Opcional) MicroSD formateada en FAT32, para los íconos con imagen
- Cable USB-C

| Función   | GPIO |
|-----------|------|
| TFT MOSI  | 11   |
| TFT SCLK  | 12   |
| TFT MISO  | 13   |
| TFT CS    | 10   |
| TFT DC    | 46   |
| TFT BL    | 45   |
| SD CLK    | 38   |
| SD CMD    | 40   |
| SD D0–D3  | 39, 41, 48, 47 |

## 📦 Estructura del repositorio

```
estacion_meteorologica/
├── estacion_meteorologica.ino   # sketch principal
├── config.h.example             # plantilla de WiFi/ubicacion (copiar a config.h)
├── Setup_ES3C28P.h               # configuracion de pines para TFT_eSPI
├── icons/                        # iconos del clima en BMP (opcional, para la SD)
│   ├── big/
│   └── small/
├── CHANGELOG.md
├── LICENSE
└── README.md
```

## 🚀 Instalación

### 1. Placa y librerías

En Arduino IDE, agrega el soporte para ESP32 (**Preferencias → URLs
adicionales de tarjetas**):
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```
Instala el paquete **esp32 by Espressif Systems**, y selecciona:

| Opción | Valor |
|---|---|
| Placa | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| Flash Size | 16MB |
| Partition Scheme | Huge APP (3MB No OTA/1MB SPIFFS) |
| PSRAM | OPI PSRAM |

Instala desde el **Gestor de Librerías**: `TFT_eSPI` (Bodmer) y
`ArduinoJson` (Benoit Blanchon, v7.x).

### 2. Configurar TFT_eSPI para esta pantalla

Copia `Setup_ES3C28P.h` a la carpeta `User_Setups/` dentro de la librería
TFT_eSPI instalada (normalmente `Documentos/Arduino/libraries/TFT_eSPI/`).
Luego edita `TFT_eSPI/User_Setup_Select.h`: comenta la línea
`#include <User_Setup.h>` y agrega debajo
`#include <User_Setups/Setup_ES3C28P.h>`.

### 3. Configurar tus datos privados

```bash
cp config.h.example config.h
```
Edita `config.h` con tu WiFi, tu ubicación (coordenadas en
(https://www.latlong.net/)) y tu zona horaria. Este archivo
está en `.gitignore`: si subes tu propio fork a GitHub, tus datos no se
suben.

### 4. (Opcional) Íconos con imagen en la microSD

Formatea una microSD en FAT32 y copia la carpeta `icons/` completa a su
raíz. Si no lo haces, el firmware sigue funcionando normal, usando íconos
dibujados por código como respaldo automático.

### 5. Cargar el sketch

Conecta por USB-C, selecciona el puerto y sube. Si no sube: mantén
presionado **BOOT** al conectar el cable.

## 🩺 Solución de problemas

- **Pantalla en negro**: revisa que `Setup_ES3C28P.h` esté bien activado y
  que el pin de backlight (IO45) esté en HIGH.
- **Colores muy raros / mezclados**: confirma que el sketch tenga
  `tft.invertDisplay(true)` y `tft.setSwapBytes(true)` en `setup()`.
- **Imagen al revés**: cambia `tft.setRotation(1)` por `3` (o prueba `0`/`2`).
- **No conecta al WiFi**: el ESP32 solo soporta redes de 2.4GHz.
- **"No se pudo montar la microSD"**: revisa formato FAT32 y que la
  tarjeta esté bien insertada; el firmware sigue funcionando sin ella.
- **Error de compilación por tipos (`IconType`, `File`)**: asegúrate de
  tener la última versión del `.ino` de este repo — este bug de orden de
  declaración (una particularidad del Arduino IDE) ya está resuelto.

## 🎨 Personalización

- Colores: constantes `COL_*` al inicio del `.ino`.
- Frecuencia de actualización del clima: `WEATHER_UPDATE_MS`.
- Íconos propios: reemplaza cualquier BMP en `icons/` (24 bits sin
  comprimir, fondo negro puro, máximo 64x64 px), manteniendo el mismo
  nombre de archivo.

## 🙏 Créditos

- Datos del clima: [Open-Meteo](https://open-meteo.com/)
- Librería de pantalla: [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) (Bodmer)
- Parseo JSON: [ArduinoJson](https://arduinojson.org/) (Benoit Blanchon)
- Placa: ES3C28P (LCDWIKI)

## 📄 Licencia

MIT — ver [LICENSE](LICENSE). Usalo, modificalo y compartilo libremente.
