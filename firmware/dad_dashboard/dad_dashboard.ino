/*
  ============================================================
  DAD DASHBOARD MEMORY BOOK
  ESP32 + SH1106 1.3" OLED (128x64, I2C)

  MODE 1: Dad Memory Book  (offline quotes/messages)
  MODE 2: Live Sports Dashboard
    Page 1: Dodgers (MLB)
    Page 2: Lakers (NBA)
    Page 3: Colts (NFL)
    Page 4: Colts Super Bowl XLI history (multi-screen, offline facts)

  Sports data now comes from a small Cloudflare Worker proxy
  instead of calling ESPN directly. ESPN's raw scoreboard JSON is
  too large/deeply-nested for the ESP32 to parse reliably (hit
  memory, nesting-depth, and buffer-size limits). The proxy does
  that parsing on a real server and returns a tiny flat payload.
  See worker.js for the proxy source + deployment steps.

  Wiring:
    OLED SDA -> GPIO 21
    OLED SCL -> GPIO 22
    Button 1 -> GPIO 18 -> other leg to GND (INPUT_PULLUP)
    Button 2 -> GPIO 19 -> other leg to GND (INPUT_PULLUP)

  Libraries needed (install via Library Manager):
    - Adafruit SH110X
    - Adafruit GFX Library
    - ArduinoJson (by Benoit Blanchon, v6 or v7)
  ============================================================
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---------------- CONFIG: EDIT THESE ----------------
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// EDIT THIS to your deployed Cloudflare Worker URL (no trailing slash).
// Example: "https://dad-dashboard-sports.yourname.workers.dev"
const char* PROXY_BASE_URL = "https://YOUR-WORKER-NAME.YOUR-SUBDOMAIN.workers.dev";

// ESPN team abbreviations used to filter via the proxy (case-insensitive)
const char* DODGERS_ABBR = "LAD";
const char* LAKERS_ABBR  = "LAL";
const char* COLTS_ABBR   = "IND";
// ------------------------------------------------------

// ---------------- DISPLAY SETUP ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------- BUTTON SETUP ----------------
#define BTN1_PIN 18   // short = next page, long = switch mode
#define BTN2_PIN 19   // previous page

const unsigned long LONG_PRESS_MS = 2000;
const unsigned long DEBOUNCE_MS   = 40;

bool btn1LastReading = HIGH;
bool btn1StableState = HIGH;
unsigned long btn1LastChange = 0;
unsigned long btn1PressStart = 0;
bool btn1LongFired = false;

bool btn2LastReading = HIGH;
bool btn2StableState = HIGH;
unsigned long btn2LastChange = 0;

// Hidden combo mode (hold both buttons)
bool superDadShown = false;

// ---------------- STATE MACHINE ----------------
int mode = 0;          // 0 = Dad Quotes, 1 = Sports
int pageIndex = 0;
unsigned long lastSportsUpdate = 0;
const unsigned long SPORTS_REFRESH_MS = 90UL * 1000UL; // 90 seconds
bool wifiConnected = false;

// ---------------- DAD QUOTES DATA ----------------
// ----------------------------------------------------
// CUSTOMIZE THESE — add as many as you like.
// Use \n for line breaks. Fits ~4-5 lines per screen.
// ----------------------------------------------------
const char* dadQuotes[] = {
  "Happy Father's\nDay!\nWe love you.",
  "Kid 1 Says:\nYou taught me\neverything\nI know.",
  "Kid 1 Says:\nThanks for\nalways being\nthere for me.",
  "Kid 1 Says:\nThank you for\ndriving us\neverywhere.",
  "Kid 2 Says:\nYou make the\nbest food.",
  "Kid 2 Says:\nYou work hard\nfor us every\nsingle day.",
  "Kid 2 Says:\nYou always know\nwhen to make\na good joke.",
  "Partner Says:\nLove you!",
  "Partner Says:\nBest dad in\nthe world.",
  "Partner Says:\nYou make great\ncoffee!",
};
const int dadQuotesCount = sizeof(dadQuotes) / sizeof(dadQuotes[0]);

// ---------------- SPORTS DATA (filled by proxy) ----------------
struct TeamScore {
  String teamName;
  String line;       // e.g. "LAD 5 - SD 3"
  String status;     // e.g. "FINAL" / "LIVE Q3" / "7:05 PM"
  bool found;
};

TeamScore dodgersScore = {"DODGERS", "No game data", "--", false};
TeamScore lakersScore  = {"LAKERS",  "No game data", "--", false};
TeamScore coltsScore   = {"COLTS",   "No game data", "--", false};

// ---------------- COLTS SUPER BOWL XLI HISTORY ----------------
// Verified facts: Colts beat the Bears 29-17 on Feb 4, 2007 at
// Dolphin Stadium, Miami Gardens FL. First Colts title since 1971.
// Split across multiple screens so it scrolls with the same buttons.
const char* superBowlScreens[] = {
  "SUPER BOWL XLI\n\nColts 29\nBears 17\nFeb 4, 2007",
  "First Super\nBowl played\nin steady rain\nthroughout.",
  "Bears scored on\nopening kickoff\nreturn, 14 sec\ninto the game.",
  "Colts trailed\n14-6 after Q1,\nthen outscored\nChicago 23-3.",
  "Peyton Manning\nnamed MVP.\n247 yds, 1 TD\npassing.",
  "Colts' first\ntitle since\nSuper Bowl V\nin 1971.",
};
const int superBowlScreenCount = sizeof(superBowlScreens) / sizeof(superBowlScreens[0]);

const int sportsPageCount = 4; // dodgers, lakers, colts, super bowl history
int sbPage = 0; // sub-page index within the Super Bowl history window

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);

  Wire.begin(21, 22); // SDA, SCL (default ESP32 pins)

  if (!display.begin(0x3C, true)) {
    Serial.println("SH1106 not found. Check wiring.");
  }

  display.clearDisplay();
  display.setRotation(0);
  showStartupAnimation();

  connectWiFi();

  drawCurrentScreen();
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  handleButtons();

  if (mode == 1 && wifiConnected) {
    unsigned long now = millis();
    if (lastSportsUpdate == 0 || now - lastSportsUpdate >= SPORTS_REFRESH_MS) {
      fetchAllScores();
      lastSportsUpdate = now;
      drawCurrentScreen();
    }
  }

  delay(20);
}

// ============================================================
// WIFI
// ============================================================
void connectWiFi() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay250WithDots();
  }

  wifiConnected = (WiFi.status() == WL_CONNECTED);

  display.clearDisplay();
  display.setCursor(0, 0);
  if (wifiConnected) {
    display.println("WiFi connected!");
    Serial.println(WiFi.localIP());
  } else {
    display.println("WiFi FAILED.");
    display.println("Sports mode");
    display.println("disabled.");
  }
  display.display();
  delay(1200);
}

void delay250WithDots() {
  display.print(".");
  display.display();
  delay(250);
}

// ============================================================
// BUTTON HANDLING (debounced, with long-press detection)
// ============================================================
void handleButtons() {
  unsigned long now = millis();

  // ---- Button 1 (next page / long-press = mode switch) ----
  bool reading1 = digitalRead(BTN1_PIN);
  if (reading1 != btn1LastReading) {
    btn1LastChange = now;
  }
  if (now - btn1LastChange > DEBOUNCE_MS && reading1 != btn1StableState) {
    btn1StableState = reading1;
    if (btn1StableState == LOW) {
      // pressed
      btn1PressStart = now;
      btn1LongFired = false;
    } else {
      // released
      unsigned long heldFor = now - btn1PressStart;
      if (heldFor < LONG_PRESS_MS && !btn1LongFired) {
        // short press -> next page
        nextPage();
      }
      superDadShown = false;
    }
  }
  btn1LastReading = reading1;

  // While button 1 is held, check for long press firing (mode switch)
  if (btn1StableState == LOW && !btn1LongFired) {
    if (now - btn1PressStart >= LONG_PRESS_MS) {
      btn1LongFired = true;
      switchMode();
    }
  }

  // ---- Button 2 (previous page) ----
  bool reading2 = digitalRead(BTN2_PIN);
  if (reading2 != btn2LastReading) {
    btn2LastChange = now;
  }
  if (now - btn2LastChange > DEBOUNCE_MS && reading2 != btn2StableState) {
    btn2StableState = reading2;
    if (btn2StableState == LOW) {
      prevPage();
    } else {
      superDadShown = false;
    }
  }
  btn2LastReading = reading2;

  // ---- Hidden combo: both buttons held at once ----
  if (btn1StableState == LOW && btn2StableState == LOW && !superDadShown) {
    superDadShown = true;
    showSuperDadMode();
  }
}

void nextPage() {
  if (mode == 0) {
    pageIndex = (pageIndex + 1) % dadQuotesCount;
  } else {
    // Inside the Super Bowl history window: scroll its sub-screens first
    if (pageIndex == 3 && sbPage < superBowlScreenCount - 1) {
      sbPage++;
    } else {
      pageIndex = (pageIndex + 1) % sportsPageCount;
      sbPage = 0;
    }
  }
  drawCurrentScreen();
}

void prevPage() {
  if (mode == 0) {
    pageIndex = (pageIndex - 1 + dadQuotesCount) % dadQuotesCount;
  } else {
    if (pageIndex == 3 && sbPage > 0) {
      sbPage--;
    } else {
      pageIndex = (pageIndex - 1 + sportsPageCount) % sportsPageCount;
      // entering Super Bowl page from the other direction -> land on its last screen
      sbPage = (pageIndex == 3) ? superBowlScreenCount - 1 : 0;
    }
  }
  drawCurrentScreen();
}

void switchMode() {
  mode = (mode == 0) ? 1 : 0;
  pageIndex = 0;
  sbPage = 0;
  showModeTransition();
  if (mode == 1 && wifiConnected && lastSportsUpdate == 0) {
    fetchAllScores();
    lastSportsUpdate = millis();
  }
  drawCurrentScreen();
}

// ============================================================
// DRAWING
// ============================================================
void drawTopBar() {
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print("MODE: ");
  display.println(mode == 0 ? "DAD" : "SPORTS");
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SH110X_WHITE);
}

void drawCurrentScreen() {
  display.clearDisplay();
  drawTopBar();

  if (mode == 0) {
    drawDadQuoteScreen();
  } else {
    drawSportsScreen();
  }

  display.display();
}

void drawDadQuoteScreen() {
  display.setTextSize(1);
  display.setCursor(0, 18);
  display.println(dadQuotes[pageIndex]);

  // bottom: page info
  char buf[20];
  snprintf(buf, sizeof(buf), "%d / %d", pageIndex + 1, dadQuotesCount);
  display.setCursor(0, 56);
  display.print(buf);
}

void drawSportsScreen() {
  if (pageIndex == 3) {
    drawSuperBowlScreen();
    return;
  }

  TeamScore* t;
  if (pageIndex == 0) t = &dodgersScore;
  else if (pageIndex == 1) t = &lakersScore;
  else t = &coltsScore;

  display.setTextSize(1);
  display.setCursor(0, 16);
  display.println(t->teamName);

  display.setCursor(0, 30);
  display.println(t->line);

  display.setCursor(0, 44);
  display.println(t->status);

  // bottom: page indicator + offline note if relevant
  char buf[20];
  snprintf(buf, sizeof(buf), "pg %d/%d", pageIndex + 1, sportsPageCount);
  display.setCursor(0, 56);
  display.print(buf);

  if (!wifiConnected) {
    display.setCursor(60, 56);
    display.print("(offline)");
  }
}

void drawSuperBowlScreen() {
  display.setTextSize(1);
  display.setCursor(0, 16);
  display.println(superBowlScreens[sbPage]);

  char buf[20];
  snprintf(buf, sizeof(buf), "pg %d/%d", sbPage + 1, superBowlScreenCount);
  display.setCursor(0, 56);
  display.print(buf);
}

void showStartupAnimation() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(10, 20);
  display.println("Loading Dad OS...");
  display.setCursor(40, 40);
  display.println("<3");
  display.display();
  delay(1500);
}

void showModeTransition() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 28);
  display.print(mode == 0 ? "DAD MODE" : "SPORTS MODE");
  display.display();
  delay(500);
}

void showSuperDadMode() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 14);
  display.println("** SUPER DAD **");
  display.setCursor(10, 32);
  display.println("SCORE: 100/100");
  display.setCursor(50, 48);
  display.println("<3");
  display.display();
  delay(1500);
  drawCurrentScreen();
}

// ============================================================
// SPORTS DATA — now via the Cloudflare Worker proxy
// ============================================================
void fetchAllScores() {
  fetchScoreFor("baseball", "mlb", DODGERS_ABBR, "DODGERS", dodgersScore);
  fetchScoreFor("basketball", "nba", LAKERS_ABBR, "LAKERS", lakersScore);
  fetchScoreFor("football", "nfl", COLTS_ABBR, "COLTS", coltsScore);
}

// Calls the proxy for one team and fills `result`.
// The proxy already filters to this team and returns a tiny,
// flat JSON payload, so no filter/nesting-limit tricks are
// needed here - that complexity now lives server-side.
void fetchScoreFor(const char* sport, const char* league, const char* teamAbbr,
                    const char* displayName, TeamScore &result) {
  if (WiFi.status() != WL_CONNECTED) return;

  String url = String(PROXY_BASE_URL) + "/?sport=" + sport + "&league=" + league + "&team=" + teamAbbr;

  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("HTTP error %d for %s\n", httpCode, url.c_str());
    http.end();
    result.found = false;
    result.line = "Proxy error";
    result.status = "Check Worker";
    return;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    Serial.print("JSON parse failed: ");
    Serial.println(err.c_str());
    result.found = false;
    result.line = "Parse error";
    result.status = "--";
    return;
  }

  JsonArray gamesArray = doc["games"].as<JsonArray>();

  if (gamesArray.size() == 0) {
    result.found = false;
    result.teamName = displayName;
    result.line = "No game today";
    result.status = "Check back later";
    return;
  }

  // Proxy already filtered to this team, so the first result is it.
  JsonObject g = gamesArray[0];
  result.found = true;
  result.teamName = displayName;
  result.line = g["matchup"] | "--";
  result.status = g["status"] | "--";
}
