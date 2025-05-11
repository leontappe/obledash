# proof of concept using the weird ESP32/LCD/button board i have around

The board has am esp32doit-devkit-v1, 240x240 ST7789V LCD, and 3 front facing buttons.

Buttons are connected as follows:

- left button: GPIO 25
- right button: GPIO 26
- center button: GPIO 27

All buttons are pulled up, so they are LOW when pressed.

The LCD is connected as follows:

Backlight: GPIO 19
DC: GPIO 4
RST/RES: GPIO 22
MOSI/SDA: GPIO 23
CLK: GPIO 18
CS: GPIO 5 

The LCD is a 240x240 ST7789V, and the ESP32 is connected to it using SPI.
