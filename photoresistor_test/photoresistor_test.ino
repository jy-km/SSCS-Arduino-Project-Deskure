
const int sensorPin = 2;
const int ledPin = 4;

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
}

void loop() {
int value = analogRead(sensorPin);

Serial.print("Light level: ");
Serial.println(value);

if(value < 400){
  digitalWrite(ledPin, HIGH);
  delay(2000);
  digitalWrite(ledPin, LOW);

} else {
  digitalWrite(ledPin, LOW);

}


delay(200);
}
