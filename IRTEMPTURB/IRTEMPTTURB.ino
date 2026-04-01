#include <esp_now.h>
#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define GROUP_ID 7
#define GROUP_NAME "Group 7"

uint8_t receiverMAC[] = {0x30,0x76,0xF5,0x91,0x6C,0xBC};

#define ESPNOW_MAX_FIELDS 8

struct __attribute__((packed)) DataField{
  char name[16];
  char unit[6];
  float value;
  uint8_t type;
  uint8_t _pad;
};

typedef struct __attribute__((packed)){
  uint8_t groupId;
  char groupName[20];
  uint8_t fieldCount;
  DataField fields[ESPNOW_MAX_FIELDS];
}EspNowPacket;

esp_now_peer_info_t espNowPeer;
bool espNowReady=false;

////////////////////////////
// WIFI
////////////////////////////

const char* ssid="nonstem";
const char* password="coaa_ns2a";

WebServer server(80);
bool wifiConnected=false;

////////////////////////////
// PIN DEFINITIONS
////////////////////////////

#define ONE_WIRE_BUS 4
#define TURBIDITY_PIN 34

#define IR1 14
#define IR2 27
#define IR3 25
#define IR4 33
#define IR5 32

#define RELAY1 18
#define RELAY2 19
#define RELAY3 21
#define RELAY4 22

const int relayPins[]={RELAY1,RELAY2,RELAY3,RELAY4};

////////////////////////////
// FEEDER SERVO
////////////////////////////

#define SERVO_PIN 26
#define PWM_FREQ 50
#define PWM_RES 16

#define SERVO_OPEN 4915
#define SERVO_CLOSE 1638

////////////////////////////
// FEEDER TIMING
////////////////////////////

const unsigned long openDuration = 1000;
const unsigned long cooldown = 1 * 60 * 1000;

unsigned long lastFeedTime=0; 

bool isFeeding=false;
bool inCooldown=false;
bool manualFeed=false;

unsigned long cooldownRemaining=0;

////////////////////////////
// SENSOR SYSTEM
////////////////////////////

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

float tempC;
int turbidityValue;
float turbidityNTU;
int countDetected=0;

bool pumpState=false;
bool subPumpState=false;

unsigned long lastSensorRead=0;
const unsigned long SENSOR_INTERVAL=1000;

////////////////////////////
// FUNCTION PROTOTYPES
////////////////////////////

void handleRoot();
void handleFeed();

////////////////////////////
// ESP NOW CALLBACK
////////////////////////////

void OnEspNowSent(const wifi_tx_info_t *info, esp_now_send_status_t status){
Serial.printf("[ESP-NOW] Delivery: %s\n",
status==ESP_NOW_SEND_SUCCESS?"OK":"FAIL");
}

////////////////////////////
// ADD FIELD
////////////////////////////

static void addField(EspNowPacket& pkt,const char* name,const char* unit,float value,uint8_t type){

if(pkt.fieldCount>=ESPNOW_MAX_FIELDS)return;

int i=pkt.fieldCount++;

strncpy(pkt.fields[i].name,name,15);
pkt.fields[i].name[15]='\0';

strncpy(pkt.fields[i].unit,unit,5);
pkt.fields[i].unit[5]='\0';

pkt.fields[i].value=value;
pkt.fields[i].type=type;
pkt.fields[i]._pad=0;

}

////////////////////////////
// SEND ESP NOW
////////////////////////////

void espNowSendData(){

if(!espNowReady)return;

EspNowPacket pkt;
memset(&pkt,0,sizeof(pkt));

pkt.groupId=GROUP_ID;
pkt.fieldCount=0;

strncpy(pkt.groupName,GROUP_NAME,19);

addField(pkt,"Temperature","C",tempC,0);
addField(pkt,"Turbidity","NTU",turbidityNTU,0);
addField(pkt,"Fish","",countDetected,0);
addField(pkt,"Turbidity Pump","",pumpState?1:0,1);
addField(pkt,"Temperature Pump","",subPumpState?1:0,1);

esp_now_send(receiverMAC,(uint8_t*)&pkt,sizeof(pkt));

}

////////////////////////////
// SENSOR READ
////////////////////////////

void readTemperature(){
sensors.requestTemperatures();
tempC=sensors.getTempCByIndex(0);
}

void readTurbidity() {
    turbidityValue = analogRead(TURBIDITY_PIN);

    // Map analog reading 0–4095 to NTU 0–1000
    turbidityNTU = map(turbidityValue, 0, 4095, 0, 1000);

    // Ensure floating-point value
    turbidityNTU = constrain(turbidityNTU, 0, 1000);
}

void readFishSensors() {
  countDetected = 0;

  const int IR_THRESHOLD = 1000;

  int irValues[5] = {
    analogRead(IR1),
    analogRead(IR2),
    analogRead(IR3),
    analogRead(IR4),
    analogRead(IR5)
  };

  bool sensorWorking[5] = {true, true, true, true, true};

  for(int i=0; i<5; i++){
    if(sensorWorking[i] && irValues[i] < IR_THRESHOLD){
      countDetected++;
    }
  }
}

