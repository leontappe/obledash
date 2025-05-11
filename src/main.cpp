#include <Arduino.h>
#include "ELMduino.h"
#include <BLEClientSerial.h>
#include <esp32_smartdisplay.h>

BLEClientSerial BLESerial;

#define DEBUG_PORT Serial
#define ELM_PORT BLESerial

#define BAUDRATE 115200

ELM327 myELM327;

uint32_t rpm = 0;

auto lv_last_tick = millis();

void setup()
{
  // init display
  smartdisplay_init();

  lv_display_t *display = lv_display_get_default();
  // lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);
  // lv_display_set_rotation(display, LV_DISPLAY_ROTATION_180);
  // lv_display_set_rotation(display, LV_DISPLAY_ROTATION_270);

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
  auto const now = millis();
  // Update the ticker
  lv_tick_inc(now - lv_last_tick);
  lv_last_tick = now;
  // Update the UI
  lv_timer_handler();

  float tempRPM = myELM327.rpm();

  if (myELM327.nb_rx_state == ELM_SUCCESS)
  {
    rpm = (uint32_t)tempRPM;
    Serial.print("RPM: ");
    Serial.println(rpm);
  }
  else if (myELM327.nb_rx_state != ELM_GETTING_MSG)
  {
    myELM327.printError();
  }
}