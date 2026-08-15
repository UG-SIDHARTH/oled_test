/*
 * Standalone OLED Hardware Test Sketch
 * Use this sketch to verify your SSD1306 OLED display connection (SDA=21, SCL=22, Address=0x3C)
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define OLED_SDA 21
#define OLED_SCL 22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting OLED Hardware Test...");

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed! Check I2C address or SDA/SCL wiring."));
    for (;;);
  }

  Serial.println(F("OLED Hardware Test Passed!"));

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.println(F("OLED Test OK!"));
  display.setCursor(10, 30);
  display.println(F("ESP32 + SSD1306"));
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.display();
}

void loop() {
  static int circleX = 10;
  static int dir = 2;

  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 10);
  display.println(F("oled_test Ready!"));
  
  display.fillCircle(circleX, 40, 6, SSD1306_WHITE);
  display.display();

  circleX += dir;
  if (circleX > 118 || circleX < 10) {
    dir = -dir;
  }
  delay(30);
}
