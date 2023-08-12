#include <Arduino.h>
#include "ServoEasing.hpp"
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h>
#include <FadingLight.h>
#include <ezButton.h>

// definitions
#define ENABLE_EASE_ELASTIC
#define DEBUG // enable debug mode with low volume and serial output

#define DFPLAYER_VOLUME 25 // between 0 and 30 (regular mode of operation)
#define PIN_SERVO_ARM 12
#define PIN_SERVO_SPEED A0
#define PIN_BUTTON_WAIVE 3
#define PIN_BUTTON_TALK 2
#define PIN_BUTTON_LASER_TRIGGER 1
#define PIN_DFPLAYER_BUSY 6
#define PIN_DFPLAYER_RX 4
#define PIN_DFPLAYER_TX 5
#define PIN_LASER_ARM 10

// declarations
ServoEasing Arm;
SoftwareSerial softSerial(PIN_DFPLAYER_RX, PIN_DFPLAYER_TX);
DFRobotDFPlayerMini dfPlayer;
Fadinglight laserArm(PIN_LASER_ARM);
ezButton laserArmTrigger(PIN_BUTTON_LASER_TRIGGER);

struct ServoPattern
{
  int startStop[2];
  int next; // pointer of next pattern
};

uint16_t laserPulseDuration = 100;  // duration of laser on time during pulse
uint16_t tSpeed; // servo speed read from adc
uint16_t firstBeat, lastBeat; // ms timestamps for beat measurement;
uint16_t tBeat;  // music speed im ms for laser patterns

ServoPattern FullWaive = {20, 120, 0};
ServoPattern TinyWaive = {45, 55, 0};

// put function declarations here:
int moveServoToPosition(int position);
void waiveArm(ServoPattern *pattern);
void pulseColarLED();
void pulseLaserEye();
void pulseLaserArm();
void playRandomTrack(int folder);

void setup()
{
  // put your setup code here, to run once:

#ifdef SERIAL_PRINT
  Serial.begin(9600);
#endif

  // setup servo
  Arm.attach(PIN_SERVO_ARM, 45);
  Arm.setEasingType(EASE_ELASTIC_OUT);

  // setup MP3 Player
  pinMode(PIN_DFPLAYER_BUSY, INPUT);
  softSerial.begin(9600);
  if (!dfPlayer.begin(softSerial, /*isACK = */ true, /*doReset = */ true))
  {
#ifdef DEBUG
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
#endif
    while (true)
    {
      delay(0); // Code to compatible with ESP8266 watch dog.
    }
  }
#ifdef DEBUG
  Serial.println(F("DFPlayer Mini online."));
  dfPlayer.volume(10);
#else
  dfPlayer.volume(#DFPLAYER_VOLUME)
#endif
  playRandomTrack(10); // folder 10 contains greetings!

  // setup lasers 8-)
  laserArmTrigger.setDebounceTime(50);
  laserArmTrigger.setCountMode(COUNT_FALLING);
  laserArm.flash(10);

  // setup controlls
  pinMode(PIN_BUTTON_WAIVE, INPUT_PULLUP); // connect one end to GND and the other to PIN_BUTTON_WAIVE
  pinMode(PIN_SERVO_SPEED, INPUT);
  pinMode(PIN_BUTTON_TALK, INPUT);
  pinMode(PIN_BUTTON_LASER_TRIGGER, INPUT);
}

void loop()
{
  // read speed poti and set
  tSpeed = analogRead(PIN_SERVO_SPEED);
  tSpeed = map(tSpeed, 0, 1023, 5, 150);
  setSpeedForAllServos(tSpeed);

  // read buttons with ezButton
  laserArmTrigger.loop();

  // update lasers
  laserArm.update();

  if (!Arm.isMoving())
  {
    // accept inputs
    if (digitalRead(PIN_BUTTON_WAIVE) == HIGH)
    {
      waiveArm(&FullWaive);
    }
  }

  if ((digitalRead(PIN_BUTTON_TALK) == HIGH) & (!digitalRead(PIN_DFPLAYER_BUSY)))
  {
    // not talking yet... start talking
    playRandomTrack((int)random(0, 5));
  }

  if (laserArmTrigger.isPressed()){
    // laser button was pressed
    pulseLaserArm();
  } else {
    laserArm.off();
  }
}

// custom functions

/*
Move the arm to the next position of the pattern.
*/
void waiveArm(ServoPattern *pattern)
{
  Arm.startEaseTo(pattern->startStop[pattern->next], tSpeed, START_UPDATE_BY_INTERRUPT);
  // set next movement position.
  pattern->next = pattern->next + 1 % 2;
}

/*
Select folder and choose random track from it.
Cat has multiple folders with different "moods". The folder selects the mood.
*/
void playRandomTrack(int folder)
{
  int trackId = dfPlayer.readFileCountsInFolder(folder);
#ifdef DEBUG
  Serial.print("Play track ");
  Serial.print(trackId);
  Serial.print(" from folder ");
  Serial.println(folder);
#endif
  dfPlayer.playFolder(folder, (int)random(1, trackId));
}


/*
calculate speed setting by calling this function 4 times in a row.
Average time between button presses will measure blink interval.
After the 4th press is detected, start the blinking until button pressed
for a 5th time, which turns off the blinking. repeat.
*/
void pulseLaserArm() {
  unsigned long count = laserArmTrigger.getCount();
  if (count == 1) {
    firstBeat = millis();
  } else if (1 <= count <=  3) {
    // measure time
    lastBeat += millis() - firstBeat;
    firstBeat = millis();
  } else if (count == 4) {
    // average over measurements
    tBeat = lastBeat / 4;
    // set blink mode;
    laserArm.setSpeed(
      laserPulseDuration,
      tBeat - laserPulseDuration,
      tBeat,
      tBeat
    );
    laserArm.blink();
  } else if (count >= 5) {
    // reset counter;
    lastBeat = 0;
    firstBeat = 0;
    laserArmTrigger.resetCount();
    laserArm.off();
  }
  
}