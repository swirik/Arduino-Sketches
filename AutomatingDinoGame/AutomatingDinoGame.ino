#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);

byte dino[8] = {
  0b00100,
  0b00110,
  0b00111,
  0b01110,
  0b11110,
  0b00100,
  0b01010,
  0b01010
};

byte cactus[8] = {
  0b00100,
  0b00100,
  0b10101,
  0b11111,
  0b01110,
  0b01110,
  0b01110,
  0b00000
};

int dinoPos = 1;
int cactusPos = 15;
bool jumping = false;
int jumpCounter = 0;
const int jumpDuration = 3; 
const int buttonPin = 2; 
int lastButtonState = HIGH; 

void setup() {
  lcd.begin();
  lcd.backlight();
  
  lcd.createChar(0, dino);
  lcd.createChar(1, cactus);
  
  pinMode(buttonPin, INPUT_PULLUP);
  
  lcd.setCursor(dinoPos, 1);
  lcd.write(byte(0)); 
}

void loop() {
  lcd.clear();
  int buttonState = digitalRead(buttonPin);
  if (buttonState == LOW && lastButtonState == HIGH) {
    if (!jumping) {
      jumping = true;
      jumpCounter = 0;
    }
  }
  lastButtonState = buttonState; 

  if (jumping) {
    jumpCounter++;
    if (jumpCounter >= jumpDuration) {
      jumping = false;
    }
  }

  lcd.setCursor(dinoPos, jumping ? 0 : 1);
  lcd.write(byte(0)); 

  if (cactusPos > 0) {
    cactusPos--;
  } else {
    cactusPos = 15;
  }
  
  lcd.setCursor(cactusPos, 1);
  lcd.write(byte(1));
  
  delay(300); 
}