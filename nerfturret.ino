/*
  taken base from ESP8266 mDNS responder sample

  This is an example of an HTTP server that is accessible
  via http://nerfturret.local URL thanks to mDNS responder.

  Instructions:
  - Update WiFi SSID and password as necessary.
  - Flash the sketch to the ESP8266 board
  - Install host software:
    - For Linux, install Avahi (http://avahi.org/).
    - For Windows, install Bonjour (http://www.apple.com/support/bonjour/).
    - For Mac OSX and iOS support is built in through Bonjour already.
  - Point your browser to http://esp8266.local, you should see a response.

 */


#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <ESP8266WebServer.h>
#include <Adafruit_PWMServoDriver.h>
#include "wlan_config.h"


// TCP server at port 80 will respond to HTTP requests
ESP8266WebServer server(80);

// I2C PWM driver
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
#define SERVOMIN  150 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX  600 // this is the 'maximum' pulse length count (out of 4096)

#define FEEDPIN 2
#define FEEDMIN 300
#define FEEDMAX 460

#define TILTMIN  275
#define TILTNEUT 360
#define TILTMAX  360

#define PANMIN  175
#define PANNEUT 375
#define PANMAX  575

#define MOTOR D5

// Global variables storing current status
int motorValue;
int panValue;
int tiltValue;

void setup(void)
{  
  initMotor();
  Serial.begin(115200);
  initUi();  
  initServos();
}

void initMotor() {
  pinMode(MOTOR, OUTPUT);
  motorValue = 0;
  analogWrite(MOTOR, motorValue);
}

void initUi() {
  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.println("Init");  
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Set up mDNS responder:
  // - first argument is the domain name, in this example
  //   the fully-qualified domain name is "esp8266.local"
  // - second argument is the IP address to advertise
  //   we send our IP address on the WiFi network
  if (!MDNS.begin("nerfturret")) {
    Serial.println("Error setting up MDNS responder!");
    while(1) { 
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  
  // Start TCP (HTTP) server
  server.on("/position", targetPositionApi);
  server.on("/shoot", shootApi);
  server.on("/shootat", positionShootApi);
  server.on("/demo", demoMode);
  server.begin();
  
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);

  Serial.println("HTTP server started at http://nerfturret.local");
}

void initServos() {
  pwm.begin();
  pwm.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
  pwm.setPWM(FEEDPIN, 0, FEEDMIN);
  targetPosition(PANNEUT, TILTNEUT); // Setting to neutral target position
}

void loop(void)
{
    server.handleClient();
}


void targetPositionApi() {
  int pan = panValue;
  if (server.hasArg("pan")) pan = server.arg("pan").toInt();
  int tilt = tiltValue;
  if (server.hasArg("tilt")) tilt = server.arg("tilt").toInt();
  targetPosition(pan, tilt);
  server.send(200, "text/plain", "Repositioned accordingly");
}

void shootApi() {  
  int count = 1;
  if (server.hasArg("count")) count = server.arg("count").toInt();
  int force = 1023;
  if (server.hasArg("force")) force = server.arg("force").toInt();
  shoot(count, force);
  server.send(200, "text/plain", "Fired");
}

void positionShootApi() {
  int pan = panValue;
  if (server.hasArg("pan")) pan = server.arg("pan").toInt();
  int tilt = tiltValue;
  if (server.hasArg("tilt")) tilt = server.arg("tilt").toInt();
  targetPosition(pan, tilt);
  int count = 1;
  if (server.hasArg("count")) count = server.arg("count").toInt();
  int force = 1023;
  if (server.hasArg("force")) force = server.arg("force").toInt();
  shoot(count, force);
  server.send(200, "text/plain", "Zilä und Schüsse gmacht chefe");
}

void targetPosition(int pan, int tilt) {
  // Range check, set to boundaries if exceeding limits
  if(pan < PANMIN) pan = PANMIN;
  else if(pan > PANMAX) pan = PANMAX;
  if(tilt < TILTMIN) tilt = TILTMIN;
  else if(tilt > TILTMAX) tilt = TILTMAX;
  Serial.print("Set target position to: PAN[");
  Serial.print(pan);
  Serial.print("] TILT[");
  Serial.print(tilt);
  Serial.println("]");
  panValue=pan;
  tiltValue=tilt;
  writeTargetPosition();
}

void writeTargetPosition() {
  pwm.setPWM(0, 0, panValue);
  pwm.setPWM(1, 0, tiltValue);
}

void shootFeed(int count) {
  for(int i=0; i<count; i++) {
    pwm.setPWM(FEEDPIN, 0, FEEDMAX);
    delay(500);
    pwm.setPWM(FEEDPIN, 0, FEEDMIN);
    delay(500);
  }
}

void shoot(int count, int force) {
  // add motor speedup function here
  Serial.print("Shoot ");
  Serial.println(count);
  setMotor(force);
  delay(1000);
  // add feeder Servo here
  shootFeed(count);
  turnOffMotor();
}

/**
 * Used for setting the value directly, it includes a 5ms delay
 */
void writeMotor() {
  analogWrite(MOTOR, motorValue);
  delay(5);
}

/**
 * Used to gradually transition to the desired force from wherever it is right now
 */
void setMotor(int steps) {
  // Check boundaries and autofix to limits
  if(steps < 0) steps = 0; // 0 is the minimum
  else if(steps > 1023) steps = 1023; // 1023 is the maximum
  
  if(steps < motorValue) {
    // turn down the force
    for(;motorValue >= steps; motorValue--) {
      writeMotor();
    }
  } else if(steps > motorValue) {
     // spin up the force
    for(;motorValue <= steps; motorValue++) {
      writeMotor();
    }
  } 
  
}

/**
 * Shut down the motor completely
 */
void turnOffMotor() {
  analogWrite(MOTOR, 0);
  motorValue = 0;
}

// you can use this function if you'd like to set the pulse length in seconds
// e.g. setServoPulse(0, 0.001) is a ~1 millisecond pulse width. its not precise!
void setServoPulse(uint8_t n, double pulse) {
  double pulselength;
  
  pulselength = 1000000;   // 1,000,000 us per second
  pulselength /= 60;   // 60 Hz
  Serial.print(pulselength); Serial.println(" us per period"); 
  pulselength /= 4096;  // 12 bits of resolution
  Serial.print(pulselength); Serial.println(" us per bit"); 
  pulse *= 1000;
  pulse /= pulselength;
  Serial.println(pulse);
  pwm.setPWM(n, 0, pulse);
}

/**
 * Demo AI function to showcase positioning and shooting
 */
void demoMode() {
  targetPosition(PANNEUT, TILTNEUT);
  delay(1000);
  targetPosition(PANMAX, TILTNEUT);
  delay(1000);
  targetPosition(PANMIN, TILTNEUT);
  delay(1000);
  targetPosition(PANNEUT, TILTMAX);
  delay(1000);
  targetPosition(PANNEUT, TILTMIN);
  delay(1000);
  targetPosition(PANNEUT, TILTNEUT);
  delay(1000);
  shoot(1, 300);
  delay(1000);
  setMotor(1023);
  delay(1000);
  for(int i=0;i<=75;i+=15) {
    targetPosition(PANNEUT - i, tiltValue +16);
    shootFeed(1);
  }
  for(int i=0;i<=75;i+=15) {
    targetPosition(PANNEUT - (75 + i), tiltValue -16);
    shootFeed(1);
  }
  turnOffMotor();
  targetPosition(PANNEUT, TILTNEUT);
  server.send(200, "text/plain", "Move on, nothing to see here... Demo is already over");
  
}

