#include <Arduino.h>
#include "ServoEasing.hpp"


// definitions
#define ENABLE_EASE_ELASTIC

#define PIN_SERVO_ARM    12
#define PIN_SERVO_SPEED  A0
#define PIN_BUTTON_WAIVE 3

// declarations
ServoEasing Arm;

struct ServoPattern {
  int startStop[2];
  int next;  // pointer of next pattern
};

uint16_t tSpeed;

ServoPattern FullWaive = {20, 120, 0};
ServoPattern TinyWaive = {45, 55, 0};

// put function declarations here:
int moveServoToPosition(int position);
void waiveArm(ServoPattern *pattern);
void pulseColarLED();
void pulseLaserEye();
void pulseLaserArm();
void playRandomTrack();



void setup() {
  // put your setup code here, to run once:
  Arm.attach(PIN_SERVO_ARM, 45);
  Arm.setEasingType(EASE_ELASTIC_OUT);

  pinMode(PIN_BUTTON_WAIVE, INPUT_PULLUP);   // connect one end to GND and the other to PIN_BUTTON_WAIVE
  pinMode(PIN_SERVO_SPEED, INPUT);
}

void loop() {
  // put your main code here, to run repeatedly:

  // read speed poti and set
  tSpeed = analogRead(PIN_SERVO_SPEED);
  tSpeed = map(tSpeed, 0, 1023, 5, 150);
  setSpeedForAllServos(tSpeed);


  if (!Arm.isMoving()) {
    // accept inputs
    if (digitalRead(PIN_BUTTON_WAIVE) == HIGH) {
      waiveArm(&FullWaive);
    }
  }


}


// custom functions

/*
Move the arm to the next posision of the pattern.
*/
void waiveArm(ServoPattern* pattern) {
    Arm.startEaseTo(pattern->startStop[pattern->next], tSpeed, START_UPDATE_BY_INTERRUPT);
    // set next movement position.
    pattern->next = pattern->next + 1 % 2;
}
