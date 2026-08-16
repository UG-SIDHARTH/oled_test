/*
 * Standalone OLED Hardware Test Sketch (U8g2 version)
 * Use this sketch to verify your SSD1306 OLED display connection (SDA=21, SCL=22, Address=0x3C)
 */

#include <Wire.h>
#include <U8g2lib.h>

#define OLED_SDA 21
#define OLED_SCL 22

// U8g2 Hardware I2C Driver for SSD1306 128x64
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ OLED_SCL, /* data=*/ OLED_SDA);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Starting OLED Hardware Test (U8g2)...");

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!u8g2.begin()) {
    Serial.println(F("U8g2 SSD1306 allocation failed! Check I2C address or SDA/SCL wiring."));
    for (;;);
  }

  Serial.println(F("OLED Hardware Test Passed!"));

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontPosTop();
  u8g2.setDrawColor(1);
  u8g2.clearBuffer();
  u8g2.drawFrame(0, 0, 128, 64);
  u8g2.setCursor(10, 10);
  u8g2.print(F("U8g2 Test OK!"));
  u8g2.setCursor(10, 30);
  u8g2.print(F("ESP32 + SSD1306"));
  u8g2.sendBuffer();
  delay(1500);
}

void loop() {
  static int circleX = 10;
  static int dir = 2;

  u8g2.clearBuffer();
  u8g2.drawFrame(0, 0, 128, 64);
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontPosTop();
  u8g2.setCursor(15, 10);
  u8g2.print(F("oled_test Ready!"));
  
  u8g2.drawDisc(circleX, 42, 6);
  u8g2.sendBuffer();

  circleX += dir;
  if (circleX > 118 || circleX < 10) {
    dir = -dir;
  }
  delay(30);
}
