#include <Arduino.h>

const int SENSOR_PIN = 6; // фоторезистор
const int RELAY_PIN = 4;  // транзистор

const int THRESHOLD_DARK = 2200;
const int THRESHOLD_LIGHT = 2800;

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  // встановлення роздільної здатності АЦП
  analogReadResolution(12);
}

void loop()
{
  unsigned int sensorInput = analogReadMilliVolts(SENSOR_PIN);
  Serial.printf("sensorInput = %d \n", sensorInput);

  if (sensorInput > THRESHOLD_DARK)
  {
    digitalWrite(RELAY_PIN, LOW);
  }
  else if (sensorInput < THRESHOLD_LIGHT)
  {
    digitalWrite(RELAY_PIN, HIGH);
  }
  delay(100);
}