////////////////////////////
// AUTOMATION LOGIC
////////////////////////////

void runAutomatedLogic(){

if(tempC>28){
digitalWrite(RELAY3,LOW);
digitalWrite(RELAY4,HIGH);
digitalWrite(RELAY2,LOW);
}

else if(tempC<23){
digitalWrite(RELAY3,HIGH);
digitalWrite(RELAY4,LOW);
digitalWrite(RELAY2,LOW);
}

else{
digitalWrite(RELAY2,HIGH);
digitalWrite(RELAY3,HIGH);
digitalWrite(RELAY4,HIGH);
}


if(turbidityNTU<200)
digitalWrite(RELAY1,LOW);
else
digitalWrite(RELAY1,HIGH);

}

////////////////////////////
// FEEDER SYSTEM
////////////////////////////

unsigned long getCooldownRemaining() {
  if (!inCooldown) return 0;
  unsigned long elapsed = millis() - lastFeedTime;
  if (elapsed >= cooldown) {
    inCooldown = false;
    return 0;
  }
  return (cooldown - elapsed) / 1000; // remaining seconds
}

void runFeeder() {
    unsigned long now = millis();

    // If cooldown is over, automatically feed
    if (!isFeeding && inCooldown == false && countDetected >=4) {
        Serial.println("Automatic Feeding Started");

        // Open feeder servo
        ledcWrite(SERVO_PIN, SERVO_OPEN);
        digitalWrite(RELAY1, LOW); // optionally turn off main pump during feeding

        lastFeedTime = now;  // start cooldown immediately
        isFeeding = true;
        inCooldown = true;   // mark cooldown active
    }

    // Close feeder after openDuration
    if (isFeeding && now - lastFeedTime >= openDuration) {
        ledcWrite(SERVO_PIN, SERVO_CLOSE);
        digitalWrite(RELAY1, HIGH); // restore main pump
        isFeeding = false;

        // Reset cooldown after feeding
        lastFeedTime = now;
        inCooldown = true;
        Serial.println("Feeding Finished, Cooldown Started");
    }

    // Update cooldownRemaining for dashboard
    if (inCooldown) {
        unsigned long elapsed = now - lastFeedTime;
        if (elapsed >= cooldown) {
            inCooldown = false;      // cooldown finished
            cooldownRemaining = 0;
        } else {
            cooldownRemaining = (cooldown - elapsed) / 1000; // seconds
        }
    } else {
        cooldownRemaining = 0;
    }

    // Manual feed overrides automatic feed
    if (manualFeed && !isFeeding) {
        Serial.println("Manual Feeding Started");

        ledcWrite(SERVO_PIN, SERVO_OPEN);
        digitalWrite(RELAY1, LOW);

        lastFeedTime = now;
        isFeeding = true;
        inCooldown = true;
        manualFeed = false;
    }
}
////////////////////////////
// WEB PAGE (UPDATED DISPLAY ONLY)
////////////////////////////

