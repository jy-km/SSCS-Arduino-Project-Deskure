
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_LSM6DS3TRC.h>
#include <Adafruit_LIS3MDL.h>
#include <MadgwickAHRS.h>

//Bluetooth Setup
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLEDescriptor *pDescr;
BLE2902 *pBLE2902;

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

unsigned long lastBleSend = 0;
const unsigned long bleSendInterval = 500; // ms — don't flood the radio

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
unsigned long currentMillis = 0;

const unsigned long imuInterval = 50;  
const unsigned long envInterval = 100;


// State Variables
bool isBuzzing = false;
String postureMsg = "";
int currentPage = 0;

//global variables for Bluetooth transmission--all declared here
float distanceCm;
int lightValue;
pitch = filter.getPitch();
Serial.println(pitch);    // <-- add this line


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

//Bluetooth callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void setup() {
  Serial.begin(115200);
  delay(500); // give serial monitor time to attach
  Serial.println("1. Serial started");

  Wire.begin();
  Serial.println("2. Wire started");

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Ready...");
  Serial.println("3. LCD initialized");

  BLEDevice::init("Deskure");   // add this if missing
  Serial.println("4. BLEDevice::init done");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  Serial.println("5. BLE server created");

  BLEService *pService = pServer->createService(SERVICE_UUID);
  Serial.println("6. BLE service created");

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  Serial.println("7. Characteristic created");

  pDescr = new BLEDescriptor((uint16_t)0x2901);
  pDescr->setValue("A very interesting variable");
  pCharacteristic->addDescriptor(pDescr);

  pBLE2902 = new BLE2902();
  pBLE2902->setNotifications(true);
  pCharacteristic->addDescriptor(pBLE2902);
  Serial.println("8. Descriptors added");

  pService->start();
  Serial.println("9. Service started");

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEAdvertisementData scanResponseData;
  scanResponseData.setName("Deskure");
  pAdvertising->setScanResponseData(scanResponseData);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("10. Advertising started");

  if (!lsm6ds3trc.begin_I2C() || !lis3mdl.begin_I2C()) {
    Serial.println("11. IMU INIT FAILED");
    lcd.clear();
    lcd.print("IMU Init Fail");
    while (1);
  }
  Serial.println("11. IMU init OK");

  filter.begin(1000.0 / imuInterval);
  Serial.println("12. Filter begin OK");
  delay(1000);
  Serial.println("13. Setup complete, entering loop");
}


void loop() {
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 2000) {
    Serial.println("loop alive");
    lastHeartbeat = millis();
  }

  currentMillis = millis();


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


    pitch = filter.getPitch();


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
    lightValue = analogRead(sensorPin);
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
    distanceCm = duration * 0.0343 / 2;


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
  /*
  5. Update and transmit data via Bluetooth BLE
  */
  if (deviceConnected && (millis() - lastBleSend >= bleSendInterval)) {
    String bleData = "{\"d\":" + String(distanceCm, 1) + ",\"l\":" + String(lightValue) + ",\"p\":" + String(pitch) + "}";
    pCharacteristic->setValue(bleData.c_str());
    pCharacteristic->notify();
    lastBleSend = millis();
  }
}
