/*
  ============================================================
  BUTTON DIAGNOSTIC TEST
  Standalone sketch — isolates button issues from OLED/WiFi code.

  Wiring assumed (same as main project):
    Button 1: GPIO 18 -> other leg to GND
    Button 2: GPIO 19 -> other leg to GND
    (using internal INPUT_PULLUP, no external resistors needed)

  Open Serial Monitor at 115200 baud after uploading.
  Press each button and watch the output.
  ============================================================
*/

#define BTN1_PIN 18
#define BTN2_PIN 19

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);

  Serial.println();
  Serial.println("=== BUTTON DIAGNOSTIC TEST ===");
  Serial.println("Resting state should read HIGH (1) for both.");
  Serial.println("Pressing a button should read LOW (0).");
  Serial.println("If a pin is stuck LOW or stuck HIGH, that's your fault.");
  Serial.println("================================");
}

int lastBtn1 = -1;
int lastBtn2 = -1;
unsigned long btn1PressStart = 0;
unsigned long btn2PressStart = 0;

void loop() {
  int btn1 = digitalRead(BTN1_PIN);
  int btn2 = digitalRead(BTN2_PIN);

  // Print raw state every loop so you can see noise/flicker
  Serial.print("BTN1(GPIO18): ");
  Serial.print(btn1);
  Serial.print("   BTN2(GPIO19): ");
  Serial.println(btn2);

  // Detect edges and time presses
  if (btn1 != lastBtn1) {
    if (btn1 == LOW) {
      btn1PressStart = millis();
      Serial.println(">>> BTN1 PRESSED");
    } else {
      Serial.print(">>> BTN1 RELEASED after ");
      Serial.print(millis() - btn1PressStart);
      Serial.println(" ms");
    }
  }

  if (btn2 != lastBtn2) {
    if (btn2 == LOW) {
      btn2PressStart = millis();
      Serial.println(">>> BTN2 PRESSED");
    } else {
      Serial.print(">>> BTN2 RELEASED after ");
      Serial.print(millis() - btn2PressStart);
      Serial.println(" ms");
    }
  }

  lastBtn1 = btn1;
  lastBtn2 = btn2;

  delay(100);
}
