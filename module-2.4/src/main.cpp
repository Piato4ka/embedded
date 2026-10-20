#include <Arduino.h>

struct ButtonConfig {
  static constexpr int buttonPin = 4;
  static constexpr unsigned long settleMs = 30;
};

volatile unsigned long rawInterruptCount = 0;
volatile bool buttonEventFlag = false;

void IRAM_ATTR handleButtonInterrupt() {
  rawInterruptCount++;
  buttonEventFlag = true;
}

class LevelDebouncedButton {
public:
  int pin;
  bool eventPending;
  unsigned long lastEventMillis;
  unsigned long pressCount;
  unsigned long ignoredCount;

  LevelDebouncedButton(int p)
      : pin(p), eventPending(false), lastEventMillis(0), pressCount(0), ignoredCount(0) {}

  void init() {
    pinMode(pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(pin), handleButtonInterrupt, FALLING);
  }

  void update(unsigned long currentMillis) {
    if (buttonEventFlag) {
      buttonEventFlag = false;
      lastEventMillis = currentMillis;
      eventPending = true;
    }

    if (!eventPending || (currentMillis - lastEventMillis) < ButtonConfig::settleMs) {
      return;
    }
    eventPending = false;

    if (digitalRead(pin) == LOW) {
      pressCount++;
      Serial.printf("Натискання %lu (сирих переривань: %lu)\n", pressCount, rawInterruptCount);
    } else {
      ignoredCount++;
      Serial.printf("Подію відкинуто: кнопка відпущена (відкинуто всього: %lu)\n", ignoredCount);
    }
  }
};

LevelDebouncedButton button(ButtonConfig::buttonPin);

void setup() {
  Serial.begin(115200);
  delay(1000);
  button.init();
  Serial.println("\n--- Завдання 3: антидребезг за рівнем ---");
}

void loop() {
  button.update(millis());
}
