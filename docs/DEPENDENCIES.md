# Abhängigkeiten

Reproduzierbarer Build über **PlatformIO** mit der **pioarduino**-Plattform
(Arduino-ESP32 Core 3.1.x). Versionen in `platformio.ini` gepinnt.

| Abhängigkeit | Version | Zweck |
|--------------|---------|-------|
| Plattform `pioarduino` | `53.03.13` | Arduino-ESP32 Core 3.1.x |
| `moononournation/GFX Library for Arduino` | `^1.4.9` | Display (ST7789 1.47") |

`WiFi` und `time` sind Teil des ESP32-Cores.

## Warum pioarduino (Core 3.x)?

Aktuelle Arduino_GFX-Versionen inkludieren `esp32-hal-periman.h`, das es erst ab
Arduino-ESP32 **Core 3.0** gibt. Das offizielle PlatformIO liefert nur Core 2.x.

## WLAN-Konfiguration (Secret-Management)

WLAN-Zugangsdaten liegen in `wifi_secrets.h` (per `#define WIFI_SSID` /
`WIFI_PASSWORD`). Diese Datei ist **gitignored**; im Repo liegt nur
`wifi_secrets.example.h` als Vorlage:

```bash
cp src/ESP32S3_MiniPwn_Dashboard/wifi_secrets.example.h \
   src/ESP32S3_MiniPwn_Dashboard/wifi_secrets.h
```
