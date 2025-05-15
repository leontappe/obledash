#include <Arduino.h>
#include "ELMduino.h"
#include <BLEClientSerial.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#define BAUDRATE 115200

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define FONT_SIZE 2

#define BUTTON_LEFT 25
#define BUTTON_RIGHT 26
#define BUTTON_CENTER 27

typedef struct DisplayData {
  uint32_t rpm = 0;
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
  const char* name = "OBDBLE";
  char localName[] = "OBDLink CX";
  BLESerial.begin(localName, "FFF0", "FFF1", "FFF2");

  if (!BLESerial.connect(name)) {
    handleError("Couldn't connect to ELM327 - Phase 1");
  }

  if (!myELM327.begin(BLESerial, true, 2000, '6', 1024, 100)) {
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

  initBLE();

  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_CENTER, INPUT_PULLUP);

  Serial.println("Fully Started");
}

// list of functions to be called in main loop as strings
char* functions[] = {
  "rpm",
  "ambientTemp",
  "currentFuelConsumption"
};

char* currentFunction;
int currentFunctionIndex = 0;

void loop() {
  /* // Simulate data for testing
  data.rpm = random(1000, 8000);                      // Simulate RPM
  data.ambientTemp = random(20, 40);                  // Simulate temperature
  data.currentFuelConsumption = random(5, 15) / 10.0; // Simulate fuel consumption */

  currentFunction = functions[currentFunctionIndex];

  float tmpData = 0;

  switch (currentFunctionIndex) {
    case 0:
      tmpData = myELM327.rpm();
      break;
    case 1:
      tmpData = myELM327.ambientAirTemp();
      break;
    case 2:
      tmpData = myELM327.fuelRate();
      break;
  }

  if (myELM327.nb_rx_state == ELM_SUCCESS) {
    switch (currentFunctionIndex) {
      case 0:
        displayData.rpm = (uint32_t)tmpData;
        break;
      case 1:
        displayData.ambientTemp = (float_t)tmpData;
        break;
      case 2:
        displayData.currentFuelConsumption = (float_t)tmpData;
        break;
    }

    drawData(displayData);
  } else if (myELM327.nb_rx_state != ELM_GETTING_MSG) {
    myELM327.printError();
  }
}