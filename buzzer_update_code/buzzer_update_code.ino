#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int trigPin = 5;
const int echoPin = 18;
const int sensorPin = 34;
const int buzzerPin = 4;

unsigned long inRangeStart = 0;      
LiquidCrystal_I2C lcd(0x27, 16, 2);

String currentLine0 = "";
String currentLine1 = "";

unsigned long buzzerTimer = 0;
bool isBuzzing = false;

void beep() {
  digitalWrite(buzzerPin, HIGH);
  buzzerTimer = millis();
  isBuzzing = true;
}

//update function to easily clear/update lines
void updateLCD(int line, String text) {
  // 1. Check if it's an actual warning before we add spaces to it
  bool isActualWarning = false;
  if (text != "") isActualWarning = true;

  // 2. Pad the text FIRST (The Fix!)
  while (text.length() < 16) {
    text += " ";
  }
 
  // 3. NOW check if this 16-character string is different from memory
  bool isDifferent = false;
  if (line == 0 && text != currentLine0) isDifferent = true;
  if (line == 1 && text != currentLine1) isDifferent = true;


  // 4. Trigger the beep only if it changed AND it is a real warning
  if (isDifferent && isActualWarning) {
    beep();
  }

  // 5. Update the physical screen and the memory
  if (line == 0 && isDifferent) {
    lcd.setCursor(0, 0);
    lcd.print(text);
    currentLine0 = text;
  } else if (line == 1 && isDifferent) {
    lcd.setCursor(0, 1);
    lcd.print(text);
    currentLine1 = text;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);

  lcd.init();
  lcd.backlight();
}

void loop() {
  //read
  int lightValue = analogRead(sensorPin);
 
  //firing ultrasonic sensor
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  float distanceCm = duration * 0.0343 / 2;

//string variables as placeholders
  String distMsg = "";
  String lightMsg = "";

//photoresistor threshold
  if (lightValue < 650) {
    lightMsg = "Too dark here!";
  }

//ultrasonic sensor threshold using old calibration logic
  unsigned long currentTime = millis();
 
  if (distanceCm > 0 && distanceCm < 15) {
    if (inRangeStart == 0) inRangeStart = currentTime;
    if (currentTime - inRangeStart >= 10000) {
        distMsg = "Way too close!";
    }
  } else if (distanceCm < 30) {
    if (inRangeStart == 0) inRangeStart = currentTime;
    if (currentTime - inRangeStart >= 20000) {
        distMsg = "Too close!";
    }
  } else {
    inRangeStart = 0;
  }
//filling in the lcd with messages based on satisfied thresholds
  if (distMsg != "" && lightMsg != "") {
    updateLCD(0, distMsg);    
    updateLCD(1, lightMsg);    

  } else if (distMsg != "") {
    updateLCD(0, "WARNING:");
    updateLCD(1, distMsg);
   
  } else if (lightMsg != "") {
    updateLCD(0, "WARNING:");
    updateLCD(1, lightMsg);
   
  } else {
    updateLCD(0, "");
    updateLCD(1, "");
  }

if (isBuzzing && (millis() - buzzerTimer >= 200)) {
    digitalWrite(buzzerPin, LOW);
    isBuzzing = false;
  }
  delay(100);
}
