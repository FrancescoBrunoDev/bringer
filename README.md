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

### Available Apps

The device includes several built-in apps accessible through the carousel menu:

- **Home** — WiFi status and clock display
  - Shows IP address when connected, "No WiFi" otherwise
  - Displays HH:MM time (NTP-synced when WiFi available, uptime-based fallback)
  
- **Text** — Text display management
  - View and manage text displayed on the e-paper

- **Beszel** — System monitoring
  - Monitor server stats (CPU, memory, disk, network)
  - Navigate between multiple systems
  - Display detailed stats on e-paper

- **NY Times** — RSS feed reader
  - Read top stories from The New York Times
  - Navigate through headlines on OLED
  - View full article details on e-paper with optimized layout
  - Filters out HTML artifacts

- **Settings** — System configuration
  - Partial update: toggles e-paper partial updates (fast updates)
  - Full cleaning: runs a recovery-style full clear (white/black cycles)

### Menu Navigation

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
pio run -e seeed_xiao_esp32c6 -t upload --upload-port /dev/ttyACM0
```
(Replace `/dev/ttyACM0` with your device. Use `sudo` if necessary, or fix udev permissions.)

3. Open serial monitor to follow progress and debug:
```bash
pio device monitor -e seeed_xiao_esp32c6 --port /dev/ttyACM0 -b 115200
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

### Image uploads / bitplanes
You can upload images (or pre-processed bitplanes) and display them on the panel via the `POST /image` endpoint. Expected JSON schema:

```json
{
  "width": 128,
  "height": 296,
  "format": "3c",     // "bw" or "3c"
  "data": "<BASE64>", // base64 of the bitplanes (bw: 1 plane; 3c: black plane + red plane concatenated)
  "forceFull": false  // optional, if true forces a full update
}
```

Notes on the bitplane format (3c):
- Bitplanes must be raw bytes: first plane = black (width*height/8 bytes), second plane = red (same size).
- Order is row-major and each byte is MSB-first (the most significant bit corresponds to the left-most pixel in each group of 8 pixels).
- `format: "bw"` sends a single black/white plane of width*height/8 bytes.

Helper script (suggested)
- An example script is included to convert a PNG into 1-bit / 3-color bitplanes and upload them via HTTP:
  - `bringer/examples/esp32c6-epaper-api/tools/upload_image.py`
  - Requires `Pillow` and `requests` (install with `pip install pillow requests`)
  - Usage:
```bash
python3 tools/upload_image.py --file myimage.png --url http://<esp-ip>/image --format 3c --width 128 --height 296 --force-full
```

Test image generation
- Generate sample images with the included script:
```bash
python3 tools/gen_sample_images.py --out images
```
This creates:
- `images/sample_bw.png`  (black/white)
- `images/sample_3c.png`  (contains red areas to test the red channel)

Practical tips
- Base64 upload is simple and robust; for larger files I can add chunked or multipart upload support.
- If you see artifacts after an upload, set `forceFull=true` to force a full refresh and remove ghosting.
- If desired, PNG/JPEG decoding could be implemented on-device (requires additional libraries and more RAM), or the Python client can be improved with dithering/threshold options optimized for e-paper.


### Image uploads / bitplanes
You can upload images (or pre-processed bitplanes) and display them via the `POST /image` endpoint. Expected JSON schema:

```json
{
  "width": 128,
  "height": 296,
  "format": "3c",     // "bw" or "3c"
  "data": "<BASE64>", // base64 of the bitplanes (bw: 1 plane; 3c: black plane + red plane concatenated)
  "forceFull": false  // optional, if true forces a full update
}
```

Notes on the bitplane format (3c):
- Bitplanes must be raw bytes: first plane = black (width*height/8 bytes), second plane = red (same size).
- Order is row-major and each byte is MSB-first (the most significant bit corresponds to the left-most pixel in each group of 8 pixels).
- `format: "bw"` sends a single black/white plane of width*height/8 bytes.

curl example (uploading JSON with base64):
```bash
curl -X POST http://<esp-ip>/image \
  -H "Content-Type: application/json" \
  -d '{"width":128,"height":296,"format":"3c","data":"<BASE64_DATA>","forceFull":true}'
```

Helper script (suggested)
- An example client script converts a PNG into 1-bit / 3-color bitplanes and uploads them via HTTP:
  - `bringer/examples/esp32c6-epaper-api/tools/upload_image.py`
  - Requires `Pillow` and `requests` (pip install pillow requests)
  - Example:
```bash
python3 tools/upload_image.py --file myimage.png --url http://<esp-ip>/image --format 3c --width 128 --height 296 --force-full
```
- The script creates the bitplanes (black/red), concatenates them and sends the payload as base64 JSON to the device.

Practical tips
- Base64 upload is simple and robust; for larger files you can upload in chunks (a chunked endpoint could be added).
- If you see artifacts after upload, try setting `forceFull=true` to force a full refresh and remove ghosting.
- Optional: implement PNG/JPEG decoding on-device (requires extra libraries and RAM), or enhance the Python script to handle palettes, dithering and anti-aliasing optimized for e-paper.

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