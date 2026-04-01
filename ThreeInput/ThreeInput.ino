#include <OneWire.h>
#include <DallasTemperature.h>

// Temperature
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Turbidity
#define TURBIDITY_PIN 34
int turbidityValue = 0;

// IR Sensor
#define IR_SENSOR 14
int IR_value;

#define SERVO_PIN 26

void setup() {
  Serial.begin(115200);

  sensors.begin();

  pinMode(IR_SENSOR, INPUT);

  Serial.println("System Starting...");
}

void loop() {

  // For Temperature
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  Serial.print("Water Temperature: ");
  Serial.print(tempC);
  Serial.println(" °C");

  if (tempC < 18) {
    Serial.println("Warning: Too cold for crayfish!");
  } 
  else if (tempC > 24) {
    Serial.println("Warning: Too warm for crayfish!");
  } 
  else {
    Serial.println("Temperature is safe for crayfish.");
  }

  // For Turbidity
  turbidityValue = analogRead(TURBIDITY_PIN);
  Serial.print("Turbidity Value: ");
  Serial.println(turbidityValue);

  if (turbidityValue > 2000) {
    Serial.println("Water is Dirty!");
  } else {
    Serial.println("Water clarity acceptable.");
  }

  // IR Sensor
  IR_value = digitalRead(IR_SENSOR);

  if (IR_value == LOW) {
    Serial.println("IR Detected Object!");
  } else {
    Serial.println("No Object Detected.");
  }

  Serial.println("----------------------------");
  delay(2000);
}
