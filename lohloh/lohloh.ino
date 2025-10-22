#include <string.h>;

String s_in;
int s_int;
int led2 = 8;
int led1 = 2;
bool isOn[3] = {false, false, false};

void setup() {
  Serial.begin(9600);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);

}

void loop() {
  if (Serial.available()) {
    s_in = Serial.readString();
    s_in.trim();
    if (s_in == "led1") s_int = 1;
    else if (s_in == "led2") s_int = 2;
    else if (s_in == "led1andled2") s_int = 3;
    else s_int = 4;
    switch(s_int) {
      case 1:
          Serial.print("Led 1 is ");
          if (!isOn[0]) {
            Serial.println("On");
            digitalWrite(led1, HIGH);
            isOn[0] = true;
          }
          else if (isOn[0]) {
            Serial.println("Off");
            digitalWrite(led1, LOW);
            isOn[0] = false;
          }
          break;
      case 2:
          Serial.print("Led 2 is ");
          if (!isOn[1]) {
            Serial.println("On");
            digitalWrite(led2, HIGH);
            isOn[1] = true;
          }
          else if (isOn[1]) {
            Serial.println("Off");
            digitalWrite(led2, LOW);
            isOn[1] = false;
          }
          break;
      case 3:
        if ((isOn[0] && isOn[1]) || (!isOn[0] && !isOn[1])) {
          Serial.print("Led 1 and 2 is ");
          if (!isOn[1]) {
            Serial.println("On");
            digitalWrite(led2, HIGH);
            digitalWrite(led1, HIGH);
            isOn[1] = true;
            isOn[0] = true;
          }
          else if (isOn[1]) {
            Serial.println("Off");
            digitalWrite(led2, LOW);
            digitalWrite(led1, LOW);
            isOn[1] = false;
            isOn[0] = false;
          }
        }
        break;
      default :
        Serial.println("INVALID INPUT");
    }
  }
  delay(50);
}