#include <LiquidCrystal_I2C.h>
#include <MFRC522.h>
#include <SPI.h>
#include <Wire.h>

// ESP32 DevKit wiring matches the main RFIDScanner sketch.
constexpr uint8_t RFID_SS = 5;
constexpr uint8_t RFID_RST = 4;
constexpr uint8_t LCD_ADDRESS = 0x27; // Try 0x3F if your I2C scan finds that address.

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);
MFRC522 rfid(RFID_SS, RFID_RST);
bool lcdFound = false;
bool readerFound = false;

void show(const String &top, const String &bottom) {
  Serial.println(top + " | " + bottom);
  if (!lcdFound) return;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(top.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(bottom.substring(0, 16));
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA, SCL

  Wire.beginTransmission(LCD_ADDRESS);
  lcdFound = Wire.endTransmission() == 0;
  if (lcdFound) {
    lcd.init();
    lcd.backlight();
  } else {
    Serial.println("LCD not found. Check wiring and I2C address.");
  }

  SPI.begin(18, 19, 23, RFID_SS); // SCK, MISO, MOSI, SS
  rfid.PCD_Init();
  delay(50);
  uint8_t version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  readerFound = version != 0x00 && version != 0xFF;
  Serial.print("MFRC522 version register: 0x");
  Serial.println(version, HEX);

  if (readerFound) show("RFID ready", "Tap a card");
  else show("RFID not found", "Check SPI wiring");
}

void loop() {
  if (!readerFound) return;
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  const char hex[] = "0123456789ABCDEF";
  String uid;
  uid.reserve(rfid.uid.size * 2);
  for (uint8_t i = 0; i < rfid.uid.size; ++i) {
    uint8_t byte = rfid.uid.uidByte[i];
    uid += hex[byte >> 4];
    uid += hex[byte & 0x0F];
  }

  Serial.println("Card UID: " + uid);
  // Split long UIDs across both LCD rows; Serial always prints the full UID.
  show("UID:" + uid.substring(0, 12), uid.length() > 12 ? uid.substring(12) : "Remove card");
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(500);
}
