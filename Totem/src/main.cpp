#include <Arduino.h>
#include "ServoEasing.hpp"
#include "jled.h"
#include <ezButton.h>
#include <string.h>

// definitions
#define ENABLE_EASE_ELASTIC
// #define DEBUG // debug lines share the UART with the control protocol

#define PIN_SERVO_ARM 9
#define PIN_BUTTON_FLASH 16
#define PIN_BUTTON_WAIVE A1
#define PIN_BUTTON_WAIVE_2 11
#define PIN_BUTTON_LASER_TRIGGER 12
#define PIN_LASER_ARM 7
#define PIN_LASER_EYE 13
#define PIN_LASER_COLLAR_1 4
#define PIN_LASER_COLLAR_2 8
#define PIN_LASER_COLLAR_3 10
#define PIN_LED_CHAIN 3  // PWM pin
#define PIN_ADC_RANDOM 0 // leave this floating for randomness.

#define SERIAL_BAUD 115200
#define LINE_MAX 64
#define HOST_TIMEOUT_MS 5000

// declarations
uint16_t laserPulseDuration = 100;   // duration of laser on time during pulse
uint16_t laserEyePulseDuration = 50; // duration of laser on time during pulse
uint16_t tSpeed = 60;                // servo speed in deg/s - our servo can handle 60
uint16_t firstBeat, lastBeat;        // ms timestamps for beat measurement;
uint16_t tBeat = 500;                // music speed im ms for laser patterns
bool blink_led_chain = false;        // should the led chain be turned on with laser eyes?

ServoEasing Arm;
JLed laserArm(PIN_LASER_ARM);
JLed laserEye(PIN_LASER_EYE);
JLed laserCollar1(PIN_LASER_COLLAR_1);
JLed laserCollar2(PIN_LASER_COLLAR_2);
JLed laserCollar3(PIN_LASER_COLLAR_3);
JLed ledChain(PIN_LED_CHAIN);

JLed collar[] = {
    laserCollar1.Blink(25, 25).Repeat(10),
    laserCollar2.Blink(25, 25).Repeat(10),
    laserCollar3.Blink(25, 25).Repeat(10)};

auto CollarSequence = JLedSequence(JLedSequence::eMode::SEQUENCE, collar).Forever();

// index: 0 eye, 1 hand, 2..4 collar groups
JLed *lasers[] = {&laserEye, &laserArm, &collar[0], &collar[1], &collar[2]};

ezButton laserArmTrigger(PIN_BUTTON_LASER_TRIGGER);
ezButton ledChainTrigger(PIN_BUTTON_FLASH);

struct ServoPattern
{
  int startStop[2];
  bool next; // pointer of next pattern
};

ServoPattern FullWaive = {{30, 110}, 0};
ServoPattern TinyWaive = {{80, 100}, 0};

// UART state
char line[LINE_MAX];
uint8_t lineLen = 0;
bool lineOverflow = false;
char *tokSave;
ServoPattern *pendingWave = nullptr;
bool collarAuto = true;
bool beatOn = false;
bool partyMode = false;
bool hostSeen = false;
uint32_t lastHostMs = 0;
uint16_t hostTimeoutMs = HOST_TIMEOUT_MS;

// put function declarations here:
void waiveArm(ServoPattern *pattern);
void pulseLaserArm();
void toggleLedChain();
void readSerial();
void handleLine(char *cmd);
void startBeat(uint16_t period);
void stopBeat();

void setup()
{
  Serial.begin(SERIAL_BAUD);

  // random number :)
  randomSeed(analogRead(PIN_ADC_RANDOM));

  // setup servo
  Arm.attach(PIN_SERVO_ARM, 45);
  Arm.setEasingType(EASE_CUBIC_IN_OUT);
  setSpeedForAllServos(tSpeed);

  // setup lasers 8-)
  laserArmTrigger.setDebounceTime(50);
  laserArmTrigger.setCountMode(COUNT_FALLING);
  laserArm.Blink(100, 100).Repeat(6);
  laserEye.Blink(100, 200).Repeat(6);
  ledChain.Blink(100, 100).Repeat(6);

  // setup Flash
  ledChainTrigger.setDebounceTime(50);

  // setup controlls
  pinMode(PIN_BUTTON_WAIVE_2, INPUT);
  pinMode(PIN_BUTTON_WAIVE, INPUT); // connect one end to GND and the other to PIN_BUTTON_WAIVE
  pinMode(PIN_BUTTON_LASER_TRIGGER, INPUT);
}

