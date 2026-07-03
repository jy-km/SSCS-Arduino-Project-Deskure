#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLEDescriptor *pDescr;
BLE2902 *pBLE2902;

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

const int trigPin = 5;
const int echoPin = 18;
const int sensorPin = 34;
const int buzzerPin = 4;

unsigned long inRangeStart = 0;      
LiquidCrystal_I2C lcd(0x27, 16, 2);
unsigned long lastBleSend = 0;
const unsigned long bleSendInterval = 500; // ms — don't flood the radio

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
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);

  lcd.init();
  lcd.backlight();

  //Bluetooth Setup
// Create the BLE Device
  BLEDevice::init("Deskure");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );                   

  // Create a BLE Descriptor
  
  pDescr = new BLEDescriptor((uint16_t)0x2901);
  pDescr->setValue("A very interesting variable");
  pCharacteristic->addDescriptor(pDescr);
  
  pBLE2902 = new BLE2902();
  pBLE2902->setNotifications(true);
  pCharacteristic->addDescriptor(pBLE2902);

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEAdvertisementData scanResponseData;
  scanResponseData.setName("Deskure");
  pAdvertising->setScanResponseData(scanResponseData);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  //read
  int resistance = analogRead(sensorPin);

  float lightValue = pow(280000.0 / resistance, 1.389);
 
  //firing ultrasonic sensor
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  float distanceCm = duration * 0.0343 / 2;

  if (deviceConnected && (millis() - lastBleSend >= bleSendInterval)) {
  String bleData = "{\"d\":" + String(distanceCm, 1) + ",\"l\":" + String(lightValue) + "}";
  pCharacteristic->setValue(bleData.c_str());
  pCharacteristic->notify();
  lastBleSend = millis();
  }
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
