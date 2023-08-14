#include <Arduino.h>
#include "ServoEasing.hpp"
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h>
#include "Blinkenlight.h"
#include "jled.h"
#include <ezButton.h>

// definitions
#define ENABLE_EASE_ELASTIC
#define DEBUG // enable debug mode with low volume and serial output

#define DFPLAYER_VOLUME 30 // between 0 and 30 (regular mode of operation)
#define PIN_SERVO_ARM 9
#define PIN_SERVO_SPEED A6
#define PIN_BUTTON_WAIVE A1
#define PIN_BUTTON_TALK 10
#define PIN_BUTTON_LASER_TRIGGER 12
#define PIN_DFPLAYER_BUSY 3
#define PIN_DFPLAYER_RX 6
#define PIN_DFPLAYER_TX 5
#define PIN_LASER_ARM 7
#define PIN_LASER_EYE 13

// declarations
ServoEasing Arm;
SoftwareSerial softSerial(PIN_DFPLAYER_RX, PIN_DFPLAYER_TX);
DFRobotDFPlayerMini dfPlayer;
//Blinkenlight laserArm(PIN_LASER_ARM);
JLed laserArm(PIN_LASER_ARM);
ezButton laserArmTrigger(PIN_BUTTON_LASER_TRIGGER);

struct ServoPattern
{
  int startStop[2];
  bool next; // pointer of next pattern
};

uint16_t laserPulseDuration = 100; // duration of laser on time during pulse
uint16_t tSpeed;                   // servo speed read from adc
uint16_t firstBeat, lastBeat;      // ms timestamps for beat measurement;
uint16_t tBeat;                    // music speed im ms for laser patterns

ServoPattern FullWaive = {{20, 120}, 0};
ServoPattern TinyWaive = {{80, 100}, 0};

// put function declarations here:
int moveServoToPosition(int position);
void waiveArm(ServoPattern *pattern);
void pulseColarLED();
void pulseLaserArm();
void playRandomTrack(int folder);

void setup()
{
  // put your setup code here, to run once:

#ifdef DEBUG
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
  dfPlayer.volume(20);
#else
  dfPlayer.volume(#DFPLAYER_VOLUME)
#endif
  playRandomTrack(9); // folder 9 contains greetings!

  // setup lasers 8-)
  laserArmTrigger.setDebounceTime(50);
  laserArmTrigger.setCountMode(COUNT_FALLING);
  //laserArm.blink();
  laserArm.Breathe(500);

  // setup controlls
  pinMode(PIN_BUTTON_WAIVE, INPUT); // connect one end to GND and the other to PIN_BUTTON_WAIVE
  pinMode(PIN_SERVO_SPEED, INPUT);
  pinMode(PIN_BUTTON_TALK, INPUT);
  pinMode(PIN_BUTTON_LASER_TRIGGER, INPUT);
  pinMode(PIN_LASER_EYE, OUTPUT);
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
  laserArm.Update();

  if (!Arm.isMoving())
  {
    // accept inputs
    if (digitalRead(PIN_BUTTON_WAIVE) == HIGH)
    {
      waiveArm(&TinyWaive);
    }
  }

  if ((digitalRead(PIN_BUTTON_TALK) == HIGH) & (!digitalRead(PIN_DFPLAYER_BUSY)))
  {
    // not talking yet... start talking
    playRandomTrack(random(0, 5));
  }

  if (laserArmTrigger.isPressed())
  {
    // laser button was pressed
    pulseLaserArm();
  }
}

// custom functions

/*
Move the arm to the next position of the pattern.
*/
void waiveArm(ServoPattern* pattern)
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
Select folder and choose random track from it.
Cat has multiple folders with different "moods". The folder selects the mood.
*/
void playRandomTrack(int folder)
{
  int filesInFolder = 5;
  // hack to get files count in folder right.
  for (int i = 0; i < 3; i++)
  {
    int res = dfPlayer.readFileCountsInFolder(folder);
    if (res != -1){
      filesInFolder = res;
    }
  }
  
#ifdef DEBUG
  Serial.print("Files in folder ");
  Serial.print(folder);
  Serial.print(": ");
  Serial.println(filesInFolder);

  Serial.print("Total folder count: ");
  Serial.println(dfPlayer.readFolderCounts());

  if (dfPlayer.readType() == DFPlayerError && dfPlayer.read() == FileMismatch)
  {
    Serial.println("dfPlayer cannot find file!");
  }

#endif
  randomSeed(analogRead(0));
  int choosenFile = random(0, filesInFolder-1);
#ifdef DEBUG
  Serial.print("Play Track ");
  Serial.println(choosenFile);
#endif
  dfPlayer.playFolder(folder, choosenFile);
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
    tBeat = lastBeat / 4;
    laserArm.Blink(laserPulseDuration, tBeat - laserPulseDuration).Forever();

    #ifdef DEBUG
      Serial.print("Stat blinking arm with speed ");
      Serial.print(tBeat);
      Serial.println("ms");
    #endif
    digitalWrite(PIN_LASER_EYE, HIGH);
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
    digitalWrite(PIN_LASER_EYE, LOW);
  }
}