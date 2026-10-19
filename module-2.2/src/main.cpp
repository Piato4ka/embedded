#include <Arduino.h>

struct RelayConfig {
  static constexpr int relayControlPin = 5;
  static constexpr int relayFeedbackPin = 18;
  static constexpr bool activeLow = false;

  static constexpr unsigned long testInterval = 1000;
  static constexpr unsigned long settleMicros = 5000;
  static constexpr unsigned long timeoutMicros = 100000;
  static constexpr unsigned int measurementsRequired = 10;
  static constexpr int maxErrors = 5;

  enum State {
    OFF = LOW,
    ON = HIGH
  };
};

portMUX_TYPE feedbackMux = portMUX_INITIALIZER_UNLOCKED;

volatile bool feedbackArmed = false;
volatile unsigned long firstEdgeMicros = 0;
volatile unsigned long lastEdgeMicros = 0;
volatile unsigned long edgeCount = 0;

unsigned long testStartMicros = 0;
unsigned int onCount = 0;
unsigned int offCount = 0;
unsigned long onDelaySum = 0;
unsigned long offDelaySum = 0;
unsigned long onBounceSum = 0;
unsigned long offBounceSum = 0;
int errorCount = 0;

void IRAM_ATTR handleRelayFeedback() {
  unsigned long currentMicros = micros();

  portENTER_CRITICAL_ISR(&feedbackMux);
  if (feedbackArmed) {
    if (edgeCount == 0) {
      firstEdgeMicros = currentMicros;
    }
    lastEdgeMicros = currentMicros;
    edgeCount++;
  }
  portEXIT_CRITICAL_ISR(&feedbackMux);
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
    set(currentState);

    attachInterrupt(digitalPinToInterrupt(feedbackPin), handleRelayFeedback, CHANGE);
  }

  void set(RelayConfig::State state) {
    currentState = state;
    bool level = (state == RelayConfig::State::ON) != RelayConfig::activeLow;
    digitalWrite(controlPin, level ? HIGH : LOW);
  }

  RelayConfig::State readContact() {
    return digitalRead(feedbackPin) == LOW ? RelayConfig::State::ON : RelayConfig::State::OFF;
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
bool testFinished = false;

void armFeedback(bool armed) {
  portENTER_CRITICAL(&feedbackMux);
  if (armed) {
    edgeCount = 0;
  }
  feedbackArmed = armed;
  portEXIT_CRITICAL(&feedbackMux);
}

void handleError() {
  relay.set(relay.readContact());
  errorCount++;

  if (errorCount >= RelayConfig::maxErrors) {
    Serial.println("Забагато помилок підряд, вимірювання зупинено");
    relay.set(RelayConfig::State::OFF);
    testFinished = true;
  }
}

void printAverage(const char *stateName, unsigned long delaySum, unsigned long bounceSum, unsigned int count) {
  unsigned long averageDelay = delaySum / count;
  Serial.printf(">>> СЕРЕДНІЙ час %s реле: %lu мкс (%.2f мс), брязкіт %lu мкс\n",
                stateName, averageDelay, averageDelay / 1000.0, bounceSum / count);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  relay.init();
  Serial.println("\n--- Старт вимірювання механічної затримки реле ---");
}

void loop() {
  if (testFinished) {
    return;
  }

  unsigned long currentMillis = millis();

  if (!testActive && noBlockingDelay(currentMillis, lastTestMillis, RelayConfig::testInterval)) {
    RelayConfig::State nextState = (relay.currentState == RelayConfig::State::ON)
                                       ? RelayConfig::State::OFF
                                       : RelayConfig::State::ON;
    armFeedback(true);
    testActive = true;
    testStartMicros = micros();
    relay.set(nextState);
  }

  if (!testActive) {
    return;
  }

  portENTER_CRITICAL(&feedbackMux);
  unsigned long firstEdge = firstEdgeMicros;
  unsigned long lastEdge = lastEdgeMicros;
  unsigned long edges = edgeCount;
  portEXIT_CRITICAL(&feedbackMux);
  unsigned long currentMicros = micros();

  bool isMeasuringOn = (relay.currentState == RelayConfig::State::ON);
  const char *stateName = isMeasuringOn ? "УВІМКНЕННЯ" : "ВИМКНЕННЯ";

  if ((currentMicros - testStartMicros) > RelayConfig::timeoutMicros) {
    armFeedback(false);
    testActive = false;
    Serial.printf("Помилка (%s): немає відповіді контакту, фронтів: %lu\n", stateName, edges);
    handleError();
    return;
  }

  if (edges == 0 || (currentMicros - lastEdge) < RelayConfig::settleMicros) {
    return;
  }

  armFeedback(false);
  testActive = false;

  if (relay.readContact() != relay.currentState) {
    Serial.printf("Помилка (%s): кінцевий стан контакту не відповідає очікуваному\n", stateName);
    handleError();
    return;
  }

  errorCount = 0;
  unsigned long mechanicalDelay = firstEdge - testStartMicros;
  unsigned long bounceTime = lastEdge - firstEdge;

  if (isMeasuringOn && onCount < RelayConfig::measurementsRequired) {
    onCount++;
    onDelaySum += mechanicalDelay;
    onBounceSum += bounceTime;
    Serial.printf("Вимірювання %u (УВІМКНЕННЯ): %lu мкс, брязкіт %lu мкс\n", onCount, mechanicalDelay, bounceTime);
  } else if (!isMeasuringOn && offCount < RelayConfig::measurementsRequired) {
    offCount++;
    offDelaySum += mechanicalDelay;
    offBounceSum += bounceTime;
    Serial.printf("Вимірювання %u (ВИМКНЕННЯ): %lu мкс, брязкіт %lu мкс\n", offCount, mechanicalDelay, bounceTime);
  }

  if (onCount >= RelayConfig::measurementsRequired && offCount >= RelayConfig::measurementsRequired) {
    Serial.println();
    printAverage("УВІМКНЕННЯ", onDelaySum, onBounceSum, onCount);
    printAverage("ВИМКНЕННЯ", offDelaySum, offBounceSum, offCount);
    relay.set(RelayConfig::State::OFF);
    testFinished = true;
  }
}