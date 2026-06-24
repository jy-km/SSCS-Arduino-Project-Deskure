#include <Wire.h>
#include <LiquidCrystal_I2C.h>


// Initialize the LCD (Address 0x27 is standard, 16x2)
LiquidCrystal_I2C lcd(0x27, 16, 2);


void setup() {
  // Start Serial communication for the computer screen
  Serial.begin(115200);
 
  // Initialize LCD
  lcd.init();
  lcd.backlight();
 
  // Print to Serial Monitor
  Serial.println("Hello World!");
 
  // Print to LCD
  lcd.setCursor(0, 0); // Column 0, Row 0
  lcd.print("Hello World!");
}


void loop() {
  // Nothing needed here for this simple test
}
