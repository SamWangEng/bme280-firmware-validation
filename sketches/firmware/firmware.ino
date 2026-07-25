#include <Wire.h>
#include <Adafruit_BME280.h>

#define BME280_ADDR 0x76  // use 0x77 if your scanner found the sensor there instead

// Status byte bit flags
#define BIT_SENSOR_DETECTED (1 << 0)  // 0x01
#define BIT_VALUE_IN_RANGE  (1 << 1)  // 0x02
#define BIT_ERROR           (1 << 2)  // 0x04

// BME280's own operating spec (datasheet), used as the sanity check
// for "is this reading plausible" - not a comfortable-room-temp check.
#define TEMP_MIN_C -40.0
#define TEMP_MAX_C  85.0

Adafruit_BME280 bme;

bool hardwareDetected = false;  // set once at boot, reflects real bme.begin() result
bool simulateFailure  = false;  // toggled live by 'F'/'R' serial commands

void setup() {
  Serial.begin(9600);
  while (!Serial) {}

  hardwareDetected = bme.begin(BME280_ADDR);

  Serial.println("Firmware ready. Send 'F' to inject a sensor failure, 'R' to clear it.");
}

void handle_debug_commands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'F' || c == 'f') {
      simulateFailure = true;
      Serial.println("[debug] fault injected: forcing SENSOR_NOT_FOUND");
    } else if (c == 'R' || c == 'r') {
      simulateFailure = false;
      Serial.println("[debug] fault cleared");
    }
  }
}

void loop() {
  handle_debug_commands();

  bool sensorOk = hardwareDetected && !simulateFailure;
  float tempC = NAN;
  bool inRange = false;

  if (sensorOk) {
    tempC = bme.readTemperature();
    inRange = (tempC >= TEMP_MIN_C && tempC <= TEMP_MAX_C);
  }

  uint8_t status = 0;
  if (sensorOk)  status |= BIT_SENSOR_DETECTED;
  if (inRange)   status |= BIT_VALUE_IN_RANGE;
  if (!sensorOk || !inRange) status |= BIT_ERROR;

  Serial.print("STATUS:0x");
  if (status < 16) Serial.print("0");
  Serial.print(status, HEX);

  Serial.print(" TEMP:");
  if (sensorOk) {
    Serial.print(tempC, 1);
  } else {  
    Serial.print("--");
  }

  Serial.print(" ERR:");
  if (!sensorOk) {
    Serial.println("SENSOR_NOT_FOUND");
  } else if (!inRange) {
    Serial.println("OUT_OF_RANGE");
  } else {
    Serial.println("NONE");
  }

  delay(1000);
}
