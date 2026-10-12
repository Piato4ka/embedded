#include <Arduino.h>

static constexpr unsigned int amountOfLoopsToMeasure = 1000;
volatile bool btnIsOn = false;

unsigned long lastMicros = 0;
unsigned long microsSum = 0;
unsigned int timerIteration = 0;

struct LedConfig
{
  enum LedState
  {
    OFF = LOW,
    ON = HIGH
  };

  enum LedMode
  {
    FLASHING,
    LED_ON,
    LED_OFF
  };

  static constexpr int LedPin = 6;
  static constexpr unsigned long intervalLed = 500; // In milliseconds
};

volatile LedConfig::LedState currentLedState = LedConfig::LedState::ON;
volatile LedConfig::LedMode currentLedMode = LedConfig::LedMode::FLASHING;

struct ButtonConfig
{
  enum BtnState
  {
    OFF = LOW,
    ON = HIGH
  };
  static constexpr int BtnPin = 5;
  static constexpr unsigned long debounceTime = 200; // 200ms debounce window
};

class Led
{
public:
  int pin;
  unsigned long lastBlinkMillis = 0; // Dedicated timer for LED blinking

  Led(int pin) : pin(pin) {}

  void init()
  {
    pinMode(pin, OUTPUT);
  }

  void set(LedConfig::LedState state)
  {
    digitalWrite(pin, state);
    currentLedState = state;
  }

  void ledFlashing()
  {
    unsigned long currentMillis = millis();
    if (currentMillis - lastBlinkMillis >= LedConfig::intervalLed)
    {
      lastBlinkMillis = currentMillis;
      if (currentLedState == LedConfig::LedState::ON)
      {
        set(LedConfig::LedState::OFF);
      }
      else
      {
        set(LedConfig::LedState::ON);
      }
    }
  }

  void ledAlwaysOn()
  {
    set(LedConfig::LedState::ON);
  }

  void ledAlwaysOff()
  {
    set(LedConfig::LedState::OFF);
  }
};

class Btn
{
public:
  int pin;

  Btn(int pin) : pin(pin) {}

  void init(void (*changeState)())
  {
    pinMode(pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(pin), changeState, FALLING);
  }
};

Led redLed = Led(LedConfig::LedPin);
Btn btn = Btn(ButtonConfig::BtnPin);


void changeState()
{
  static unsigned long lastInterruptTime = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - lastInterruptTime > ButtonConfig::debounceTime)
  {
    btnIsOn = !btnIsOn;
    currentLedMode = static_cast<LedConfig::LedMode>((static_cast<int>(currentLedMode) + 1) % 3);
    lastInterruptTime = currentMillis;
  }
}

void measureTimeAverageForLoop()
{
  unsigned long currentMicros = micros();
  unsigned long timeDiff = currentMicros - lastMicros;
  lastMicros = currentMicros;

  if (timerIteration < amountOfLoopsToMeasure)
  {
    microsSum += timeDiff;
    timerIteration++;
  }
  else
  {
    Serial.printf("the average micros is: %lu \n", microsSum / amountOfLoopsToMeasure);
    timerIteration = 0;
    microsSum = 0;
  }
}

void setup()
{
  Serial.begin(115200);
  redLed.init();
  btn.init(changeState);
  lastMicros = micros();
}

void loop()
{
  measureTimeAverageForLoop();

  switch (currentLedMode)
  {
  case LedConfig::LedMode::FLASHING:
    redLed.ledFlashing();
    break;
  case LedConfig::LedMode::LED_OFF:
    redLed.ledAlwaysOff();
    break;
  case LedConfig::LedMode::LED_ON:
    redLed.ledAlwaysOn();
    break;
  default:
    break;
  }
}