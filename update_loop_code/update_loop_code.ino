#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

const int trigPin = 5;
const int echoPin = 18;
const int sensorPin = 34; 

unsigned long inRangeStart = 0;      
LiquidCrystal_I2C lcd(0x27, 16, 2);

String currentLine0 = "";
String currentLine1 = "";

//update function to easily clear/update lines
void updateLCD(int line, String text) {
  while (text.length() < 16) {
    text += " ";
  }
  

  if (line == 0 && text != currentLine0) {
    lcd.setCursor(0, 0);
    lcd.print(text);
    currentLine0 = text;
  } else if (line == 1 && text != currentLine1) {
    lcd.setCursor(0, 1);
    lcd.print(text);
    currentLine1 = text;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
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

  delay(100);
}