void loop()
{
  readSerial();

  // host went away: keep the party going instead of going dark
  if (hostSeen && hostTimeoutMs && !partyMode && millis() - lastHostMs > hostTimeoutMs)
  {
    partyMode = true;
    startBeat(tBeat);
    CollarSequence.Reset();
    collarAuto = true;
  }

  // read buttons with ezButton
  laserArmTrigger.loop();
  ledChainTrigger.loop();

  // update lasers
  laserArm.Update();
  laserEye.Update();
  ledChain.Update();
  CollarSequence.Update();
  if (!collarAuto)
  {
    for (auto &c : collar)
    {
      c.Update();
    }
  }

  if (!Arm.isMoving())
  {
    if (pendingWave)
    {
      waiveArm(pendingWave);
      pendingWave = nullptr;
    }
    // accept inputs
    else if (digitalRead(PIN_BUTTON_WAIVE) == HIGH)
    {
      waiveArm(&TinyWaive);
    }
    else if (digitalRead(PIN_BUTTON_WAIVE_2) == LOW)
    {
      waiveArm(&FullWaive);
    }
  }

  if (laserArmTrigger.isPressed())
  {
    // laser button was pressed
    pulseLaserArm();
  }

  if (ledChainTrigger.isPressed())
  {
    toggleLedChain();
  }
}

// custom functions

/*
Move the arm to the next position of the pattern.
*/
void waiveArm(ServoPattern *pattern)
{
  int nextPosition = pattern->startStop[pattern->next];

#ifdef DEBUG
  Serial.print("# moving arm to position ");
  Serial.print(nextPosition);
  Serial.print(" with speed ");
  Serial.println(tSpeed);
#endif

  Arm.startEaseTo(nextPosition, tSpeed, START_UPDATE_BY_INTERRUPT);
  // set next movement position.
  pattern->next = !pattern->next;
}

/*
Blink arm and eye laser (and the led chain, if enabled) with the given beat.
*/
void startBeat(uint16_t period)
{
  tBeat = period;
  laserArm.Blink(laserPulseDuration, period > laserPulseDuration ? period - laserPulseDuration : 1).Forever();
  uint16_t eyeOff = period / 2 > laserEyePulseDuration ? period / 2 - laserEyePulseDuration : 1;
  laserEye.Blink(laserEyePulseDuration, eyeOff).Forever();

  if (blink_led_chain == true)
  {
    ledChain.Breathe(period).Forever();
  }
  beatOn = true;
}

void stopBeat()
{
  lastBeat = 0;
  firstBeat = 0;
  laserArmTrigger.resetCount();
  laserArm.FadeOff(300);
  laserArm.Stop();
  laserEye.Stop();
  ledChain.Stop();
  beatOn = false;
}

/*
calculate speed setting by calling this function 4 times in a row.
Average time between button presses will measure blink interval.
After the 4th press is detected, start the blinking until button pressed
for a 5th time, which turns off the blinking. repeat.
*/
void pulseLaserArm()
{

#ifdef DEBUG
  Serial.print("# Laser Arm was triggered. Count: ");
  Serial.println(laserArmTrigger.getCount());
#endif
  unsigned long count = laserArmTrigger.getCount();
  if (count == 1)
  {
    firstBeat = millis();
  }
  else if ((1 < count) && (4 > count))
  {
    // measure time
    lastBeat += millis() - firstBeat;
    firstBeat = millis();
  }
  else if (count == 4)
  {
    // average over measurements
    startBeat(lastBeat / 2);
  }
  else if (count >= 5)
  {
    stopBeat();
  }
}

/*
Toggle whether the led chain breathes with the beat
*/
void toggleLedChain()
{
  blink_led_chain = !blink_led_chain;
}

// UART control. Protocol: docs/uart-protocol.md

#define NEXT() strtok_r(nullptr, " \t", &tokSave)
bool IS(const char *a, const char *b) { return a && !strcasecmp(a, b); }

// parse token t as a number within [lo, hi]; false on missing/invalid
bool num(char *t, long lo, long hi, long &out)
{
  char *end;
  if (!t)
    return false;
  out = strtol(t, &end, 10);
  return *end == '\0' && out >= lo && out <= hi;
}

