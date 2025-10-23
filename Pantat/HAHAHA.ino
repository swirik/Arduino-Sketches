#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Set LCD address to 0x27 (or 0x3F if needed)
LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup() {
  Wire.begin();
  lcd.begin();
  lcd.backlight();
  lcd.clear();
}

void loop() {
  String text = "PANTATTTT!";
  int length = text.length();

  for (int pos = 0; pos <= 20; pos++) { // Move from left to right
    lcd.clear();
    
    lcd.setCursor(pos, 2); // Print in 1st row
    lcd.print(text);
    
    delay(300);
  }
   // Pause before restarting
}
