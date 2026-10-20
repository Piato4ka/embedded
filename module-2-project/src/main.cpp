#include <Arduino.h>

struct TrafficConfig
{
  static constexpr int pinGreen = 5;
  static constexpr int pinYellow = 6;
  static constexpr int pinRed = 7;
  static constexpr int pinButton = 4;

  static constexpr unsigned long greenMs = 10000;
  static constexpr unsigned long greenBlinkMs = 3000;
  static constexpr unsigned long yellowMs = 3000;
  static constexpr unsigned long redMs = 10000;
  static constexpr unsigned long redYellowMs = 2000;
  static constexpr unsigned long blinkHalfPeriodMs = 500;
  static constexpr unsigned long debounceMs = 50;

  static constexpr uint16_t timerDivider = 80;
  static constexpr uint32_t timerFrequency = 1000000;
  static constexpr uint64_t ticksPerMs = 1000;
};

enum TrafficState
{
  STATE_GREEN = 0,
  STATE_GREEN_BLINK,
  STATE_YELLOW,
  STATE_RED,
  STATE_RED_YELLOW,
  STATE_YELLOW_BLINK
};

enum TrafficEvent
{
  EVT_STATE_TIMEOUT = 1,
  EVT_BLINK_TICK,
  EVT_BUTTON
};

volatile bool stateTimeoutFlag = false;
volatile bool blinkTickFlag = false;
volatile bool buttonEdgeFlag = false;

hw_timer_t *stateTimer = nullptr;
hw_timer_t *blinkTimer = nullptr;

void IRAM_ATTR onStateTimer()
{
  stateTimeoutFlag = true;
}

void IRAM_ATTR onBlinkTimer()
{
  blinkTickFlag = true;
}

void IRAM_ATTR onButtonEdge()
{
  buttonEdgeFlag = true;
}

uint64_t msToTicks(unsigned long ms)
{
  return (uint64_t)ms * TrafficConfig::ticksPerMs;
}

void initTimers()
{
  stateTimer = timerBegin(0, TrafficConfig::timerDivider, true);
  timerAttachInterrupt(stateTimer, onStateTimer, true);
  timerStop(stateTimer);

  blinkTimer = timerBegin(1, TrafficConfig::timerDivider, true);
  timerAttachInterrupt(blinkTimer, onBlinkTimer, true);
  timerAlarmWrite(blinkTimer, msToTicks(TrafficConfig::blinkHalfPeriodMs), true);
  timerAlarmEnable(blinkTimer);
}

void armStateTimer(unsigned long ms)
{
  timerStop(stateTimer);
  timerWrite(stateTimer, 0);
  stateTimeoutFlag = false;
  timerAlarmWrite(stateTimer, msToTicks(ms), false);
  timerAlarmEnable(stateTimer);
  timerStart(stateTimer);
}

void stopStateTimer()
{
  timerStop(stateTimer);
  stateTimeoutFlag = false;
}

void restartBlinkTimer()
{
  timerWrite(blinkTimer, 0);
  blinkTickFlag = false;
}

class TrafficLight
{
public:
  TrafficState currentState;
  bool blinkOn;
  int buttonStableLevel;
  bool buttonCheckPending;
  unsigned long buttonEdgeMillis;

  TrafficLight()
      : currentState(STATE_RED), blinkOn(false), buttonStableLevel(HIGH),
        buttonCheckPending(false), buttonEdgeMillis(0) {}

  void init()
  {
    pinMode(TrafficConfig::pinGreen, OUTPUT);
    pinMode(TrafficConfig::pinYellow, OUTPUT);
    pinMode(TrafficConfig::pinRed, OUTPUT);
    pinMode(TrafficConfig::pinButton, INPUT_PULLUP);
    buttonStableLevel = digitalRead(TrafficConfig::pinButton);

    attachInterrupt(digitalPinToInterrupt(TrafficConfig::pinButton), onButtonEdge, CHANGE);

    enterState(STATE_RED);
  }

  void handleEvent(TrafficEvent event)
  {
    switch (event)
    {
    case EVT_STATE_TIMEOUT:
      if (currentState != STATE_YELLOW_BLINK)
      {
        enterState(nextState(currentState));
      }
      break;

    case EVT_BLINK_TICK:
      if (currentState == STATE_GREEN_BLINK || currentState == STATE_YELLOW_BLINK)
      {
        blinkOn = !blinkOn;
        updateLamps();
      }
      break;

    case EVT_BUTTON:
      enterState(currentState == STATE_YELLOW_BLINK ? STATE_YELLOW : STATE_YELLOW_BLINK);
      break;
    }
  }

