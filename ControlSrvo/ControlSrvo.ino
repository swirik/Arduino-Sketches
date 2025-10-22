#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

#define SERVO_FREQ 50      
#define SERVO_MIN     600     
#define SERVO_MAX     2400


void setup() {
  Serial.begin(115200);
  Serial.println("PCA9685 8-channel Servo test");

  Wire.begin(21, 22);            

  pwm.begin();
  pwm.setOscillatorFrequency(25000000); 
  pwm.setPWMFreq(SERVO_FREQ);

  delay(10);
}

void moveServo(int servo, int angle) {
  angle = constrain(angle, 0, 180);
  int convert = map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
  pwm.writeMicroseconds(servo, convert);
}

void loop() {
  
  for (int i = 0; i < 6; i++) {
    moveServo(i, 0);
  }
  delay(1000);
    for (int i = 0; i < 6; i++) {
    moveServo(i, 180);
  }
  delay(1000);

}
