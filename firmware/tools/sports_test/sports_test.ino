/*
  ============================================================
  SPORTS MODE TEST: via Cloudflare Worker proxy
  Standalone sketch — pulls a small, pre-cleaned JSON payload
  from your own proxy instead of ESPN directly. This avoids the
  payload-size and nesting-depth problems we hit parsing ESPN's
  raw scoreboard JSON directly on the ESP32.

  SETUP REQUIRED:
  1. Deploy worker.js to Cloudflare Workers (see comments in that
     file for steps).
  2. Copy your Worker's URL below into PROXY_BASE_URL.
  3. Edit WIFI_SSID / WIFI_PASSWORD below.

  Wiring (same as main project):
    OLED SDA -> GPIO 21
    OLED SCL -> GPIO 22
    Button 1 -> GPIO 18 -> GND (next page)
    Button 2 -> GPIO 19 -> GND (previous page)

  Libraries needed:
    - Adafruit SH110X
    - Adafruit GFX Library
    - ArduinoJson (v6 or v7)
  ============================================================
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---------------- CONFIG ----------------
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// EDIT THIS to your deployed Cloudflare Worker URL (no trailing slash).
// Example: "https://dad-dashboard-sports.yourname.workers.dev"
const char* PROXY_BASE_URL = "https://YOUR-WORKER-NAME.YOUR-SUBDOMAIN.workers.dev";

const char* SPORT_PARAM = "baseball"; // change to test other sports
const char* LEAGUE_PARAM = "mlb";

const unsigned long REFRESH_MS = 15UL * 1000UL; // 15 sec for testing
// ------------------------------------------

// ---------------- DISPLAY ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------------- BUTTONS ----------------
#define BTN1_PIN 18
#define BTN2_PIN 19
const unsigned long DEBOUNCE_MS = 40;

bool btn1Last = HIGH, btn1Stable = HIGH;
bool btn2Last = HIGH, btn2Stable = HIGH;
unsigned long btn1Change = 0, btn2Change = 0;

// ---------------- GAME DATA ----------------
struct Game {
  String matchup;
  String status;
};

const int MAX_GAMES = 10;
Game games[MAX_GAMES];
int gameCount = 0;

int pageIndex = 0;
unsigned long lastFetch = 0;
int refreshCounter = 0;

// ============================================================
void setup() {
  Serial.begin(115200);
  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);

  Wire.begin(21, 22);
  if (!display.begin(0x3C, true)) {
    Serial.println("SH1106 not found.");
  }
  display.clearDisplay();

  showMessage("Connecting WiFi...");
  connectWiFi();

  fetchGames();
  lastFetch = millis();
  drawScreen();
}

// ============================================================
void loop() {
  handleButtons();

  if (WiFi.status() != WL_CONNECTED) {
    showMessage("WiFi lost.\nReconnecting...");
    connectWiFi();
  }

  unsigned long now = millis();
  if (now - lastFetch >= REFRESH_MS) {
    fetchGames();
    lastFetch = now;
    drawScreen();
  }

  delay(20);
}

// ============================================================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect failed.");
  }
}

void showMessage(const char* msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 20);
  display.println(msg);
  display.display();
}

// ============================================================
// BUTTONS
// ============================================================
void handleButtons() {
  unsigned long now = millis();

  bool r1 = digitalRead(BTN1_PIN);
  if (r1 != btn1Last) btn1Change = now;
  if (now - btn1Change > DEBOUNCE_MS && r1 != btn1Stable) {
    btn1Stable = r1;
    if (btn1Stable == LOW) { nextPage(); }
  }
  btn1Last = r1;

  bool r2 = digitalRead(BTN2_PIN);
  if (r2 != btn2Last) btn2Change = now;
  if (now - btn2Change > DEBOUNCE_MS && r2 != btn2Stable) {
    btn2Stable = r2;
    if (btn2Stable == LOW) { prevPage(); }
  }
  btn2Last = r2;
}

void nextPage() {
  if (gameCount == 0) return;
  pageIndex = (pageIndex + 1) % gameCount;
  drawScreen();
}

void prevPage() {
  if (gameCount == 0) return;
  pageIndex = (pageIndex - 1 + gameCount) % gameCount;
  drawScreen();
}

// ============================================================
// DRAWING
// ============================================================
void drawScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  display.setCursor(0, 0);
  display.print("SCORES upd#");
  display.print(refreshCounter);
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SH110X_WHITE);

  if (gameCount == 0) {
    display.setCursor(0, 24);
    display.println("No games\nfound today.");
  } else {
    display.setCursor(0, 18);
    display.println(games[pageIndex].matchup);

    display.setCursor(0, 34);
    display.println(games[pageIndex].status);

    char buf[20];
    snprintf(buf, sizeof(buf), "game %d/%d", pageIndex + 1, gameCount);
    display.setCursor(0, 56);
    display.print(buf);
  }

  display.display();
}

// ============================================================
// FETCH: pulls the small pre-cleaned payload from the proxy
// ============================================================
void fetchGames() {
  if (WiFi.status() != WL_CONNECTED) return;

  String url = String(PROXY_BASE_URL) + "/?sport=" + SPORT_PARAM + "&league=" + LEAGUE_PARAM;

  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);
  int code = http.GET();

  if (code != 200) {
    Serial.printf("HTTP error %d for %s\n", code, url.c_str());
    http.end();
    return;
  }

  // This payload should now be tiny (well under 1KB typically),
  // so no filter, no nesting-limit override, no streaming tricks
  // needed - the small size is the whole point of the proxy.
  String payload = http.getString();
  http.end();

  Serial.printf("Proxy payload length: %d chars\n", payload.length());
  Serial.println(payload); // small enough to just print in full for debugging

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    Serial.print("JSON parse failed: ");
    Serial.println(err.c_str());
    return;
  }

  JsonArray gamesArray = doc["games"].as<JsonArray>();
  int count = 0;
  for (JsonObject g : gamesArray) {
    if (count >= MAX_GAMES) break;
    games[count].matchup = g["matchup"] | "???";
    games[count].status = g["status"] | "???";
    count++;
  }

  gameCount = count;
  if (pageIndex >= gameCount) pageIndex = 0;

  refreshCounter++;
  Serial.printf("Fetched %d games - update #%d\n", gameCount, refreshCounter);
}
