#include <Arduino.h>
#include "ServoEasing.hpp"
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h>

// definitions
#define ENABLE_EASE_ELASTIC
#define DEBUG // enable debug mode with low volume and serial output

#define DFPLAYER_VOLUME 25 // between 0 and 30 (regular mode of operation)
#define PIN_SERVO_ARM 12
#define PIN_SERVO_SPEED A0
#define PIN_BUTTON_WAIVE 3
#define PIN_DFPLAYER_RX 4
#define PIN_DFPLAYER_TX 5

// declarations
ServoEasing Arm;
SoftwareSerial softSerial(PIN_DFPLAYER_RX, PIN_DFPLAYER_TX);
DFRobotDFPlayerMini dfPlayer;

struct ServoPattern
{
  int startStop[2];
  int next; // pointer of next pattern
};

uint16_t tSpeed; // servo speed read from adc

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
  playRandomTrack(10);

  // setup controlls
  pinMode(PIN_BUTTON_WAIVE, INPUT_PULLUP); // connect one end to GND and the other to PIN_BUTTON_WAIVE
  pinMode(PIN_SERVO_SPEED, INPUT);
}

void loop()
{
  // read speed poti and set
  tSpeed = analogRead(PIN_SERVO_SPEED);
  tSpeed = map(tSpeed, 0, 1023, 5, 150);
  setSpeedForAllServos(tSpeed);

  if (!Arm.isMoving())
  {
    // accept inputs
    if (digitalRead(PIN_BUTTON_WAIVE) == HIGH)
    {
      waiveArm(&FullWaive);
    }
  }
}

// custom functions

/*
Move the arm to the next posision of the pattern.
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
  dfPlayer.playFolder(folder, random(1, dfPlayer.readFileCountsInFolder(folder)));
}
