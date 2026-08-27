/*
 * NAV2ESP — receives newline-terminated JSON nav payloads over classic
 * Bluetooth SPP and renders turn-by-turn info on a 1.8" ST7735 TFT (128x160).
 *
 * Wiring (Also available in README):
 *   LED -> 3V3   SCK -> GPIO18   SDA -> GPIO23   A0 -> GPIO2
 *   RESET -> GPIO4   CS -> GPIO5   GND -> GND   VCC -> 3V3
 */

#include <Arduino.h>
#include "BluetoothSerial.h"
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// --- Pin definitions ---
#define TFT_CS   5
#define TFT_RST  4
#define TFT_DC   2
// SCK (18) and MOSI/SDA (23) are hardware SPI pins — the library uses them
// automatically, no need to pass them into the constructor.

// Forward declarations — required since these functions are defined
// later in the file but called earlier (in loop()).
void parseNavPayload(const String& json);
void updateDisplay();

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
BluetoothSerial SerialBT;

String lineBuffer = "";

// Latest parsed values.
String currentTurn = "";
String currentRoad = "";
int currentDistM = 0;
int currentEtaMin = 0;
bool currentOffRoute = false;

// Track previous values so we only redraw what changed — avoids full-screen
// flicker on every update.
String lastDrawnTurn = "";
String lastDrawnRoad = "";
int lastDrawnDistM = -1;
int lastDrawnEtaMin = -1;
bool lastDrawnOffRoute = false;
bool firstDraw = true;

void setup() {
  Serial.begin(115200);

  SerialBT.begin("ESP32_Nav");
  Serial.println("Bluetooth SPP started, waiting for connection...");

  tft.initR(INITR_BLACKTAB); // most common variant for this board; see note below if colors look wrong
  tft.setRotation(1);        // landscape — adjust 0-3 to taste
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.print("Waiting for navigation...");
}

void loop() {
  while (SerialBT.available()) {
    char c = SerialBT.read();
    if (c == '\n') {
      parseNavPayload(lineBuffer);
      lineBuffer = "";
    } else if (c != '\r') {
      lineBuffer += c;
    }
  }
}

void parseNavPayload(const String& json) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, json);

  if (err) {
    Serial.print("JSON parse failed: ");
    Serial.println(err.c_str());
    return;
  }

  currentTurn = doc["turn"] | "";
  currentRoad = doc["road"] | "";
  currentDistM = doc["dist"] | 0;
  currentEtaMin = doc["eta"] | 0;
  currentOffRoute = doc["off"] | false;

  Serial.printf(
    "Turn: %s | Road: %s | Dist: %dm | ETA: %d min | OffRoute: %s\n",
    currentTurn.c_str(), currentRoad.c_str(), currentDistM,
    currentEtaMin, currentOffRoute ? "true" : "false"
  );

  updateDisplay();
}

void updateDisplay() {
  if (firstDraw) {
    tft.fillScreen(ST77XX_BLACK);
    firstDraw = false;
  }

  // --- Turn indicator (big text, top area) ---
  if (currentTurn != lastDrawnTurn) {
    tft.fillRect(0, 0, tft.width(), 40, ST77XX_BLACK);
    tft.setTextSize(3);
    tft.setTextColor(currentOffRoute ? ST77XX_RED : ST77XX_GREEN);
    tft.setCursor(10, 8);
    tft.print(currentTurn);
    lastDrawnTurn = currentTurn;
  }

  // --- Road name ---
  if (currentRoad != lastDrawnRoad) {
    tft.fillRect(0, 45, tft.width(), 20, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(5, 48);
    tft.print(currentRoad);
    lastDrawnRoad = currentRoad;
  }

  // --- Distance to turn ---
  if (currentDistM != lastDrawnDistM) {
    tft.fillRect(0, 75, tft.width(), 20, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(5, 78);
    tft.print(currentDistM);
    tft.print(" m");
    lastDrawnDistM = currentDistM;
  }

  // --- ETA ---
  if (currentEtaMin != lastDrawnEtaMin) {
    tft.fillRect(0, 100, tft.width(), 20, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(5, 103);
    tft.print("ETA ");
    tft.print(currentEtaMin);
    tft.print(" min");
    lastDrawnEtaMin = currentEtaMin;
  }

  // --- Off-route banner ---
  if (currentOffRoute != lastDrawnOffRoute) {
    tft.fillRect(0, 125, tft.width(), 15, currentOffRoute ? ST77XX_RED : ST77XX_BLACK);
    if (currentOffRoute) {
      tft.setTextSize(1);
      tft.setTextColor(ST77XX_WHITE);
      tft.setCursor(5, 128);
      tft.print("OFF ROUTE - rerouting...");
    }
    lastDrawnOffRoute = currentOffRoute;
  }
}