void handleLine(char *cmd)
{
  cmd = strtok_r(cmd, " \t", &tokSave);
  if (!cmd)
    return;

  lastHostMs = millis();
  hostSeen = true;

  if (IS(cmd, "PING"))
  {
    Serial.println("PONG");
    return;
  }
  if (IS(cmd, "ID"))
  {
    Serial.println("OK TOTEM 1");
    return;
  }
  if (IS(cmd, "STATUS"))
  {
    Serial.print("OK arm=");
    Serial.print(Arm.getCurrentAngle());
    Serial.print(" move=");
    Serial.print(Arm.isMoving());
    Serial.print(" collar=");
    Serial.print(collarAuto ? "auto" : "manual");
    Serial.print(" beat=");
    Serial.print(beatOn ? tBeat : 0);
    Serial.print(" party=");
    Serial.println(partyMode);
    return;
  }
  if (IS(cmd, "TIMEOUT"))
  {
    long ms;
    if (!num(NEXT(), 0, 60000, ms))
      goto range;
    hostTimeoutMs = ms;
    goto ok;
  }

  // everything below takes control from party mode
  if (partyMode)
  {
    partyMode = false;
    stopBeat();
  }

  if (IS(cmd, "OFF"))
  {
    stopBeat();
    for (auto l : lasers)
      l->Off();
    CollarSequence.Stop();
    collarAuto = false;
    ledChain.Off();
    blink_led_chain = false;
    pendingWave = nullptr;
    Arm.stop();
    hostSeen = false; // OFF stays off, even if the host goes away
    goto ok;
  }

  if (IS(cmd, "SERVO") || IS(cmd, "WAVE"))
  {
    bool servo = IS(cmd, "SERVO");
    char *arg = NEXT();
    long angle;
    pendingWave = nullptr;
    if (IS(arg, "STOP"))
      Arm.stop();
    else if (servo)
    {
      if (!num(arg, 0, 180, angle))
        goto range;
      Arm.startEaseTo((int)angle, tSpeed, START_UPDATE_BY_INTERRUPT);
    }
    else if (IS(arg, "TINY"))
      pendingWave = &TinyWaive;
    else if (IS(arg, "FULL"))
      pendingWave = &FullWaive;
    else
      goto args;
    goto ok;
  }

  if (IS(cmd, "BEAT"))
  {
    long period;
    char *arg = NEXT();
    if (IS(arg, "OFF"))
      stopBeat();
    else if (num(arg, 20, 60000, period))
      startBeat(period);
    else
      goto range;
    goto ok;
  }

  if (IS(cmd, "COLLAR"))
  {
    if (!IS(NEXT(), "AUTO"))
      goto args;
    CollarSequence.Reset();
    collarAuto = true;
    goto ok;
  }

  if (IS(cmd, "LED"))
  {
    char *action = NEXT();
    long a, b;
    if (IS(action, "ON"))
      ledChain.On();
    else if (IS(action, "OFF"))
      ledChain.Off();
    else if (IS(action, "LEVEL") && num(NEXT(), 0, 255, a))
      ledChain.Set(a);
    else if (IS(action, "BLINK") && num(NEXT(), 1, 60000, a) && num(NEXT(), 1, 60000, b))
      ledChain.Blink(a, b).Forever();
    else if (IS(action, "BREATHE") && num(NEXT(), 20, 60000, a))
      ledChain.Breathe(a).Forever();
    else
      goto range;
    goto ok;
  }

  if (IS(cmd, "LASER"))
  {
    char *target = NEXT();
    char *action = NEXT();
    long ms = 0;
    uint8_t first, last;
    if (IS(target, "EYE"))
      first = last = 0;
    else if (IS(target, "HAND"))
      first = last = 1;
    else if (IS(target, "COLLAR1"))
      first = last = 2;
    else if (IS(target, "COLLAR2"))
      first = last = 3;
    else if (IS(target, "COLLAR3"))
      first = last = 4;
    else if (IS(target, "COLLAR"))
      first = 2, last = 4;
    else if (IS(target, "ALL"))
      first = 0, last = 4;
    else
      goto args;
    if (!IS(action, "ON") && !IS(action, "OFF") && !IS(action, "PULSE"))
      goto args;
    char *msArg = NEXT();
    if (msArg && !num(msArg, 1, 60000, ms))
      goto range;

    if (first >= 2)
    {
      CollarSequence.Stop();
      collarAuto = false;
    }
    for (uint8_t i = first; i <= last; i++)
    {
      if (IS(action, "ON"))
        lasers[i]->On();
      else if (IS(action, "OFF"))
        lasers[i]->Off();
      else // pulse: Blink(on, 1) once, so the laser ends up off
        lasers[i]->Blink(ms ? ms : (i == 0 ? laserEyePulseDuration : laserPulseDuration), 1).Repeat(1);
    }
    goto ok;
  }

  Serial.println("ERR unknown");
  return;
args:
  Serial.println("ERR args");
  return;
range:
  Serial.println("ERR range");
  return;
ok:
  Serial.println("OK");
}

void readSerial()
{
  while (Serial.available() > 0)
  {
    char c = Serial.read();
    if (c == '\n')
    {
      line[lineLen] = '\0';
      if (!lineOverflow)
        handleLine(line);
      lineLen = 0;
      lineOverflow = false;
    }
    else if (c != '\r' && !lineOverflow)
    {
      if (lineLen < LINE_MAX - 1)
        line[lineLen++] = c;
      else
      {
        lineOverflow = true;
        Serial.println("ERR line");
      }
    }
  }
}
