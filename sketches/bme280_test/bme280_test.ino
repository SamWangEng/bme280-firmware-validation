#include <Wire.h>
#include <Adafruit_BME280.h>

#define BME280_ADDR 0x76  // use 0x77 if your scanner found the sensor there instead

Adafruit_BME280 bme;

void setup() {
  Serial.begin(9600);
  while (!Serial) {}

  if (!bme.begin(BME280_ADDR)) {
    Serial.println("Could not find BME280 - check wiring/address!");
    while (1) {}  // halt here so the failure is obvious
  }

  Serial.println("BME280 found, starting readings...");
}

void loop() {
  float tempC = bme.readTemperature();
  Serial.print("TEMP:");
  Serial.println(tempC);
  delay(1000);
}
