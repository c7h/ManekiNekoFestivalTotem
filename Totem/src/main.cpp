#include <Arduino.h>
#include "ServoEasing.hpp"
#include <SoftwareSerial.h>
#include "Blinkenlight.h"
#include "jled.h"
#include <ezButton.h>

// definitions
#define ENABLE_EASE_ELASTIC
#define DEBUG // enable debug mode with low volume and serial output

#define PIN_SERVO_ARM 9
#define PIN_BUTTON_FLASH 16 
#define PIN_BUTTON_WAIVE A1
#define PIN_BUTTON_WAIVE_2 11
#define PIN_BUTTON_LASER_TRIGGER 12
#define PIN_DFPLAYER_BUSY 3
#define PIN_DFPLAYER_RX 6
#define PIN_DFPLAYER_TX 5
#define PIN_LASER_ARM 7
#define PIN_LASER_EYE 13
#define PIN_LASER_COLLAR_1 4
#define PIN_LASER_COLLAR_2 8
#define PIN_LASER_COLLAR_3 10
#define PIN_LED_CHAIN 2
#define PIN_ADC_RANDOM 0 // leave this floating for randomness.

// declarations
uint16_t ledChainPluseDuration = 200;// duration of led chain pulse
uint16_t laserPulseDuration = 100;   // duration of laser on time during pulse
uint16_t laserEyePulseDuration = 50; // duration of laser on time during pulse
uint16_t tSpeed = 60;                // servo speed in deg/s - our servo can handle 60
uint16_t firstBeat, lastBeat;        // ms timestamps for beat measurement;
uint16_t tBeat;                      // music speed im ms for laser patterns
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
    laserCollar3.Blink(25, 25).Repeat(10)
};



auto CollarSequence = JLedSequence(JLedSequence::eMode::SEQUENCE, collar).Forever();

int laserCollarIndex = 0; // keep track of the current laser collar position.
bool prevLaserArmStatus = false;

ezButton laserArmTrigger(PIN_BUTTON_LASER_TRIGGER);
ezButton ledChainTrigger(PIN_BUTTON_FLASH);

struct ServoPattern
{
  int startStop[2];
  bool next; // pointer of next pattern
};



ServoPattern FullWaive = {{30, 110}, 0};
ServoPattern TinyWaive = {{80, 100}, 0};

// put function declarations here:
int moveServoToPosition(int position);
void waiveArm(ServoPattern *pattern);
void pulseLaserArm();
void moveLaserCollarPosition();
void toggleLedChain();

void setup()
{
  // put your setup code here, to run once:

#ifdef DEBUG
  Serial.begin(9600);
#endif

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

  // read buttons with ezButton
  laserArmTrigger.loop();
  ledChainTrigger.loop();

  // update lasers
  laserArm.Update();
  laserEye.Update();
  ledChain.Update();
  CollarSequence.Update();

  if (!Arm.isMoving())
  {
    // accept inputs
    if (digitalRead(PIN_BUTTON_WAIVE) == HIGH)
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
  Serial.print("moving arm to position ");
  Serial.print(nextPosition);
  Serial.print(" with speed ");
  Serial.println(tSpeed);
#endif

  Arm.startEaseTo(nextPosition, tSpeed, START_UPDATE_BY_INTERRUPT);
  // set next movement position.
  pattern->next = !pattern->next;
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
  Serial.print("Laser Arm was triggered. Count: ");
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
    tBeat = lastBeat / 2;
    laserArm.Blink(laserPulseDuration, tBeat - laserPulseDuration).Forever();

#ifdef DEBUG
    Serial.print("Stat blinking arm with speed ");
    Serial.print(tBeat);
    Serial.println("ms");
#endif
    laserEye.Blink(laserEyePulseDuration, tBeat / 2 - laserEyePulseDuration).Forever();

    if (blink_led_chain == true) {
      ledChain.Breathe(tBeat).Forever();
    }

  }
  else if (count >= 5)
  {
// reset counter;
#ifdef DEBUG
    Serial.print("laser arm turn off...");
#endif
    lastBeat = 0;
    firstBeat = 0;
    laserArmTrigger.resetCount();
    laserArm.FadeOff(300);
    laserArm.Stop();
    laserEye.Stop();
    ledChain.Stop();
  }
}

/*
Move the laser to the next position;
turn laser on and pervious off;
*/
void moveLaserCollarPosition()
{

#ifdef DEBUG
  Serial.print("collar turn off ");
  Serial.print(laserCollarIndex);
#endif

  collar[laserCollarIndex].On();
  // keep track of current position
  laserCollarIndex = (laserCollarIndex + 1) % 3;
  collar[laserCollarIndex].Off();

#ifdef DEBUG
  Serial.print(". Turn on ");
  Serial.println(laserCollarIndex);
#endif
}


/*
Flash the laser Collar briefly and return to normal operation
*/
void toggleLedChain()
{
  Serial.print("Toggle LED chain setting: ");
  blink_led_chain = ! blink_led_chain;
  Serial.println(blink_led_chain);
}