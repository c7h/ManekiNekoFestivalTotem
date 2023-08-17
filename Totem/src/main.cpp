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

#define DFPLAYER_VOLUME 25 // between 0 and 30 (regular mode of operation)
#define PIN_SERVO_ARM 9
#define PIN_SERVO_SPEED A6
#define PIN_BUTTON_WAIVE A1
#define PIN_BUTTON_TALK 11
#define PIN_BUTTON_LASER_TRIGGER 12
#define PIN_DFPLAYER_BUSY 3
#define PIN_DFPLAYER_RX 6
#define PIN_DFPLAYER_TX 5
#define PIN_LASER_ARM 7
#define PIN_LASER_EYE 13
#define PIN_LASER_COLLAR_1 4
#define PIN_LASER_COLLAR_2 8
#define PIN_LASER_COLLAR_3 10
#define PIN_ADC_RANDOM 0   // leave this floating for randomness.

// declarations
ServoEasing Arm;
SoftwareSerial softSerial(PIN_DFPLAYER_RX, PIN_DFPLAYER_TX);
DFRobotDFPlayerMini dfPlayer;
JLed laserArm(PIN_LASER_ARM);
JLed laserEye(PIN_LASER_EYE);
JLed laserCollar1(PIN_LASER_COLLAR_1);
JLed laserCollar2(PIN_LASER_COLLAR_2);
JLed laserCollar3(PIN_LASER_COLLAR_3);

JLed collar[] = {
  laserCollar1.Blink(25, 25).Repeat(10),
  laserCollar2.Blink(25, 25).Repeat(10),
  laserCollar3.Blink(25, 25).Repeat(10)
};

auto CollarSequence = JLedSequence(JLedSequence::eMode::SEQUENCE, collar).Forever();

int laserCollarIndex = 0; // keep track of the current laser collar position.
bool prevLaserArmStatus = false;

ezButton laserArmTrigger(PIN_BUTTON_LASER_TRIGGER);
ezButton talkTrigger(PIN_BUTTON_TALK);

struct ServoPattern
{
  int startStop[2];
  bool next; // pointer of next pattern
};

uint16_t laserPulseDuration    = 100; // duration of laser on time during pulse
uint16_t laserEyePulseDuration = 50; // duration of laser on time during pulse
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
void moveLaserCollarPosition();
void playRandomTrack(int folder);


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
  dfPlayer.volume(DFPLAYER_VOLUME);
#else
  dfPlayer.volume(DFPLAYER_VOLUME)
#endif
  playRandomTrack(9); // folder 9 contains greetings!

  // setup lasers 8-)
  laserArmTrigger.setDebounceTime(50);
  laserArmTrigger.setCountMode(COUNT_FALLING);
  laserArm.Blink(100, 100).Repeat(6);
  laserEye.Blink(100, 200).Repeat(6);


  // setup controlls
  talkTrigger.setDebounceTime(100);
  pinMode(PIN_BUTTON_WAIVE, INPUT); // connect one end to GND and the other to PIN_BUTTON_WAIVE
  pinMode(PIN_SERVO_SPEED, INPUT);
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
  talkTrigger.loop();

  // update lasers
  laserArm.Update();
  laserEye.Update();
  // laserCollar1.Update();
  // laserCollar2.Update();
  // laserCollar3.Update();
  CollarSequence.Update();


  if (!Arm.isMoving())
  {
    // accept inputs
    if (digitalRead(PIN_BUTTON_WAIVE) == HIGH)
    {
      waiveArm(&TinyWaive);
    }
  }

  if ((talkTrigger.isPressed()) && (digitalRead(PIN_DFPLAYER_BUSY)))
  {
    #ifdef DEBUG
    Serial.println("The talk button is pressed");
    Serial.print("DFPlayer Busy: ");
    Serial.println(digitalRead(PIN_DFPLAYER_BUSY));
    #endif
    // not talking yet... start talking
    
    playRandomTrack(random(0, 5));
    //dfPlayer.next();
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
    delay(100);
    if (res != -1){
      filesInFolder = res;
    }
  }
  
#ifdef DEBUG
  Serial.print("Files in folder ");
  Serial.print(folder);
  Serial.print(": ");
  Serial.println(filesInFolder);

  if (dfPlayer.readType() == DFPlayerError && dfPlayer.read() == FileMismatch)
  {
    Serial.println("dfPlayer cannot find file!");
  }

#endif
  //int seed = analogRead(PIN_ADC_RANDOM);
  int seed = millis();
  randomSeed(seed);
  int choosenFile = random(0, filesInFolder-1);
#ifdef DEBUG
  Serial.print("Seed: ");
  Serial.print(seed);
  Serial.print(". Play Track ");
  Serial.println(choosenFile);
#endif
  dfPlayer.playFolder(folder, choosenFile);
  delay(1000);
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
    laserEye.Blink(laserEyePulseDuration, tBeat/2 - laserEyePulseDuration).Forever(); 
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
    Serial.print( ". Turn on ");
    Serial.println(laserCollarIndex);
  #endif  
}