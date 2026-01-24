# ESP32‑C6 E‑Paper API

This example runs an HTTP API on an ESP32‑C6 and lets you control a WeAct 2.9" e‑paper display (GDEM029C90 / GxEPD2_3C).  
It supports both STA (connect to your Wi‑Fi) and AP fallback (if no Wi‑Fi credentials are set or connection fails).

Project location:
```
bringer/examples/esp32c6-oled-menu
```

---

## Features

- Connects to your Wi‑Fi (or starts AP `EPaper-AP`) and starts an HTTP server.
- Endpoints:
  - `GET  /`       — simple HTML UI (form) for quick testing
  - `GET  /status` — returns JSON with IP, current text and partial-update capability
  - `POST /text`   — set the text and color (JSON or form data)
  - `POST /clear`  — clears the screen (full white)
- Uses partial updates (if supported by the panel) for faster updates.
- Prints timing on serial so you can measure update times.

Code layout
-----------

- `src/` — source files organized into:
  - `drivers/` — hardware drivers:
    - `epaper/` — `display.h`, `display.cpp` — e-paper driver and helpers
    - `oled/` — `oled.h`, `oled.cpp` — SSD1306 driver and status helpers
  - `app/` — application code:
    - `server/` — `server.h`, `server.cpp` — HTTP server and request handlers
    - `ui/` — `ui.h`, `ui.cpp` — OLED-driven menu UI (Prev / Next / Confirm buttons; confirm short = select, confirm long = cancel/back)
    - `controls/` — `controls.h`, `controls.cpp` — button/input controls & callbacks
    - `wifi/` — `wifi.h`, `wifi.cpp` — WiFi connect / AP fallback helpers
  - `utils/` — `base64.h`, `base64.cpp` — base64 decoder used by image uploads
  - `config.h` — pins and behavior flags
  - `main.ino` — minimal bootstrap that wires modules together

---
## OLED menu UI

This project includes a simple menu-driven UI that uses the SSD1306 OLED and three hardware buttons (Prev, Next, Confirm) for quick local control:

- Prev (short press): scroll to previous menu item
- Next (short press): scroll to next menu item
- Confirm (short press): select / activate the current item
- Confirm (long press): go back / cancel

Initial screen (shown on the OLED at startup):
- Home — shows WiFi status and a clock:
  - If WiFi is connected the top line shows the assigned IP address; otherwise it shows "No WiFi".
  - The clock displays HH:MM:SS. When WiFi is available the device attempts to sync time via NTP; if unavailable the display falls back to a local uptime-based clock.
- Enter the Settings menu: press Next (short press).
  - Settings:
    - Partial update: toggles e-paper partial updates (fast updates)
    - Full cleaning: runs a recovery-style full clear (white/black cycles)
- Menu navigation:
  - Prev (short press): scroll to previous entry
  - Next (short press): scroll between entries
  - Confirm (short press): select the current entry
  - Confirm (long press): go back to the Home screen

Note: the OLED is reserved for the menu/home UI by default — status/progress messages from the e-paper module (e.g. "Loading...", "Clearing 1/4") are suppressed so the OLED remains dedicated to the UI.

## Quick setup

1. Edit Wi‑Fi credentials:
   - Open `src/secrets.h` and set `WIFI_SSID` / `WIFI_PASSWORD`.
   - Optional: change AP name/password if you want (`AP_SSID`, `AP_PASSWORD`).

2. Build & upload with PlatformIO (example):
```bash
cd bringer/examples/esp32c6-epaper-api
pio run -e esp32c6-devkitm-1 -t upload --upload-port /dev/ttyACM0
```
(Replace `/dev/ttyACM0` with your device. Use `sudo` if necessary, or fix udev permissions.)

3. Open serial monitor to follow progress and debug:
```bash
pio device monitor -e esp32c6-devkitm-1 --port /dev/ttyACM0 -b 115200
```

If Wi‑Fi connection fails, the ESP will start an AP (SSID `EPaper-AP` by default). Connect to it and use the AP IP (usually `192.168.4.1`).

---

## API Usage

### Set text (JSON)
Set the displayed text and optional color (`"red"`|`"black"`). Optional `forceFull` forces a full refresh instead of partial.

Request:
```bash
curl -X POST http://<esp-ip>/text \
  -H "Content-Type: application/json" \
  -d '{"text":"Hello from API","color":"red","forceFull":false}'
```

Response:
```json
{"status":"ok","text":"Hello from API"}
```

### Set text (form)
You can also POST form data (useful from the HTML UI):
```bash
curl -X POST http://<esp-ip>/text -d "text=Hello&color=black"
```

### Get status
```bash
curl http://<esp-ip>/status
```
Response example:
```json
{"ip":"192.168.1.42","text":"Hello API","partialSupported":true}
```

### Clear display
```bash
# Quick clear (works with GET or POST):
curl http://<esp-ip>/clear

# or explicitly (POST):
curl -X POST http://<esp-ip>/clear
```

### Upload di immagini / matrici
Puoi caricare immagini (o matrici pre-elaborate) e mostrarle sul display tramite l'endpoint `POST /image`. Lo schema JSON atteso è:

```json
{
  "width": 128,
  "height": 296,
  "format": "3c",     // "bw" o "3c"
  "data": "<BASE64>", // base64 dei bitplane (bw: 1 piano; 3c: piano nero + piano rosso concatenati)
  "forceFull": false  // opzionale, se true forza un full update
}
```

Note sul formato dei bitplane (3c):
- I bitplane devono essere raw bytes: primo piano = nero (width*height/8 byte), secondo piano = rosso (stessa dimensione).
- L'ordine è row‑major e ogni byte è MSB‑first (il bit più significativo corrisponde al pixel più a sinistra di ogni gruppo di 8 pixel).
- `format: "bw"` invia un solo piano (nero/bianco) di width*height/8 byte.

