#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4); // Adjust for your LCD

// Custom character for a tiny dino
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

// Cactus obstacle
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

// Variables for game
int dinoPos = 1;
int cactusPos = 15;
bool jumping = false;
int jumpCounter = 0;
const int jumpDuration = 3; // How many frames the jump lasts
const int buttonPin = 2; // Button for jumping
int lastButtonState = HIGH; // To detect button press

void setup() {
  lcd.begin();
  lcd.backlight();
  
  lcd.createChar(0, dino);
  lcd.createChar(1, cactus);
  
  pinMode(buttonPin, INPUT_PULLUP);
  
  lcd.setCursor(dinoPos, 1);
  lcd.write(byte(0)); // Show dino
}

void loop() {
  lcd.clear();

  // Read button state
  int buttonState = digitalRead(buttonPin);
  
  // Detect new button press (falling edge)
  if (buttonState == LOW && lastButtonState == HIGH) {
    if (!jumping) { // Only start jump if not already jumping
      jumping = true;
      jumpCounter = 0;
    }
  }
  lastButtonState = buttonState; // Update last state
  
  // Handle jumping
  if (jumping) {
    jumpCounter++;
    if (jumpCounter >= jumpDuration) {
      jumping = false;
    }
  }
  
  // Dino position based on jump state
  lcd.setCursor(dinoPos, jumping ? 0 : 1);
  lcd.write(byte(0)); // Dino     

  // Move cactus left
  if (cactusPos > 0) {
    cactusPos--;
  } else {
    cactusPos = 15;
  }
  
  lcd.setCursor(cactusPos, 1);
  lcd.write(byte(1)); // Cactus
  
  delay(300); // Adjust speed
}