  void pollButton(unsigned long currentMillis)
  {
    if (buttonEdgeFlag)
    {
      buttonEdgeFlag = false;
      buttonEdgeMillis = currentMillis;
      buttonCheckPending = true;
    }

    if (buttonCheckPending && (currentMillis - buttonEdgeMillis) >= TrafficConfig::debounceMs)
    {
      buttonCheckPending = false;
      int level = digitalRead(TrafficConfig::pinButton);

      if (level != buttonStableLevel)
      {
        buttonStableLevel = level;
        if (level == LOW)
        {
          handleEvent(EVT_BUTTON);
        }
      }
    }
  }

private:
  void enterState(TrafficState state)
  {
    currentState = state;
    blinkOn = true;
    restartBlinkTimer();

    unsigned long duration = stateDuration(state);
    if (duration > 0)
    {
      armStateTimer(duration);
    }
    else
    {
      stopStateTimer();
    }

    updateLamps();
    Serial.printf("Стан: %s\n", stateName(state));
  }

  void updateLamps()
  {
    bool green = (currentState == STATE_GREEN) ||
                 (currentState == STATE_GREEN_BLINK && blinkOn);
    bool yellow = (currentState == STATE_YELLOW) ||
                  (currentState == STATE_RED_YELLOW) ||
                  (currentState == STATE_YELLOW_BLINK && blinkOn);
    bool red = (currentState == STATE_RED) ||
               (currentState == STATE_RED_YELLOW);

    digitalWrite(TrafficConfig::pinGreen, green ? HIGH : LOW);
    digitalWrite(TrafficConfig::pinYellow, yellow ? HIGH : LOW);
    digitalWrite(TrafficConfig::pinRed, red ? HIGH : LOW);
  }

  static TrafficState nextState(TrafficState state)
  {
    switch (state)
    {
    case STATE_GREEN:
      return STATE_GREEN_BLINK;
    case STATE_GREEN_BLINK:
      return STATE_YELLOW;
    case STATE_YELLOW:
      return STATE_RED;
    case STATE_RED:
      return STATE_RED_YELLOW;
    case STATE_RED_YELLOW:
      return STATE_GREEN;
    default:
      return STATE_RED;
    }
  }

  static unsigned long stateDuration(TrafficState state)
  {
    switch (state)
    {
    case STATE_GREEN:
      return TrafficConfig::greenMs;
    case STATE_GREEN_BLINK:
      return TrafficConfig::greenBlinkMs;
    case STATE_YELLOW:
      return TrafficConfig::yellowMs;
    case STATE_RED:
      return TrafficConfig::redMs;
    case STATE_RED_YELLOW:
      return TrafficConfig::redYellowMs;
    default:
      return 0;
    }
  }

  static const char *stateName(TrafficState state)
  {
    switch (state)
    {
    case STATE_GREEN:
      return "ЗЕЛЕНИЙ";
    case STATE_GREEN_BLINK:
      return "ЗЕЛЕНИЙ МИГОТЛИВИЙ";
    case STATE_YELLOW:
      return "ЖОВТИЙ";
    case STATE_RED:
      return "ЧЕРВОНИЙ";
    case STATE_RED_YELLOW:
      return "ЧЕРВОНИЙ + ЖОВТИЙ";
    case STATE_YELLOW_BLINK:
      return "ЖОВТИЙ МИГОТЛИВИЙ";
    default:
      return "?";
    }
  }
};

TrafficLight trafficLight;

void setup()
{
  Serial.begin(115200);
  delay(1000);
  initTimers();
  trafficLight.init();
  Serial.println("Старт світлофора \n");
}

void loop()
{
  if (stateTimeoutFlag)
  {
    stateTimeoutFlag = false;
    trafficLight.handleEvent(EVT_STATE_TIMEOUT);
  }

  if (blinkTickFlag)
  {
    blinkTickFlag = false;
    trafficLight.handleEvent(EVT_BLINK_TICK);
  }

  trafficLight.pollButton(millis());
}
