#include <Arduino.h>
#include "ELMduino.h"
#include <BLEClientSerial.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#define BAUDRATE 115200

#define DEBUG_PORT Serial
#define ELM_PORT BLESerial

// Touchscreen pins
#define XPT2046_IRQ 36  // T_IRQ
#define XPT2046_MOSI 32 // T_DIN
#define XPT2046_MISO 39 // T_OUT
#define XPT2046_CLK 25  // T_CLK
#define XPT2046_CS 33   // T_CS

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define FONT_SIZE 2

typedef struct DisplayData
{
  float_t rpm = 0;
  float_t ambientTemp = 0;
  float_t currentFuelConsumption = 0;
} display_data_t;

BLEClientSerial BLESerial;

ELM327 myELM327;
DisplayData displayData = DisplayData();

TFT_eSPI tft = TFT_eSPI();

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// some useful screen shortcuts
const int centerX = SCREEN_WIDTH / 2;
const int centerY = SCREEN_HEIGHT / 2;
const int leftPadded = 16;
const int topPadded = SCREEN_HEIGHT - 16;
const int rightPadded = SCREEN_WIDTH - 16;
const int bottomPadded = 16;

// Touchscreen coordinates: (x, y) and pressure (z)
int x, y, z;

// Print Touchscreen info about X, Y and Pressure (Z) on the Serial Monitor
void printTouchToSerial(int touchX, int touchY, int touchZ)
{
  Serial.print("X = ");
  Serial.print(touchX);
  Serial.print(" | Y = ");
  Serial.print(touchY);
  Serial.print(" | Pressure = ");
  Serial.print(touchZ);
  Serial.println();
}

void drawData(display_data_t data)
{
  // Clear TFT screen
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);

  int textY = 80;

  String tempText = "RPM: " + String(data.rpm);
  tft.drawCentreString(tempText, leftPadded, textY, FONT_SIZE);

  textY += 20;
  tempText = "Fuel Consumption: " + String(data.currentFuelConsumption);
  tft.drawCentreString(tempText, leftPadded, textY, FONT_SIZE);

  textY += 20;
  tempText = "Temperature: " + String(data.ambientTemp);
  tft.drawCentreString(tempText, leftPadded, textY, FONT_SIZE);
}

void onTouch()
{
  // Get Touchscreen points
  TS_Point p = touchscreen.getPoint();
  // Calibrate Touchscreen points with map function to the correct width and height
  x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
  y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
  z = p.z;

  printTouchToSerial(x, y, z);

  delay(100);
}

void setup()
{
  // Start the SPI for the touchscreen and init the touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  // Set the Touchscreen rotation in landscape mode
  // Note: in some displays, the touchscreen might be upside down, so you might need to set the rotation to 3: touchscreen.setRotation(3);
  touchscreen.setRotation(1);

  // Start the tft display
  tft.init();
  // Set the TFT display rotation in landscape mode
  tft.setRotation(1);

  // Clear the screen before writing to it
  tft.fillScreen(TFT_WHITE);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);

  // init BLE
  const char *deviceName = "OBDBLE";
  DEBUG_PORT.begin(BAUDRATE);
  ELM_PORT.begin((char *)deviceName);

  if (!ELM_PORT.connect())
  {
    DEBUG_PORT.println("Couldn't connect to OBD scanner - Phase 1");
    while (1)
      ;
  }

  if (!myELM327.begin(ELM_PORT, true, 2000))
  {
    Serial.println("Couldn't connect to OBD scanner - Phase 2");
    while (1)
      ;
  }

  Serial.println("Connected to ELM327");
}

void loop()
{
  // Checks if Touchscreen was touched, and prints X, Y and Pressure (Z) info on the TFT display and Serial Monitor
  if (touchscreen.tirqTouched() && touchscreen.touched())
  {
    onTouch();
  }

  display_data_t data = DisplayData();

  float tempRPM = myELM327.rpm();
  if (myELM327.nb_rx_state == ELM_SUCCESS)
  {
    displayData.rpm = (float_t)tempRPM;
    data.rpm = displayData.rpm;
  }
  else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
  {
    myELM327.printError();
  }

  float tempAmbientTemp = myELM327.ambientAirTemp();
  if (myELM327.nb_rx_state == ELM_SUCCESS)
  {
    displayData.ambientTemp = (float_t)tempAmbientTemp;
    data.ambientTemp = tempAmbientTemp;
  }
  else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
  {
    myELM327.printError();
  }

  float tempFuelRate = myELM327.fuelRate();
  if (myELM327.nb_rx_state == ELM_SUCCESS)
  {
    displayData.currentFuelConsumption = (float_t)tempFuelRate;
    data.currentFuelConsumption = displayData.currentFuelConsumption;
  }
  else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
  {
    myELM327.printError();
  }

  drawData(data);
}