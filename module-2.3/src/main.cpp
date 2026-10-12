#include <Arduino.h>

struct LedConfig {
  static constexpr int redLedpin = 5;
  static constexpr int blueLedPin = 6;
  static constexpr int greenLedPin = 7;

  static constexpr int redLedStep = 200;
  static constexpr int blueLedStep = 500;
  static constexpr int greenLedStep = 1000;

  enum LedState {
    OFF = LOW,
    ON = HIGH
  };
};


bool noBlockingDelay(unsigned long currentMillis, unsigned long &previousMillis, int step) {
  if ((currentMillis - previousMillis) >= (unsigned long)step) {
    previousMillis = currentMillis; 
    return true;
  }
  return false;
}

class Led
{
public:
  int pin;
  bool ledState = false; 
  unsigned long previousMillis = 0; 

  Led(int pin) : pin(pin) {}

  void init()
  {
    pinMode(pin, OUTPUT);
  }

  void ledFlashing(int step)
  {
    unsigned long currentMillis = millis();
    if (noBlockingDelay(currentMillis, previousMillis, step))
    {
      if (ledState)
      {
        set(LedConfig::LedState::OFF);
      }
      else
      {
        set(LedConfig::LedState::ON);
      }
    }
  }

  void set(LedConfig::LedState state)
  {
    digitalWrite(pin, state);
    ledState = (state == HIGH);
  }
};

Led redLed = Led(LedConfig::redLedpin);
Led blueLed = Led(LedConfig::blueLedPin);
Led greenLed = Led(LedConfig::greenLedPin);

void setup() {
  redLed.init();
  blueLed.init();
  greenLed.init();
}

void loop() {
  redLed.ledFlashing(LedConfig::redLedStep);
  blueLed.ledFlashing(LedConfig::blueLedStep);
  greenLed.ledFlashing(LedConfig::greenLedStep);
}