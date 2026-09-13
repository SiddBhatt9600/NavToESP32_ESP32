# NAV2ESP — ESP32 Firmware

The display-side half of NAV2ESP: PlatformIO firmware for an ESP32 that receives turn-by-turn navigation data over classic Bluetooth and renders it on a small SPI TFT. No map data, routing, or network connection lives on the ESP32 — it just parses JSON and draws.

The Android app that drives this is a separate repo: https://github.com/SiddBhatt9600/NavToESP32_Android.

<img width="900" height="1193" alt="esp32" src="https://github.com/user-attachments/assets/d29368b8-4dac-48bc-87a5-d67130e44ec2" />

## How it works

```
Phone (routing + GPS tracking)
        │
        ▼
JSON payload over Bluetooth SPP
        │
        ▼
ESP32 parses JSON ──▶ renders on TFT
```

## Requirements

- Original ESP32 board (classic Bluetooth SPP requires BR/EDR — **not** supported on ESP32-S3/C3/H2, which are BLE-only)
- 1.8" ST7735 SPI TFT (128x160)
- [PlatformIO](https://platformio.org/)

## Hardware wiring

| TFT pin | ESP32 GPIO |
|---|---|
| LED  | 3V3 |
| SCK  | GPIO18 |
| SDA  | GPIO23 |
| A0   | GPIO2 |
| RESET | GPIO4 |
| CS   | GPIO5 |
| GND  | GND |
| VCC  | 3V3 |

SCK/SDA use the ESP32's hardware VSPI pins for full-speed SPI. This board is 3.3V logic — do not connect VCC to 5V.

Note: SCK/SDA not to be confused with I2C. Here SCK is clock, SDA is MOSI and CS exists at GPIO5. MISO is not used for this as we are not taking any input from display to ESP.

If the display shows a wrong-colored or noisy image on first boot, try a different init tab variant in `main.cpp` (`INITR_GREENTAB`, `INITR_REDTAB`, etc.) — cheap ST7735 clones vary and this is the most common fix.

## Setup

1. Clone this repo and open it in PlatformIO.
2. Wire the TFT as above.
3. Build and upload:
   ```
   pio run --target upload
   ```
4. Open the serial monitor to confirm it booted correctly:
   ```
   pio device monitor
   ```
   You should see `Bluetooth SPP started.`
5. On your phone, go to Bluetooth settings and pair with **ESP32_Nav**. This is a one-time step, done outside any app.

## Display states

- **Not connected** — shows numbered setup instructions (pair the phone, open the app, enter a destination and start navigating) instead of a blank "waiting" message.
- **Connected, navigating** — shows the current turn, road name, distance to the turn, and ETA. A small dot in the top-right corner is green while the Bluetooth connection is live.
- **Connection drops mid-navigation** — the last-known nav data stays on screen (so you're not left with nothing), but the corner dot turns red and the device falls back to the instruction screen once you stop receiving data, so a real disconnect is never silently invisible.
- **Off-route** — the turn indicator turns red and a banner reads "OFF ROUTE - rerouting..." while the phone recalculates.

## JSON payload format

One newline-terminated JSON object per update, read over Bluetooth SPP:

```json
{"turn":"L","road":"MG Road","dist":120,"eta":5,"off":false}
```

| Key | Meaning |
|---|---|
| `turn` | Maneuver code (`L`, `R`, `STRAIGHT`, `DEPART`, `ARRIVE`, etc.) |
| `road` | Road name for the current step |
| `dist` | Distance to the next turn, in meters |
| `eta`  | Estimated minutes remaining |
| `off`  | Whether the phone currently thinks it's off-route |

Parsed with [ArduinoJson](https://arduinojson.org/) v7.

## Roadmap

- BLE receiver variant, for S3/C3/H2 boards
- Wi-Fi/WebSocket receiver as an alternative to Bluetooth SPP
- On-display mini-map rendering (vector route overlay, sent from the phone)
- Offline map tiles read from the onboard SD card slot, for areas with no phone connectivity
