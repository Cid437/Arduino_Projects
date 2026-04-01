#define SERVO_PIN 26
#define PWM_FREQ 50
#define PWM_RES 16

// 5 IR sensor pins
#define IR1 14
#define IR2 27
#define IR3 25
#define IR4 33
#define IR5 32

// Servo positions
#define SERVO_OPEN 4915
#define SERVO_CLOSE 1638

// Time constants
const unsigned long oneSecond = 1000UL;
const unsigned long oneMinute = 60 * oneSecond;
const unsigned long oneHour   = 60 * oneMinute;

// Feeder timings
const unsigned long openDuration = 1 * oneSecond;   // Servo stays open 1 second
const unsigned long cooldown    = 5 * oneSecond;   // Wait 10 seconds before next feed

// Timing trackers
unsigned long lastFeedTime = 0;
bool isFeeding = false;   // Is servo currently open?
bool inCooldown = false;  // Is cooldown active?

void setup() {
  pinMode(IR1, INPUT);
  pinMode(IR2, INPUT);
  pinMode(IR3, INPUT);
  pinMode(IR4, INPUT);
  pinMode(IR5, INPUT);

  Serial.begin(115200);
  ledcAttach(SERVO_PIN, PWM_FREQ, PWM_RES);

  Serial.println("Fish feeder system started...");
}

void loop() {

  // Count detected IR sensors
  int countDetected = 0;
  if (digitalRead(IR1) == LOW) countDetected++;
  if (digitalRead(IR2) == LOW) countDetected++;
  if (digitalRead(IR3) == LOW) countDetected++;
  if (digitalRead(IR4) == LOW) countDetected++;
  if (digitalRead(IR5) == LOW) countDetected++;

  Serial.print("Detected sensors: ");
  Serial.println(countDetected);

  unsigned long currentTime = millis();

  // Start feeding if ≤ 2 sensors detect and not feeding or in cooldown
  if (!isFeeding && !inCooldown && countDetected <= 2) {
    Serial.println("Feeder opening...");
    ledcWrite(SERVO_PIN, SERVO_OPEN);
    lastFeedTime = currentTime;
    isFeeding = true;
  }

  // Close servo after openDuration
  if (isFeeding && (currentTime - lastFeedTime >= openDuration)) {
    ledcWrite(SERVO_PIN, SERVO_CLOSE);
    Serial.println("Feeder closed, entering cooldown");
    isFeeding = false;
    inCooldown = true;
    lastFeedTime = currentTime;  // start cooldown timer
  }

  // End cooldown after cooldown time
  if (inCooldown && (currentTime - lastFeedTime >= cooldown)) {
    inCooldown = false;
    Serial.println("Cooldown ended, feeder ready");
  }

  // 🔹 Serial print time left until next feed
  if (!isFeeding && inCooldown) {
    unsigned long timeLeft = cooldown - (currentTime - lastFeedTime);
    Serial.print("Time until next feed: ");
    Serial.print(timeLeft / 1000); // in seconds
    Serial.println(" seconds");
  }

  delay(500);  // small loop delay for stability
}