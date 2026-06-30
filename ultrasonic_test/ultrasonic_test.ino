const int trigPin = 5;
const int echoPin = 18;

unsigned long inRangeStart = 0;      
unsigned long lastWarningTime = 0;   
bool thresholdReached = false;       

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
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
    if (currentTime - inRangeStart >= 15000) {
        if (!thresholdReached || currentTime - lastWarningTime >= 5000) {
            Serial.println("WARNING: Way too close!");
            lastWarningTime = currentTime;
            thresholdReached = true;
        }
    }

} else if (distanceCm < 30) {
    // z2
    if (inRangeStart == 0) inRangeStart = currentTime;
    if (currentTime - inRangeStart >= 30000) {
        if (!thresholdReached || currentTime - lastWarningTime >= 5000) {
            Serial.println("WARNING: Too close to screen!");
            lastWarningTime = currentTime;
            thresholdReached = true;
        }
    }

} else {
    // res
    inRangeStart = 0;
    thresholdReached = false;
    lastWarningTime = 0;
}

  delay(100);
}