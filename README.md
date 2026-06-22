# Dad Dashboard 🎁

A two-mode interactive gift device built on an ESP32 and a 1.3" OLED display — a scrollable family message book and a live sports scoreboard, all in a pocket-sized package controlled by two buttons.

---

## What It Does

### Mode 1 — Memory Book
Scrollable personal messages, quotes, and notes from the family. Fully offline — no internet needed for this mode. Customize the messages directly in the firmware.

### Mode 2 — Live Sports Dashboard
Real-time scores for three teams via ESPN's public API, routed through a lightweight Cloudflare Worker proxy. Shows live scores, upcoming game times, and final results. Also includes a multi-screen historical highlight page that scrolls with the same buttons.

Hold both buttons at once for a hidden easter egg.

---

## Demo

```
┌──────────────────┐    ┌──────────────────┐
│ MODE: DAD        │    │ MODE: SPORTS      │
│──────────────────│    │──────────────────│
│ Kid 1 Says:      │    │ TEAM A            │
│ Thanks for       │    │ TMA 5 - TMB 3     │
│ always being     │    │ LIVE Bot 7th      │
│ there for me.    │    │                   │
│ 3 / 10           │    │ pg 1/4            │
└──────────────────┘    └──────────────────┘
```

---

## Hardware

| Part | Details |
|------|---------|
| Microcontroller | ESP32 (any standard dev board) |
| Display | SH1106 1.3" OLED, 128×64, I2C |
| Button 1 | Momentary tactile switch |
| Button 2 | Momentary tactile switch |
| Power | USB power bank or wall adapter |

### Wiring

```
OLED SDA  →  GPIO 21
OLED SCL  →  GPIO 22
OLED VCC  →  3.3V
OLED GND  →  GND

Button 1  →  GPIO 18 + GND  (INPUT_PULLUP, no resistor needed)
Button 2  →  GPIO 19 + GND  (INPUT_PULLUP, no resistor needed)
```

See `docs/wiring.md` for a detailed pin reference and common troubleshooting notes.

---

## Controls

| Action | Result |
|--------|--------|
| Button 1 short press | Next page |
| Button 2 short press | Previous page |
| Button 1 hold 2 seconds | Switch mode (Memory Book ↔ Sports) |
| Hold both buttons | Hidden easter egg |

---

## Project Structure

```
dad-dashboard/
├── firmware/
│   ├── dad_dashboard/
│   │   └── dad_dashboard.ino   ← main sketch — upload this
│   └── tools/
│       ├── button_test/        ← standalone GPIO diagnostic sketch
│       └── sports_test/        ← standalone proxy connectivity test
├── proxy/
│   └── worker.js               ← Cloudflare Worker — deploy this
├── docs/
│   └── wiring.md
└── README.md
```

---

## Setup

### 1. Arduino Libraries

Install via **Arduino IDE → Tools → Manage Libraries**:

- `Adafruit SH110X`
- `Adafruit GFX Library`
- `ArduinoJson` by Benoit Blanchon (v6 or v7)

### 2. Deploy the Cloudflare Worker

ESPN's raw scoreboard API returns 100KB+ deeply nested JSON responses. The ESP32 cannot parse these reliably — it hits buffer truncation limits, nesting depth limits, and chunked transfer encoding issues with ArduinoJson's stream filter. The Cloudflare Worker runs server-side, fetches ESPN's response, extracts only what the display needs, and returns a flat payload typically under 500 bytes.

Cloudflare Workers are free (100,000 requests/day — well above what a 90-second refresh cycle needs).

1. Sign up at [dash.cloudflare.com](https://dash.cloudflare.com) (free)
2. Go to **Workers & Pages → Create → Create Worker**
3. Give it a name (e.g. `dad-dashboard-sports`)
4. Paste the contents of `proxy/worker.js` into the editor
5. Click **Save and Deploy**
6. Test it in a browser before touching the firmware:
   ```
   https://YOUR-WORKER.YOUR-SUBDOMAIN.workers.dev/?sport=baseball&league=mlb&team=LAD
   ```
   You should see a small JSON response. If you do, the hard part is done.

### 3. Configure the Firmware

Open `firmware/dad_dashboard/dad_dashboard.ino` and fill in the config block at the top:

```cpp
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* PROXY_BASE_URL = "https://YOUR-WORKER.YOUR-SUBDOMAIN.workers.dev";
```

> ⚠️ Do not commit real credentials to a public repo.

### 4. Upload

Select your ESP32 board in Arduino IDE and upload. On first boot it will connect to WiFi, then drop into Memory Book mode. Switch to Sports mode with a long press of Button 1.

---

## Customization

### Messages

Edit the `dadQuotes[]` array in the firmware. Each entry is a string displayed on one screen:

```cpp
const char* dadQuotes[] = {
  "Happy Father's\nDay!\nWe love you.",
  "Kid 1 Says:\nThanks for\nalways being\nthere for me.",
  // add as many as you like
};
```

Use `\n` for line breaks. Fits roughly 4–5 lines of small text per screen.

### Teams

Change the ESPN abbreviations in the config block to follow any three teams:

```cpp
const char* DODGERS_ABBR = "LAD";   // MLB
const char* LAKERS_ABBR  = "LAL";   // NBA
const char* COLTS_ABBR   = "IND";   // NFL
```

The proxy supports any sport ESPN covers. URL format:
```
?sport=baseball&league=mlb&team=LAD
?sport=basketball&league=nba&team=LAL
?sport=football&league=nfl&team=IND
?sport=hockey&league=nhl&team=VGK
```

### Historical Highlight Page

The fourth sports page is a multi-screen historical highlight that scrolls with the same buttons. Edit the `superBowlScreens[]` array in the firmware to change it to any event you like — a championship game, a milestone season, anything that fits the theme.

---

## How the Proxy Works

```
ESP32  →  Cloudflare Worker  →  ESPN API
              ↓
       Extracts 3 fields
       (teams, scores, state)
              ↓
       Returns ~500 bytes
              ↓
         ESP32 parses
         with ease
```

The Worker accepts `sport`, `league`, and `team` query parameters and handles sorting (live games first, then upcoming, then finals) before the response ever reaches the device.

---

## Diagnostic Tools

Two standalone sketches are included in `firmware/tools/` for testing during development:

**`button_test/`** — prints raw GPIO state to Serial Monitor at 115200 baud. Useful for isolating wiring issues before adding display or network code.

**`sports_test/`** — connects to the proxy and prints the full JSON response to Serial Monitor. Useful for confirming the Worker is deployed correctly before integrating it into the main firmware.

---

## Acknowledgements

- [ESPN Public API](https://gist.github.com/akeaswaran/b48b02f1c94f873c6655e7129910fc3b) — undocumented but publicly accessible scoreboard endpoints
- [ArduinoJson](https://arduinojson.org/) by Benoit Blanchon
- [Adafruit GFX / SH110X](https://github.com/adafruit/Adafruit_SH110x) libraries