void handleRoot() {
  pumpState = (digitalRead(RELAY1) == LOW);
  subPumpState = (digitalRead(RELAY2) == LOW);
  bool peltierHeat = (digitalRead(RELAY3) == LOW);
  bool peltierCool = (digitalRead(RELAY4) == LOW);

  // Temperature percentage for slider
  int tempPercent = constrain((tempC / 40.0) * 100, 0, 100);

  // Turbidity percentage slider (bounded 50–1000 NTU)
  int turbMin = 50;
  int turbMax = 1000;
  int turbPercent;
  float ntuDisplay = turbidityNTU; // use NTU value for slider
  if (ntuDisplay >= turbMax) {
    turbPercent = 100;
  } else if (ntuDisplay <= turbMin) {
    turbPercent = 0;
  } else {
    turbPercent = (int)((ntuDisplay - turbMin) / float(turbMax - turbMin) * 100);
  }

  String page = "<!DOCTYPE html><html><head>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<meta http-equiv='refresh' content='3'>";
  page += "<style>";
  page += "body{font-family:Arial;background:#0f172a;color:white;margin:0;text-align:center;}";
  page += "h1{padding:15px;}";
  page += ".container{display:flex;flex-wrap:wrap;justify-content:center;}";
  page += ".card{background:#1e293b;border-radius:10px;padding:20px;margin:10px;width:180px;box-shadow:0 0 10px rgba(0,0,0,0.4);}";
  page += ".value{font-size:28px;font-weight:bold;margin-top:10px;}";
  page += ".bar{position:relative;height:15px;border-radius:10px;margin-top:15px;overflow:hidden;}";
  page += ".tempBar{background:linear-gradient(to right, blue, red);}";
  page += ".turbBar{background:linear-gradient(to right, #5c4033, rgba(255,255,255,0.7));}";
  page += ".slider{position:absolute;top:-3px;width:6px;height:22px;background:white;border-radius:3px;}";
  page += ".statusPanel{background:#111827;margin:20px;padding:20px;border-radius:10px;width:90%;max-width:500px;margin-left:auto;margin-right:auto;}";
  page += ".statusRow{display:flex;justify-content:space-between;padding:8px;border-bottom:1px solid #333;}";
  page += ".on{color:#22c55e;font-weight:bold;}";
  page += ".off{color:#ef4444;font-weight:bold;}";
  page += "button{padding:12px 25px;border:none;border-radius:8px;background:#22c55e;color:white;font-size:18px;cursor:pointer;margin-top:20px;}";
  page += "button:hover{background:#16a34a;}";
  page += "</style></head><body>";

  page += "<h1>Aquarium Monitoring Dashboard</h1>";
  page += "<div class='container'>";

  // Temperature
  page += "<div class='card'><div>Temperature</div><div class='value'>" + String(tempC) + " &deg;C</div>";
  page += "<div class='bar tempBar'><div class='slider' style='left:" + String(tempPercent) + "%'></div></div></div>";

  // Turbidity in NTU
  page += "<div class='card'><div>Turbidity</div><div class='value'>" + String(turbidityNTU, 1) + " NTU</div>";
  page += "<div class='bar turbBar'><div class='slider' style='left:" + String(turbPercent) + "%'></div></div></div>";

  // Fish detected
  page += "<div class='card'><div>Fish Detected</div><div class='value'>" + String(countDetected) + "</div></div>";

  // IR Sensor Values
  page += "<div class='card'><div>IR Sensor Values</div><div class='value'>";
  page += "IR1: " + String(analogRead(IR1)) + "<br>";
  page += "IR2: " + String(analogRead(IR2)) + "<br>";
  page += "IR3: " + String(analogRead(IR3)) + "<br>";
  page += "IR4: " + String(analogRead(IR4)) + "<br>";
  page += "IR5: " + String(analogRead(IR5));
  page += "</div></div>";

  // Feeder Cooldown
  page += "<div class='card'><div>Feeder Cooldown</div><div class='value'>" + String(cooldownRemaining) + " s</div></div>";

  page += "</div>"; // container end

  // System Status Panel
  page += "<div class='statusPanel'><h2>System Status</h2>";
  page += "<div class='statusRow'><span>Filter Pump Pump</span><span class='" + String(pumpState ? "on" : "off") + "'>" + String(pumpState ? "ON" : "OFF") + "</span></div>";
  page += "<div class='statusRow'><span>Temperature Pump</span><span class='" + String(subPumpState ? "on" : "off") + "'>" + String(subPumpState ? "ON" : "OFF") + "</span></div>";
  page += "<div class='statusRow'><span>Peltier (Cooling)</span><span class='" + String(peltierHeat ? "on" : "off") + "'>" + String(peltierHeat ? "ON" : "OFF") + "</span></div>";
  page += "<div class='statusRow'><span>Peltier (Heating)</span><span class='" + String(peltierCool ? "on" : "off") + "'>" + String(peltierCool ? "ON" : "OFF") + "</span></div>";
  page += "</div>";

  page += "<a href='/feed'><button>FEED NOW</button></a>";
  page += "</body></html>";

  server.send(200, "text/html", page);
}

void handleFeed(){
  manualFeed = true;
  server.sendHeader("Location","/");
  server.send(303);
}

////////////////////////////
// SETUP
////////////////////////////

void setup(){

  Serial.begin(115200);

  WiFi.mode(WIFI_STA);

  pinMode(IR1,INPUT);
  pinMode(IR2,INPUT);
  pinMode(IR3,INPUT);
  pinMode(IR4,INPUT);
  pinMode(IR5,INPUT);

  for(int i=0;i<4;i++){
  pinMode(relayPins[i],OUTPUT);
  digitalWrite(relayPins[i],HIGH);
  }

  sensors.begin();
  ledcAttach(SERVO_PIN,PWM_FREQ,PWM_RES);

  WiFi.begin(ssid,password);

  int attempts=0;
  while(WiFi.status()!=WL_CONNECTED && attempts<20){
  delay(500);
  attempts++;
  }

if(WiFi.status()==WL_CONNECTED){
  wifiConnected=true;

  Serial.println("WiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());   // IP

  server.on("/",handleRoot);
  server.on("/feed",handleFeed);
  server.begin();
}

  if(esp_now_init()==ESP_OK){
  esp_now_register_send_cb(OnEspNowSent);
  memcpy(espNowPeer.peer_addr,receiverMAC,6);
  espNowPeer.channel=0;
  espNowPeer.encrypt=false;

  if(esp_now_add_peer(&espNowPeer)==ESP_OK){
  espNowReady=true;
    }
  }

  lastFeedTime = millis();
  inCooldown = true;
}

////////////////////////////
// LOOP
////////////////////////////

void loop(){

readTemperature();
readTurbidity();
readFishSensors();

pumpState=(digitalRead(RELAY1)==LOW);
subPumpState=(digitalRead(RELAY2)==LOW);

runAutomatedLogic();
runFeeder();

if(wifiConnected)
server.handleClient();

unsigned long now=millis();

if(now-lastSensorRead>=SENSOR_INTERVAL){
lastSensorRead=now;
espNowSendData();
}

if(Serial.available()){
if(Serial.read()=='f'){
manualFeed=true;
}
}

delay(10);
}