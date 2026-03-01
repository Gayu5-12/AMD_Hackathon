#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <WebServer.h>

// -------- WiFi ----------
char ssid[] = "Redmi";
char pass[] = "Gayu1223";

// -------- Web Server ----------
WebServer server(80);

// -------- Pins ----------
#define FLAME_SENSOR 26
#define HOSE_SERVO 27
#define SENSOR_SERVO 14
#define PUMP_PIN 23

#define IN1 33
#define IN2 32
#define IN3 19
#define IN4 18

// -------- Settings ----------
int motorSpeed = 200;
const int scanStep = 5;
const int scanDelay = 40;
const int moveDelay = 800;

// -------- States ----------
bool manualMode = false;
String fireStatus = "No Fire";
int currentServoAngle = 90;

// -------- Servo ----------
Servo sensorServo;
Servo hoseServo;

// -------- Motor ----------
void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void moveForward(int speed) {
  analogWrite(IN1, speed);
  analogWrite(IN2, 0);
  analogWrite(IN3, speed);
  analogWrite(IN4, 0);
}

void turnLeft(int speed) {
  analogWrite(IN1, speed);
  analogWrite(IN2, 0);
  analogWrite(IN3, 0);
  analogWrite(IN4, speed);
}

void turnRight(int speed) {
  analogWrite(IN1, 0);
  analogWrite(IN2, speed);
  analogWrite(IN3, speed);
  analogWrite(IN4, 0);
}

// -------- Fire Detect ----------
bool isFireDetected() {
  int count = 0;
  for (int i=0;i<3;i++){
    if(digitalRead(FLAME_SENSOR)==HIGH) count++;
    delay(5);
  }
  return (count>=2);
}

// -------- Scan ----------
int scanForFire() {
  static int angle = 0;
  static bool forwardScan = true;
  int detected = -1;

  sensorServo.write(angle);
  hoseServo.write(angle);
  currentServoAngle = angle;
  delay(scanDelay);

  if(isFireDetected()){
    fireStatus="Fire Detected!";
    detected = angle;
  }

  if(forwardScan){
    angle+=scanStep;
    if(angle>=180) forwardScan=false;
  } else {
    angle-=scanStep;
    if(angle<=0) forwardScan=true;
  }

  return detected;
}

// -------- Align ----------
void alignToFire(int angle){
  if(angle<70) turnRight(motorSpeed);
  else if(angle>110) turnLeft(motorSpeed);
  else moveForward(motorSpeed);

  delay(moveDelay);
  stopMotors();
}

// -------- Extinguish ----------
void extinguishFire(int centerAngle){

  digitalWrite(PUMP_PIN, LOW);

  int startAngle = max(0, centerAngle-25);
  int endAngle = min(180, centerAngle+25);

  for(int pos=startAngle; pos<=endAngle; pos+=4){
    hoseServo.write(pos);
    delay(20);
  }

  for(int pos=endAngle; pos>=startAngle; pos-=4){
    hoseServo.write(pos);
    delay(20);
  }

  digitalWrite(PUMP_PIN, HIGH);
  fireStatus="Fire Extinguished";
}

// -------- HTML UI ----------
const char webpage[] PROGMEM = R"====(
<!DOCTYPE html>
<html>
<head>
<title>Fire Robot</title>
<style>
body{font-family:Arial;background:#111;color:#fff;text-align:center;}
.card{background:#222;padding:20px;margin:15px;border-radius:10px;}
button{padding:15px;font-size:18px;margin:10px;}
</style>
</head>
<body>

<h1>🔥 Fire Fighting Robot</h1>

<div class="card">
<h2>Mode</h2>
<button onclick="fetch('/mode?set=auto')">AUTO</button>
<button onclick="fetch('/mode?set=manual')">MANUAL</button>
</div>

<div class="card">
<h2>Pump</h2>
<button onclick="fetch('/pump?set=on')">ON</button>
<button onclick="fetch('/pump?set=off')">OFF</button>
</div>

<div class="card">
<h2>Speed</h2>
<input type="range" min="0" max="255" value="200"
onchange="fetch('/speed?value='+this.value)">
</div>

<div class="card">
<h2>Status</h2>
<p id="fire">Loading...</p>
<p id="servo">Angle...</p>
</div>

<script>
setInterval(()=>{
fetch('/status').then(r=>r.json()).then(data=>{
document.getElementById('fire').innerText=data.fire;
document.getElementById('servo').innerText="Servo: "+data.angle;
});
},1000);
</script>

</body>
</html>
)====";

// -------- Routes ----------
void setupRoutes(){

  server.on("/", [](){
    server.send(200,"text/html",webpage);
  });

  server.on("/mode", [](){
    if(server.arg("set")=="manual") manualMode=true;
    else manualMode=false;
    server.send(200,"text/plain","OK");
  });

  server.on("/pump", [](){
    if(server.arg("set")=="on") digitalWrite(PUMP_PIN,LOW);
    else digitalWrite(PUMP_PIN,HIGH);
    server.send(200,"text/plain","OK");
  });

  server.on("/speed", [](){
    motorSpeed=server.arg("value").toInt();
    server.send(200,"text/plain","OK");
  });

  server.on("/status", [](){
    String json="{\"fire\":\""+fireStatus+"\",\"angle\":"+String(currentServoAngle)+"}";
    server.send(200,"application/json",json);
  });

  server.begin();
}

// -------- Setup ----------
void setup(){

  Serial.begin(115200);

  pinMode(FLAME_SENSOR, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  sensorServo.attach(SENSOR_SERVO);
  hoseServo.attach(HOSE_SERVO);
  sensorServo.write(90);
  hoseServo.write(90);

  digitalWrite(PUMP_PIN,HIGH);
  stopMotors();

  WiFi.begin(ssid,pass);
  while(WiFi.status()!=WL_CONNECTED) delay(500);

  Serial.println("Connected!");
  Serial.println(WiFi.localIP());

  setupRoutes();
}

// -------- Loop ----------
void loop(){

  server.handleClient();

  if(manualMode) return;

  int fireAngle=scanForFire();

  if(fireAngle!=-1){
    stopMotors();
    alignToFire(fireAngle);
    extinguishFire(fireAngle);
  } else {
    fireStatus="No Fire";
    moveForward(motorSpeed/2);
  }
}