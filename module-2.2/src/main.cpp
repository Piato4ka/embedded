#include <Arduino.h>

struct RelayConfig {
  static constexpr int relayControlPin = 5;
  static constexpr int relayFeedbackPin = 18;
  
  static constexpr unsigned long testInterval = 1000;
  static constexpr unsigned long debounceMicros = 500;

  enum State {
    OFF = LOW,
    ON = HIGH
  };
};

volatile unsigned long lastInterruptTime = 0;
volatile unsigned long activationTimeMicros = 0;
volatile bool feedbackTriggered = false;

unsigned long testStartMicros = 0;
unsigned int measurementCount = 0;
unsigned long totalDelaySum = 0;

bool isMeasuringOn = true;

void IRAM_ATTR handleRelayFeedback() {
  unsigned long currentMicros = micros();
  
  if ((currentMicros - lastInterruptTime) > RelayConfig::debounceMicros) {
    if (!feedbackTriggered) {
      activationTimeMicros = currentMicros;
      feedbackTriggered = true;
    }
    lastInterruptTime = currentMicros;
  }
}

class RelayMeasurement {
public:
  int controlPin;
  int feedbackPin;
  RelayConfig::State currentState;

  RelayMeasurement(int cPin, int fPin) : controlPin(cPin), feedbackPin(fPin), currentState(RelayConfig::State::OFF) {}

  void init() {
    pinMode(controlPin, OUTPUT);
    pinMode(feedbackPin, INPUT_PULLUP);
    digitalWrite(controlPin, currentState);

    attachInterrupt(digitalPinToInterrupt(feedbackPin), handleRelayFeedback, CHANGE);
  }

  void set(RelayConfig::State state) {
    currentState = state;
    digitalWrite(controlPin, state);
  }
};

bool noBlockingDelay(unsigned long currentMillis, unsigned long &previousMillis, unsigned long interval) {
  if ((currentMillis - previousMillis) >= interval) {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}

RelayMeasurement relay(RelayConfig::relayControlPin, RelayConfig::relayFeedbackPin);

unsigned long lastTestMillis = 0;
bool testActive = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  relay.init();
  Serial.println("\n--- Старт вимірювання механічної затримки реле ---");
}

void loop() {
  unsigned long currentMillis = millis();

  if (noBlockingDelay(currentMillis, lastTestMillis, RelayConfig::testInterval) && !testActive) {
    feedbackTriggered = false;
    testActive = true;
    
    if (isMeasuringOn) {
      testStartMicros = micros();
      relay.set(RelayConfig::State::ON);
    } else {
      testStartMicros = micros();
      relay.set(RelayConfig::State::OFF);
    }
  }

  if (testActive && feedbackTriggered) {
    unsigned long mechanicalDelay = activationTimeMicros - testStartMicros;
    
    measurementCount++;
    totalDelaySum += mechanicalDelay;

    if (isMeasuringOn) {
      Serial.printf("Вимірювання %d (УВІМКНЕННЯ): %lu мкс\n", measurementCount, mechanicalDelay);
    } else {
      Serial.printf("Вимірювання %d (ВИМКНЕННЯ): %lu мкс\n", measurementCount, mechanicalDelay);
    }

    testActive = false;

    if (measurementCount >= 10) {
      unsigned long averageDelay = totalDelaySum / 10;
      if (isMeasuringOn) {
        Serial.printf(">>> СЕРЕДНІЙ час УВІМКНЕННЯ реле: %lu мкс (%.2f мс)\n\n", averageDelay, averageDelay / 1000.0);
        isMeasuringOn = false;
      } else {
        Serial.printf(">>> СЕРЕДНІЙ час ВИМКНЕННЯ реле: %lu мкс (%.2f мс)\n\n", averageDelay, averageDelay / 1000.0);
        isMeasuringOn = true;
      }
      
      measurementCount = 0;
      totalDelaySum = 0;
      delay(2000);
    }
  }
}