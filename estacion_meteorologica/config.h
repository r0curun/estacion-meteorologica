// ============================================================
// config.h.example
//
// COMO USAR:
//   1. Copia este archivo y renombralo a "config.h"
//   2. Completa tu WiFi y tu ubicacion abajo
//   3. Listo — "config.h" esta en .gitignore, asi que git nunca lo
//      va a subir a GitHub (tu WiFi y ubicacion quedan privados)
// ============================================================

#pragma once

// --- WiFi ---
#define WIFI_SSID     "Nombredetured"
#define WIFI_PASSWORD "contrasenadetured"

// --- Ubicacion de tu ciudad (por defecto: Guayaquil, Ecuador) ---
// Busca la tuya en https://www.latlong.net/
#define LATITUDE  -2.170998
#define LONGITUDE -79.922356

// --- Zona horaria (en segundos respecto a UTC) ---
// Ejemplos: Mexico centro = -6*3600, Argentina = -3*3600, Espana = 1*3600
#define GMT_OFFSET_SEC      (-5 * 3600)
#define DAYLIGHT_OFFSET_SEC 0
