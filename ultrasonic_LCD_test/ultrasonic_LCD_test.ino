#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

const int trigPin = 5;
const int echoPin = 18;

unsigned long inRangeStart = 0;      
unsigned long lastWarningTime = 0;   
bool thresholdReached = false;       

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  lcd.init();         
  lcd.backlight();   
}

void loop() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  float distanceCm = duration * 0.0343 / 2;

  Serial.print("Distance: ");
  Serial.print(distanceCm);
  Serial.println(" cm");
  
  unsigned long currentTime = millis();

  if (distanceCm > 0 && distanceCm < 15) {
    // z1
    if (inRangeStart == 0) inRangeStart = currentTime;
    if (currentTime - inRangeStart >= 10000) {
        if (!thresholdReached || currentTime - lastWarningTime >= 5000) {
            Serial.println("WARNING: Way too close!");
            lcd.setCursor(0, 0);
            lcd.print("WARNING: ");
            lcd.setCursor(0, 1);
            lcd.print("Way too close!");
            lastWarningTime = currentTime;
            thresholdReached = true;
        }
    }

} else if (distanceCm < 30) {
    // z2
    if (inRangeStart == 0) inRangeStart = currentTime;
    if (currentTime - inRangeStart >= 20000) {
        if (!thresholdReached || currentTime - lastWarningTime >= 5000) {
            Serial.println("WARNING: Too close to screen!");
            lcd.setCursor(0, 0);
            lcd.print("WARNING: ");
            lcd.setCursor(0, 1);
            lcd.print("Too close!");
            lastWarningTime = currentTime;
            thresholdReached = true;
        }
    }

} else {
    // restart/reset
    lcd.setCursor(0, 0);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    inRangeStart = 0;
    thresholdReached = false;
    lastWarningTime = 0;
}

  delay(100);
}