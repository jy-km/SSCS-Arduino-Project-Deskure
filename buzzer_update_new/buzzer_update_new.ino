#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_LSM6DS3TRC.h>
#include <Adafruit_LIS3MDL.h>
#include <MadgwickAHRS.h>


// Hardware Pins
const int trigPin = 5;
const int echoPin = 18;
const int sensorPin = 34;
const int buzzerPin = 4;


// LCD Setup
LiquidCrystal_I2C lcd(0x27, 16, 2);
String currentLine0 = "";
String currentLine1 = "";


// IMU Setup
Adafruit_LSM6DS3TRC lsm6ds3trc;
Adafruit_LIS3MDL lis3mdl = Adafruit_LIS3MDL();
Madgwick filter;


// Timing Variables
unsigned long inRangeStart = 0;      
unsigned long buzzerTimer = 0;
unsigned long imuLastUpdate = 0;
unsigned long envLastUpdate = 0;
unsigned long postureBadStart = 0; // CRITICAL: IMU Hysteresis timer
unsigned long lastPageSwap = 0;    // Tracks the 3-second LCD alternating cycle


const unsigned long imuInterval = 50;  
const unsigned long envInterval = 100;


// State Variables
bool isBuzzing = false;
String postureMsg = "";
int currentPage = 0;


// Buzzer Activation (0.2 seconds non-blocking)
void beep() {
  digitalWrite(buzzerPin, HIGH);
  buzzerTimer = millis();
  isBuzzing = true;
}


// LCD Update Logic
void updateLCD(int line, String text) {
  // Prevent beeping just because the word "WARNING:" appears
  bool isActualWarning = (text != "" && text != "WARNING:");
 
  while (text.length() < 16) {
    text += " ";
  }
 
  bool isDifferent = false;
  if (line == 0 && text != currentLine0) isDifferent = true;
  if (line == 1 && text != currentLine1) isDifferent = true;


  if (isDifferent && isActualWarning) {
    beep();
  }


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
  Wire.begin();


  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);


  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Ready...");


  // Initialize IMU
  if (!lsm6ds3trc.begin_I2C() || !lis3mdl.begin_I2C()) {
    lcd.clear();
    lcd.print("IMU Init Fail");
    while (1);
  }


  filter.begin(1000.0 / imuInterval);
  delay(1000);
}


void loop() {
  unsigned long currentMillis = millis();


  // ---------------------------------------------------------
  // 1. IMU Polling Loop (Strict 50ms interval)
  // ---------------------------------------------------------
  if (currentMillis - imuLastUpdate >= imuInterval) {
    sensors_event_t accel, gyro, temp, mag;
    lsm6ds3trc.getEvent(&accel, &gyro, &temp);
    lis3mdl.getEvent(&mag);


    filter.update(gyro.gyro.x * RAD_TO_DEG,
                  gyro.gyro.y * RAD_TO_DEG,
                  gyro.gyro.z * RAD_TO_DEG,
                  accel.acceleration.x / 9.80665,
                  accel.acceleration.y / 9.80665,
                  accel.acceleration.z / 9.80665,
                  mag.magnetic.x, mag.magnetic.y, mag.magnetic.z);


    float pitch = filter.getPitch();


    // Hysteresis Logic: Prevents I2C crashing and false alarms
    if (pitch < 43.0 || pitch > 80.0) {
      if (postureBadStart == 0) postureBadStart = currentMillis;
     
      if (currentMillis - postureBadStart >= 1500) {
        if (pitch < 43.0) postureMsg = "Slouching!";
        if (pitch > 80.0) postureMsg = "Too far back!";
      }
    } else {
      postureBadStart = 0;
      postureMsg = "";
    }


    imuLastUpdate = currentMillis;
  }


  // ---------------------------------------------------------
  // 2. Environmental Sensor Loop (100ms interval)
  // ---------------------------------------------------------
  if (currentMillis - envLastUpdate >= envInterval) {
    String distMsg = "";
    String lightMsg = "";


    // Read Light
    int lightValue = analogRead(sensorPin);
    if (lightValue < 650) {
      lightMsg = "Too dark here!";
    }


    // Read Distance
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
   
    long duration = pulseIn(echoPin, HIGH, 30000);
    float distanceCm = duration * 0.0343 / 2;


    if (distanceCm > 0 && distanceCm < 15) {
      if (inRangeStart == 0) inRangeStart = currentMillis;
      if (currentMillis - inRangeStart >= 10000) {
          distMsg = "Way too close!";
      }
    } else if (distanceCm > 0 && distanceCm < 30) {
      if (inRangeStart == 0) inRangeStart = currentMillis;
      if (currentMillis - inRangeStart >= 20000) {
          distMsg = "Too close!";
      }
    } else {
      inRangeStart = 0;
    }


    // ---------------------------------------------------------
    // 3. Dynamic LCD Pagination Matrix
    // ---------------------------------------------------------
    String activeWarnings[4]; // Expanded array to support 4 items safely
    int warningCount = 0;


    if (postureMsg != "") activeWarnings[warningCount++] = postureMsg;
    if (distMsg != "") activeWarnings[warningCount++] = distMsg;
    if (lightMsg != "") activeWarnings[warningCount++] = lightMsg;


    int totalPages = (warningCount + 1) / 2; // Calculates total screens needed


    if (warningCount == 0) {
      updateLCD(0, "");
      updateLCD(1, "");
      currentPage = 0;
    }
    else if (warningCount == 1) {
      updateLCD(0, "WARNING:");
      updateLCD(1, activeWarnings[0]);
      currentPage = 0;
    }
    else if (warningCount == 2) {
      updateLCD(0, activeWarnings[0]);
      updateLCD(1, activeWarnings[1]);
      currentPage = 0;
    }
    else {
      // Logic for 3 or more warnings (Alternates every 3000ms)
      if (currentMillis - lastPageSwap >= 3000) {
        currentPage++;
        if (currentPage >= totalPages) {
          currentPage = 0; // Loop back to the first page
        }
        lastPageSwap = currentMillis;
      }


      int topIndex = currentPage * 2;
      int bottomIndex = topIndex + 1;


      if (bottomIndex < warningCount) {
        // We have two valid warnings for this page
        updateLCD(0, activeWarnings[topIndex]);
        updateLCD(1, activeWarnings[bottomIndex]);
      } else {
        // We only have one warning left for this page (e.g., the 3rd warning)
        updateLCD(0, "WARNING:");
        updateLCD(1, activeWarnings[topIndex]);
      }
    }


    envLastUpdate = currentMillis;
  }


  // ---------------------------------------------------------
  // 4. Hardware Cutoff Loop
  // ---------------------------------------------------------
  if (isBuzzing && (currentMillis - buzzerTimer >= 200)) {
    digitalWrite(buzzerPin, LOW);
    isBuzzing = false;
  }
}
