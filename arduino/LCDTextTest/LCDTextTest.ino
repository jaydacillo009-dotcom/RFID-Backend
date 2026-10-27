#include <LiquidCrystal_I2C.h>
#include <Wire.h>

// ESP32 DevKit: SDA = GPIO 21, SCL = GPIO 22.
// Change 0x27 to 0x3F if that is your LCD backpack's address.
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Hello, world!");
  lcd.setCursor(0, 1);
  lcd.print("LCD is working");
}

void loop() {
  // Keep the text on screen.
}
