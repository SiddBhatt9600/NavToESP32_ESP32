/*
 * NAV2ESP — receives newline-terminated JSON nav payloads over classic
 * Bluetooth SPP and renders turn-by-turn info on a 1.8" ST7735 TFT (128x160).
 *
 * Wiring (see README):
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

// Forward declarations.
void parseNavPayload(const String& json);
void updateDisplay();
void showConnectInstructions();
void drawConnectionIndicator();

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
BluetoothSerial SerialBT;

String lineBuffer = "";

// Latest parsed nav values.
String currentTurn = "";
String currentRoad = "";
int currentDistM = 0;
int currentEtaMin = 0;
bool currentOffRoute = false;

// Last-drawn values, to avoid redundant redraws.
String lastDrawnTurn = "";
String lastDrawnRoad = "";
int lastDrawnDistM = -1;
int lastDrawnEtaMin = -1;
bool lastDrawnOffRoute = false;

// Connection state tracking.
bool wasConnected = false;
bool everReceivedData = false;
bool showingConnectScreen = true;
unsigned long lastIndicatorDraw = 0;
bool navScreenInitialized = false;

void setup() {
  Serial.begin(115200);

  SerialBT.begin("ESP32_Nav");
  Serial.println("Bluetooth SPP started.");

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  showConnectInstructions();
}

void loop() {
  bool isConnected = SerialBT.hasClient();

  // Connection state just changed.
  if (isConnected != wasConnected) {
    if (isConnected) {
      Serial.println("Client connected.");
      // Don't clear the screen yet — wait for real data so we don't flash
      // an empty nav layout before the first payload arrives.
    } else {
      Serial.println("Client disconnected.");
      everReceivedData = false;
      lineBuffer = "";
      showConnectInstructions();
    }
    wasConnected = isConnected;
  }

  while (SerialBT.available()) {
    char c = SerialBT.read();
    if (c == '\n') {
      parseNavPayload(lineBuffer);
      lineBuffer = "";
    } else if (c != '\r') {
      lineBuffer += c;
    }
  }

  // Refresh the small connection-status dot periodically, independent of
  // nav data arriving — this is what lets the user see a drop even if the
  // phone never sends another "off" payload to make it obvious.
  if (everReceivedData && millis() - lastIndicatorDraw > 500) {
    drawConnectionIndicator();
    lastIndicatorDraw = millis();
  }
}

void showConnectInstructions() {
  showingConnectScreen = true;
  navScreenInitialized = false;  // force a full clear next time nav data arrives
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);

  tft.setCursor(5, 10);
  tft.print("NAV2ESP - Not Connected");

  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(5, 35);
  tft.print("1. Pair phone with");
  tft.setCursor(5, 47);
  tft.print("   \"ESP32_Nav\" in");
  tft.setCursor(5, 59);
  tft.print("   Bluetooth settings");

  tft.setCursor(5, 80);
  tft.print("2. Open the NAV2ESP app");

  tft.setCursor(5, 100);
  tft.print("3. Enter a destination");
  tft.setCursor(5, 112);
  tft.print("   and tap Start");

  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(5, 140);
  tft.print(SerialBT.hasClient() ? "Bluetooth: connected" : "Bluetooth: waiting...");
}

void parseNavPayload(const String& json) {
  JsonDocument doc;
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

  everReceivedData = true;
  showingConnectScreen = false;

  updateDisplay();
}

void updateDisplay() {
  if (!navScreenInitialized) {
    tft.fillScreen(ST77XX_BLACK);
    lastDrawnTurn = "";
    lastDrawnRoad = "";
    lastDrawnDistM = -1;
    lastDrawnEtaMin = -1;
    lastDrawnOffRoute = false;
    navScreenInitialized = true;
  }

  if (currentTurn != lastDrawnTurn) {
    tft.fillRect(0, 0, tft.width(), 40, ST77XX_BLACK);
    tft.setTextSize(3);
    tft.setTextColor(currentOffRoute ? ST77XX_RED : ST77XX_GREEN);
    tft.setCursor(10, 8);
    tft.print(currentTurn);
    lastDrawnTurn = currentTurn;
  }

  if (currentRoad != lastDrawnRoad) {
    tft.fillRect(0, 45, tft.width(), 20, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(5, 48);
    tft.print(currentRoad);
    lastDrawnRoad = currentRoad;
  }

  if (currentDistM != lastDrawnDistM) {
    tft.fillRect(0, 75, tft.width(), 20, ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(5, 78);
    tft.print(currentDistM);
    tft.print(" m");
    lastDrawnDistM = currentDistM;
  }

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

  drawConnectionIndicator();
}

// Small dot in the top-right corner: green = connected, red = disconnected.
// Drawn on top of the nav screen so a mid-navigation drop is visible without
// losing the last-known turn/road/distance info underneath it.
void drawConnectionIndicator() {
  bool connected = SerialBT.hasClient();
  uint16_t color = connected ? ST77XX_GREEN : ST77XX_RED;
  tft.fillCircle(tft.width() - 8, 8, 4, color);
}
