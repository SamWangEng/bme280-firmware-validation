#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(9600); //means roughly 9600 bits per second are being clocked across the wire, speed of the communication
  while (!Serial) {}
  Serial.println("I2C scanner starting...");
}

void loop() {
  int found = 0;

  // Walk through every possible I2C device address, 0 and 127 are reserved
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();

    if (err == 0) {
      Serial.print("Device found at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }

  if (found == 0) {
    Serial.println("No I2C devices found.");
  }

  Serial.println("---");
  delay(3000);
}
