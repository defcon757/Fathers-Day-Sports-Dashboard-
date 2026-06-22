# Dad Dashboard 🎁

A Father's Day gift built on an ESP32 and a 1.3" OLED display. Two modes, two buttons, one device.

**Mode 1 — Dad Memory Book:** Personal messages, quotes, and memories from the family, scrollable like a digital scrapbook.

**Mode 2 — Live Sports Dashboard:** Real-time scores for the Dodgers, Lakers, and Colts via ESPN's API, routed through a lightweight Cloudflare Worker proxy so the ESP32 doesn't have to parse huge JSON payloads directly.

Hidden easter egg: hold both buttons at once.

---

## Demo

```
┌──────────────────┐    ┌──────────────────┐
│ MODE: DAD        │    │ MODE: SPORTS      │
│──────────────────│    │──────────────────│
│ Aidan Says:      │    │ DODGERS           │
│ Your great       │    │ LAD 5 - SD 3      │
│ at bass, and I   │    │ LIVE Bot 7th      │
│ dont mean        │    │                   │
│ the fish         │    │ pg 1/4            │
│ 3 / 13           │    │                   │
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
| Power | USB power bank or wall adapter via USB |

### Wiring

```
OLED SDA  →  GPIO 21
OLED SCL  →  GPIO 22
OLED VCC  →  3.3V
OLED GND  →  GND

Button 1  →  GPIO 18 + GND  (INPUT_PULLUP, no resistor needed)
Button 2  →  GPIO 19 + GND  (INPUT_PULLUP, no resistor needed)
```

---

## Controls

| Action | Result |
|--------|--------|
| Button 1 short press | Next page |
| Button 2 short press | Previous page |
| Button 1 hold 2 seconds | Switch mode (Dad ↔ Sports) |
| Hold both buttons | 🎉 Hidden Super Dad mode |

---

## Project Structure

```
dad-dashboard/
├── firmware/
│   ├── dad_dashboard/
│   │   └── dad_dashboard.ino   ← main sketch (upload this)
│   └── tools/
│       ├── button_test/        ← standalone GPIO diagnostic
│       └── sports_test/        ← standalone proxy API test
├── proxy/
│   └── worker.js               ← Cloudflare Worker (deploy this)
├── docs/
│   └── wiring.md               ← detailed wiring reference
├── .gitignore
└── README.md
```

---

## Setup

### 1. Arduino Libraries

Install these via **Arduino IDE → Tools → Manage Libraries**:

- `Adafruit SH110X`
- `Adafruit GFX Library`
- `ArduinoJson` by Benoit Blanchon (v6 or v7)

### 2. Deploy the Cloudflare Worker (free)

The ESP32 talks to a small proxy instead of ESPN directly — ESPN's raw scoreboard JSON is too large and deeply nested for the ESP32's parser. The proxy trims it down to a few hundred bytes.

1. Sign up at [dash.cloudflare.com](https://dash.cloudflare.com) (free)
2. Go to **Workers & Pages → Create → Create Worker**
3. Name it something like `dad-dashboard-sports`
4. Paste the contents of `proxy/worker.js` into the editor
5. Click **Save and Deploy**
6. Your Worker URL will look like: `https://dad-dashboard-sports.yourname.workers.dev`
7. Test it in a browser:
   ```
   https://dad-dashboard-sports.yourname.workers.dev/?sport=baseball&league=mlb&team=LAD
   ```
   You should see a small JSON response with the Dodgers' game (or `"No game today"` if there isn't one).

### 3. Configure the Firmware

Open `firmware/dad_dashboard/dad_dashboard.ino` and edit the config block near the top:

```cpp
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* PROXY_BASE_URL = "https://YOUR-WORKER-NAME.YOUR-SUBDOMAIN.workers.dev";
```

> ⚠️ **Never commit real credentials to a public repo.** The `.gitignore` in this project excludes a `secrets.h` file — see the note in the firmware for how to use it.

### 4. Upload

Select your ESP32 board in Arduino IDE and upload. The device will:
- Show a "Loading Dad OS..." startup screen
- Connect to WiFi (dots appear while connecting)
- Drop into Dad Memory Book mode

---

## Customizing Messages

The Dad Memory Book messages live in an array near the top of `dad_dashboard.ino`:

```cpp
const char* dadQuotes[] = {
  "Happy Father's\nDay! We love you.",
  "Aidan Says: \nYou taught me\neverything I know.",
  // add more here
};
```

Use `\n` for line breaks. The OLED fits about 4–5 lines of small text per screen.

---

## Customizing Teams

Change the ESPN team abbreviations in the config block:

```cpp
const char* DODGERS_ABBR = "LAD";   // MLB
const char* LAKERS_ABBR  = "LAL";   // NBA
const char* COLTS_ABBR   = "IND";   // NFL
```

The proxy supports any sport/league ESPN covers. Format:
```
?sport=baseball&league=mlb&team=LAD
?sport=basketball&league=nba&team=LAL
?sport=football&league=nfl&team=IND
?sport=hockey&league=nhl&team=VGK
```

---

## How the Proxy Works

ESPN's public scoreboard endpoints return 100KB+ JSON responses with deeply nested structures (box scores, player leaders, betting odds, video links). The ESP32 hit three separate failure modes trying to parse this directly: buffer truncation at 60KB, a nesting-depth limit of 10, and chunked transfer encoding interacting badly with ArduinoJson's streaming filter.

The Cloudflare Worker sidesteps all of this: it runs on a real server, fetches ESPN's full response, extracts only the three fields the OLED actually needs (team abbreviations, scores, game state), and returns a flat JSON array that's typically under 500 bytes. The Worker runs on Cloudflare's free tier (100,000 requests/day) and doesn't spin down between requests.

---

## Acknowledgements

- [ESPN Public API](https://gist.github.com/akeaswaran/b48b02f1c94f873c6655e7129910fc3b) — undocumented but publicly accessible scoreboard endpoints
- [ArduinoJson](https://arduinojson.org/) by Benoit Blanchon
- [Adafruit GFX / SH110X](https://github.com/adafruit/Adafruit_SH110x) libraries

---

## License

MIT — do whatever you want with it. If you build one for your own dad, that's the point.
