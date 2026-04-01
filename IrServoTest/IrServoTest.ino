#define SERVO_PIN 26
#define PWM_FREQ 50
#define PWM_RES 16
#define IR_Sensor 14
int IR;  

void setup() {
  pinMode(IR_Sensor, INPUT); 
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting servo test...");
  ledcAttach(SERVO_PIN, PWM_FREQ, PWM_RES);
}

void loop() {

  IR=digitalRead(IR_Sensor);  
  if(IR==LOW){              
  Serial.println("Servo: 90 degrees");
  ledcWrite(SERVO_PIN, 4915);
  delay(1000);
}
else {
  Serial.println("Servo: 0 degrees");
  ledcWrite(SERVO_PIN, 1638);
  delay(1000);
}
}