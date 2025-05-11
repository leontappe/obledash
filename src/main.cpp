#include <Arduino.h>
#include "ELMduino.h"
#include <BLEClientSerial.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#define BAUDRATE 115200

#define DEBUG_PORT Serial
#define ELM_PORT BLESerial

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define FONT_SIZE 2

#define BUTTON_LEFT 25
#define BUTTON_RIGHT 26
#define BUTTON_CENTER 27

typedef struct DisplayData {
  float_t rpm = 0;
  float_t ambientTemp = 0;
  float_t currentFuelConsumption = 0;
} display_data_t;

BLEClientSerial BLESerial;

ELM327 myELM327;
DisplayData displayData = DisplayData();

TFT_eSPI tft = TFT_eSPI();

// some useful screen shortcuts
const int centerX = SCREEN_WIDTH / 2;
const int centerY = SCREEN_HEIGHT / 2;
const int leftPadded = 16;
const int topPadded = SCREEN_HEIGHT - 16;
const int rightPadded = SCREEN_WIDTH - 16;
const int bottomPadded = 16;

void handleError(const char* errorMessage) {
  Serial.println(errorMessage);
  tft.fillScreen(TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.drawCentreString(errorMessage, centerX, centerY, FONT_SIZE);
  while (1); // Halt execution
}

void initBLE() {
  const char* deviceName = "OBDBLE";
  DEBUG_PORT.begin(BAUDRATE);
  ELM_PORT.begin((char*)deviceName);

  if (!ELM_PORT.connect()) {
    handleError("Couldn't connect to ELM327 - Phase 1");
  }

  if (!myELM327.begin(ELM_PORT, true, 2000)) {
    handleError("Couldn't connect to ELM327 - Phase 2");
  }

  Serial.println("Connected to ELM327");
}

void initDisplay() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextSize(FONT_SIZE);

  tft.drawCentreString("Initializing...", centerX, 80, FONT_SIZE);
}

void drawData(display_data_t data) {
  Serial.println("Updating display...");
  Serial.print("RPM: ");
  Serial.print(data.rpm);
  Serial.print("; Fuel Consumption: ");
  Serial.print(data.currentFuelConsumption);
  Serial.print("; Temperature: ");
  Serial.println(data.ambientTemp);

  // Clear TFT screen
  //tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);

  const int offset = 35;
  int textY = 50;

  String tempText = "RPM: " + String(data.rpm);
  tft.drawCentreString(tempText, centerX, textY, FONT_SIZE);

  textY += offset;
  tempText = "Fuel: " + String(data.currentFuelConsumption) + "L/h";
  tft.drawCentreString(tempText, centerX, textY, FONT_SIZE);

  textY += offset;
  tempText = "Temp: " + String(data.ambientTemp) + "C";
  tft.drawCentreString(tempText, centerX, textY, FONT_SIZE);
}

void setup() {
  Serial.begin(BAUDRATE);
  Serial.println("Initializing LCD...");

  initDisplay();

  Serial.println("LCD Initialized");

  Serial.println("Initializing BLE...");

  //initBLE();

  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_CENTER, INPUT_PULLUP);

  Serial.println("Fully Started");
}

void loop() {
  display_data_t data;

  /* // Simulate data for testing
  data.rpm = random(1000, 8000);                      // Simulate RPM
  data.ambientTemp = random(20, 40);                  // Simulate temperature
  data.currentFuelConsumption = random(5, 15) / 10.0; // Simulate fuel consumption */

  float tempRPM = myELM327.rpm();
  if (myELM327.nb_rx_state == ELM_SUCCESS) {
    displayData.rpm = (float_t)tempRPM;
    data.rpm = displayData.rpm;
  } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
    myELM327.printError();
  }

  float tempAmbientTemp = myELM327.ambientAirTemp();
  if (myELM327.nb_rx_state == ELM_SUCCESS) {
    displayData.ambientTemp = (float_t)tempAmbientTemp;
    data.ambientTemp = tempAmbientTemp;
  } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
    myELM327.printError();
  }

  float tempFuelRate = myELM327.fuelRate();
  if (myELM327.nb_rx_state == ELM_SUCCESS) {
    displayData.currentFuelConsumption = (float_t)tempFuelRate;
    data.currentFuelConsumption = displayData.currentFuelConsumption;
  } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
    myELM327.printError();
  }

  drawData(data);
  delay(50);
}