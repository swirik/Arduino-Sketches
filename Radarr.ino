// Includes the Servo library
#include <Servo.h>
// Defines Tirg and Echo pins of the Ultrasonic Sensor
const int trigPin = 10;
const int echoPin = 11;
// Define pins for buzzer and LED
const int buzzerPin = 8;  // Connect buzzer to pin 8
const int ledPin = 9;     // Connect LED to pin 9

// Variables for the duration and the distance
long duration;
int distance;
int thresholdDistance = 20; // Distance in cm to trigger alerts

Servo myServo; // Creates the servo object to control the motor

void setup() {
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);  // Sets the echoPin as an Input
  pinMode(buzzerPin, OUTPUT); // Set buzzer pin as output
  pinMode(ledPin, OUTPUT);    // Set LED pin as output
  Serial.begin(9600);
  myServo.attach(12); // Defines on which pin is the servo motor attached
}

void loop() {
  // rotates the servo motor from 15 to 165 degrees
  for(int i=15; i<=165; i++){  
    myServo.write(i);
    delay(30);
    distance = calculateDistance(); // Calls a function for calculating the distance measured by the Ultrasonic sensor for each degree
    
    // Check if object is detected within threshold
    if(distance < thresholdDistance && distance > 0) {
      digitalWrite(ledPin, HIGH); // Turn on LED
      tone(buzzerPin, 1000, 50);  // Beep at 1kHz for 50ms
    } else {
      digitalWrite(ledPin, LOW);  // Turn off LED
      noTone(buzzerPin);          // Stop beeping
    }
    
    Serial.print(i); // Sends the current degree into the Serial Port
    Serial.print(","); // Sends addition character right next to the previous value needed later in the Processing IDE for indexing
    Serial.print(distance); // Sends the distance value into the Serial Port
    Serial.print("."); // Sends addition character right next to the previous value needed later in the Processing IDE for indexing
  }
  
  // Repeats the previous lines from 165 to 15 degrees
  for(int i=165; i>15; i--){  
    myServo.write(i);
    delay(30);
    distance = calculateDistance();
    
    // Check if object is detected within threshold
    if(distance < thresholdDistance && distance > 0) {
      digitalWrite(ledPin, HIGH); // Turn on LED
      tone(buzzerPin, 1000, 50);  // Beep at 1kHz for 50ms
    } else {
      digitalWrite(ledPin, LOW);  // Turn off LED
      noTone(buzzerPin);          // Stop beeping
    }
    
    Serial.print(i);
    Serial.print(",");
    Serial.print(distance);
    Serial.print(".");
  }
}

// Function for calculating the distance measured by the Ultrasonic sensor
int calculateDistance(){ 
  digitalWrite(trigPin, LOW); 
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH); 
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH); // Reads the echoPin, returns the sound wave travel time in microseconds
  distance = duration*0.034/2;
  return distance;
}