Script di aiuto (suggerito)
- Ho aggiunto uno script d'esempio che converte una PNG in 1-bit / 3-color bitplanes e le carica via HTTP:
  - `bringer/examples/esp32c6-epaper-api/tools/upload_image.py`
  - Richiede `Pillow` e `requests` (installali con `pip install pillow requests`)
  - Esempio d'uso:
```bash
python3 tools/upload_image.py --file myimage.png --url http://<esp-ip>/image --format 3c --width 128 --height 296 --force-full
```

Script per generare immagini di test
- Puoi generare un paio di sample direttamente con lo script incluso:
```bash
python3 tools/gen_sample_images.py --out images
```
Questo crea:
- `images/sample_bw.png`  (bianco/nero)
- `images/sample_3c.png`  (contiene aree rosse per testare il canale rosso)

Suggerimenti pratici
- L'upload base64 è semplice e robusto; per file più grandi posso aggiungere supporto chunked o upload binario (multipart).
- Se vedi artefatti dopo l'upload, passa `forceFull=true` per ottenere un aggiornamento completo e rimuovere ghosting.
- Se preferisci, posso integrare il decoding PNG/JPEG direttamente sul dispositivo (richiede librerie aggiuntive e più RAM), oppure migliorare il client Python con opzioni di dithering/threshold per la conversione.


### Upload di immagini / matrici
Puoi caricare immagini (o matrici pre-elaborate) e mostrarle sul display tramite l'endpoint `POST /image`. Lo schema JSON atteso è:

```json
{
  "width": 128,
  "height": 296,
  "format": "3c",     // "bw" o "3c"
  "data": "<BASE64>", // base64 dei bitplane (bw: 1 piano; 3c: piano nero + piano rosso concatenati)
  "forceFull": false  // opzionale, se true forza un full update
}
```

Note sul formato dei bitplane (3c):
- I bitplane devono essere raw bytes: primo piano = nero (width*height/8 bytes), secondo piano = rosso (stessa dimensione).
- L'ordine è row‑major e ogni byte è MSB‑first (il bit più significativo corrisponde al pixel più a sinistra della fetta da 8 pixel).
- `format: "bw"` invia un solo piano (nero/bianco) di width*height/8 byte.

Esempio curl (upload di JSON con base64):
```bash
curl -X POST http://<esp-ip>/image \
  -H "Content-Type: application/json" \
  -d '{"width":128,"height":296,"format":"3c","data":"<BASE64_DATA>","forceFull":true}'
```

Script di aiuto (suggerito)
- Ho aggiunto uno script d'esempio (client) che converte una PNG in 1-bit / 3-color bitplanes e le carica via HTTP:
  - `bringer/examples/esp32c6-epaper-api/tools/upload_image.py`
  - Richiede `Pillow` e `requests` (pip install pillow requests)
  - Esempio:
```bash
python3 tools/upload_image.py --file myimage.png --url http://<esp-ip>/image --format 3c --width 128 --height 296 --force-full
```
- Lo script crea i piani (nero/rosso), li concatena e invia il payload in JSON (base64) al dispositivo.

Suggerimenti pratici
- L'upload base64 è semplice e robusto; per file più grandi puoi inviare in chunk (posso aggiungere un endpoint chunked se vuoi).
- Se vedi artefatti dopo upload, prova a impostare `forceFull=true` per ottenere un aggiornamento completo e rimuovere ghosting.
- Se vuoi, aggiungo la decodifica PNG/JPEG direttamente sul dispositivo (richiede librerie aggiuntive e più RAM), oppure posso migliorare lo script Python per gestire palette, dithering e anti‑aliasing ottimizzati per e‑paper.

---


## Web UI
Open `http://<esp-ip>/` in a browser to use the quick form interface (handy for manual testing).

---

## Notes & Troubleshooting

- Partial updates are much faster for small changes but can leave ghosting over time. If you see artifacts (noise / old pixels):
  1. Enable `ENABLE_FORCE_CLEAR = true` in `src/main.ino` (temporarily),
  2. Rebuild & upload. The firmware will run several full white/black cycles to clean the panel.
  3. When the panel looks correct again, set `ENABLE_FORCE_CLEAR = false` and re-upload to return to fast behavior.

- If the screen is blank:
  - Check wiring: `VCC=3.3V`, `GND`, `SDA (MOSI)=23`, `SCL (SCK)=18`, `CS=5`, `D/C=21`, `RST=2`, `BUSY=4`.
  - Open the serial monitor to see debug lines (connect and check for `displayText:` messages).
  - Try a power cycle (disconnect VCC for a second and reconnect) and re-upload.

- If the API responds but the display doesn't change:
  - Verify the serial output shows `displayText: usedPartial=... time(ms)=...`
  - If partial updates are unsupported/unstable, set `ENABLE_PARTIAL_UPDATE = false` to force full updates.

- Security: This example exposes an unauthenticated HTTP API on your network. Do not use on untrusted networks without adding authentication.

---

## Example integration (simple Python client)
```python
import requests
url = "http://<esp-ip>/text"
r = requests.post(url, json={"text":"Hi from Python", "color":"red"})
print(r.text)
```

---

## Libraries used
- GxEPD2 (display driver)
- ArduinoJson (JSON parsing)
- Built-in ESP32 Arduino WiFi + WebServer

Check the licenses of these libraries before using in production.

---

If you'd like, I can:
- add authentication (API key),
- add an endpoint to control font size or choose different fonts,
- implement a small websocket or SSE for push notifications,
- or make the server support multiple text entries (multi-line layout and wrapping).

Which enhancement would you prefer next?