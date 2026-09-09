# Changelog

Todos los cambios notables de este proyecto se documentan aqui.

## [1.2.0]
### Cambiado
- Reorganizado como proyecto publicable: credenciales WiFi y ubicacion
  movidas a `config.h` (fuera de git) usando `config.h.example` como
  plantilla.
- Agregada licencia MIT, `.gitignore` y este changelog.

## [1.1.0]
### Agregado
- Iconos del clima como imagenes BMP cargadas desde la microSD
  (carpeta `icons/big` y `icons/small`), con respaldo automatico a
  iconos vectoriales si la SD no esta disponible.
- Sombra suave en los iconos para dar sensacion de profundidad.
### Cambiado
- Pantalla dividida en dos zonas de redibujo independientes
  (`drawDateTime()` y `drawWeatherInfo()`) para reducir el parpadeo:
  la hora se redibuja cada minuto, el clima solo cuando hay datos nuevos.
- Tarjetas de pronostico con fondo negro y borde blanco (en vez de
  gris), para que combinen sin "costura" con el fondo de los iconos.
- Reduccion de tamano de la temperatura, icono de viento y gota de
  humedad; recoloreado de la gota a celeste pastel.
- Icono de viento rediseñado para escalar correctamente en tamano.
- Texto de condicion del clima reubicado debajo de la temperatura.
### Corregido
- Colores invertidos del panel ILI9341 (`invertDisplay`).
- Colores mezclados en los iconos BMP por falta de `setSwapBytes(true)`.
- Error de compilacion por orden de declaracion de tipos (`IconType`,
  `fs::File`) causado por la generacion automatica de prototipos del
  IDE de Arduino.

## [1.0.0]
### Agregado
- Primera version funcional: fecha/hora por NTP, clima actual y
  pronostico de 3 dias via Open-Meteo (sin API key), pantalla ILI9341
  240x320 vía TFT_eSPI, iconos de clima dibujados por codigo.
