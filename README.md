<div align="center">

# ESP32S3-MiniPwn-Dashboard

[![CI](https://github.com/G4MEOVER18/ESP32S3-MiniPwn-Dashboard/actions/workflows/platformio-build.yml/badge.svg)](https://github.com/G4MEOVER18/ESP32S3-MiniPwn-Dashboard/actions/workflows/platformio-build.yml)

**ESP32-S3 Status-Dashboard auf 1.47" ST7789-Display (Arduino_GFX)** — WLAN-Verbindung, Uhrzeit und Status auf einen Blick.

[![Plattform](https://img.shields.io/badge/Plattform-ESP32--S3-blue?style=flat-square)](.)
[![Build](https://img.shields.io/badge/Build-PlatformIO%20(pioarduino)-orange?style=flat-square&logo=platformio)](https://platformio.org)
[![Display](https://img.shields.io/badge/Display-ST7789%201.47%22-green?style=flat-square)](.)
[![Lizenz](https://img.shields.io/badge/Lizenz-MIT-green?style=flat-square)](LICENSE)

</div>

---

Kompaktes Display-Dashboard für den ESP32-S3: verbindet sich mit dem WLAN, synchronisiert die Uhrzeit und stellt Status-Informationen auf einem 1.47"-Display dar. WLAN-Zugangsdaten werden **sauber per Header ausgelagert** (`wifi_secrets.h`, gitignored) — keine Credentials im Code.

## Hardware

| Parameter | Wert |
|-----------|------|
| MCU | ESP32-S3 (`esp32-s3-devkitc-1`) |
| Display | ST7789 1.47" über Arduino_GFX (Pins MOSI 45 · SCLK 40 · CS 42 · DC 41 · RST 39 · BL 48) |

## Vor dem Bauen: WLAN-Daten

```bash
cp src/ESP32S3_MiniPwn_Dashboard/wifi_secrets.example.h \
   src/ESP32S3_MiniPwn_Dashboard/wifi_secrets.h
# WIFI_SSID / WIFI_PASSWORD eintragen — wifi_secrets.h ist gitignored
```

## Fertige Firmware

Vorkompiliertes Image unter [**Releases**](https://github.com/G4MEOVER18/ESP32S3-MiniPwn-Dashboard/releases/latest):

```bash
esptool.py --chip esp32s3 write_flash 0x0 ESP32S3-MiniPwn-Dashboard_esp32s3_v1.0.0_merged.bin
```

> Hinweis: Das vorkompilierte Image enthält Platzhalter-WLAN-Daten — für eigenen Betrieb selbst mit gesetzten `wifi_secrets.h` bauen.

## Selbst bauen (PlatformIO)

```bash
pio run                       # bauen
pio run -t upload             # flashen
pio device monitor -b 115200  # serielle Ausgabe
```

Arduino_GFX (aktuell) benötigt Arduino-ESP32 **Core 3.x** (`esp32-hal-periman.h`) → **pioarduino**-Plattform (gepinnt). Details: [`docs/DEPENDENCIES.md`](docs/DEPENDENCIES.md).

## Lizenz

MIT – siehe [LICENSE](LICENSE). © 2026 Yanis Ameseder

---

## Kontakt

**Entwickler:** Yanis Ameseder · **E-Mail:** [g4me.over.18@gmail.com](mailto:g4me.over.18@gmail.com)
