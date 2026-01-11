#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) {}
  }

  String text = "You\n  won!";

  display.clearDisplay();

  display.setTextSize(3);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(SCREEN_WIDTH/4, 0);
  display.println(text);

  display.display();
}

void loop() {
}
