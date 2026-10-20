#include <Arduino.h>
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"

struct FanConfig {
  static constexpr int fanPin = 16;
  static constexpr bool activeLow = false;

  static constexpr uint64_t periodMinutes = 60;
  static constexpr uint64_t workMinutes = 15;
  static constexpr uint64_t minuteMs = 60000;

  static constexpr uint16_t timerDivider = 80;
  static constexpr uint32_t timerFrequency = 1000000;
  static constexpr uint64_t ticksPerMs = 1000;

  static constexpr uint32_t onLevel = activeLow ? 0 : 1;
  static constexpr uint32_t offLevel = activeLow ? 1 : 0;
};

static_assert(FanConfig::workMinutes > 0 && FanConfig::workMinutes < FanConfig::periodMinutes,
              "workMinutes має бути більше 0 і менше periodMinutes");

volatile bool fanOnFlag = false;
volatile bool fanOffFlag = false;

hw_timer_t *periodTimer = nullptr;
hw_timer_t *workTimer = nullptr;

static inline void IRAM_ATTR setFanLevel(uint32_t level) {
  gpio_ll_set_level(&GPIO, (gpio_num_t)FanConfig::fanPin, level);
}

void IRAM_ATTR onPeriodTimer() {
  setFanLevel(FanConfig::onLevel);
  fanOnFlag = true;
}

void IRAM_ATTR onWorkTimer() {
  setFanLevel(FanConfig::offLevel);
  fanOffFlag = true;
}

uint64_t minutesToTicks(uint64_t minutes) {
  return minutes * FanConfig::minuteMs * FanConfig::ticksPerMs;
}

hw_timer_t *createTimer(uint8_t number, void (*handler)(), uint64_t alarmTicks) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)number;
  hw_timer_t *timer = timerBegin(FanConfig::timerFrequency);
  timerStop(timer);
  timerAttachInterrupt(timer, handler);
  timerAlarm(timer, alarmTicks, true, 0);
#else
  hw_timer_t *timer = timerBegin(number, FanConfig::timerDivider, true);
  timerStop(timer);
  timerAttachInterrupt(timer, handler, true);
  timerAlarmWrite(timer, alarmTicks, true);
  timerAlarmEnable(timer);
#endif
  timerWrite(timer, 0);
  return timer;
}

class ExhaustFan {
public:
  int pin;
  unsigned long cycleCount;

  ExhaustFan(int p) : pin(p), cycleCount(0) {}

  void init() {
    pinMode(pin, OUTPUT);
    setFanLevel(FanConfig::offLevel);
  }

  void start() {
    uint64_t periodTicks = minutesToTicks(FanConfig::periodMinutes);
    uint64_t workTicks = minutesToTicks(FanConfig::workMinutes);

    periodTimer = createTimer(0, onPeriodTimer, periodTicks);
    workTimer = createTimer(1, onWorkTimer, periodTicks);
    timerWrite(workTimer, periodTicks - workTicks);

    setFanLevel(FanConfig::onLevel);
    fanOnFlag = true;

    timerStart(periodTimer);
    timerStart(workTimer);
  }

  void printEvents(unsigned long currentMillis) {
    if (fanOnFlag) {
      fanOnFlag = false;
      cycleCount++;
      printTime(currentMillis);
      Serial.printf("Вентилятор УВІМКНЕНО (цикл %lu)\n", cycleCount);
    }

    if (fanOffFlag) {
      fanOffFlag = false;
      printTime(currentMillis);
      Serial.printf("Вентилятор ВИМКНЕНО (цикл %lu)\n", cycleCount);
    }
  }

private:
  static void printTime(unsigned long currentMillis) {
    unsigned long seconds = currentMillis / 1000;
    Serial.printf("[%02lu:%02lu:%02lu] ", seconds / 3600, (seconds / 60) % 60, seconds % 60);
  }
};

ExhaustFan fan(FanConfig::fanPin);

void setup() {
  Serial.begin(115200);
  delay(1000);
  fan.init();
  Serial.printf("\n--- Старт витяжного вентилятора: %llu хв роботи кожні %llu хв ---\n",
                FanConfig::workMinutes, FanConfig::periodMinutes);
  fan.start();
}

void loop() {
  fan.printEvents(millis());
}