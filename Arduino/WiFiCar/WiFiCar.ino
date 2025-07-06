#include <WiFiS3.h>
#include <Servo.h>  // Include Servo library

WiFiServer server(80);  // HTTP server on port 80

const char* ssid = "Mishawifi";
const char* password = "12311231";

// Motor pins
int in1 = 6, in2 = 7, in3 = 8, in4 = 9;
int ENA = 5, ENB = 11;

// Ultrasonic sensor pins
int Echo = A4;  
int Trig = A5; 

// Servo control
Servo myservo;
int servoAngle = 90;  // Initial position of servo

// Motor control functions
void _mStop() {
  digitalWrite(ENA, LOW);
  digitalWrite(ENB, LOW);
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
  Serial.println("Stop");
}

void _mForward() {
  digitalWrite(ENA,HIGH); digitalWrite(ENB,HIGH);
  digitalWrite(in1,HIGH); digitalWrite(in2,LOW);
  digitalWrite(in3,LOW);  digitalWrite(in4,HIGH);
  Serial.println("Forward");
  delay(500);
  _mStop();
}

void _mBack() {
  digitalWrite(ENA,HIGH); digitalWrite(ENB,HIGH);
  digitalWrite(in1,LOW);  digitalWrite(in2,HIGH);
  digitalWrite(in3,HIGH); digitalWrite(in4,LOW);
  Serial.println("Back");
  delay(500);
  _mStop();
}

void _mLeft() {
  digitalWrite(ENA,HIGH); digitalWrite(ENB,HIGH);
  digitalWrite(in1,HIGH); digitalWrite(in2,LOW);
  digitalWrite(in3,HIGH); digitalWrite(in4,LOW);
  Serial.println("Left");
  delay(250);
  _mStop();
}

void _mRight() {
  digitalWrite(ENA,HIGH); digitalWrite(ENB,HIGH);
  digitalWrite(in1,LOW);  digitalWrite(in2,HIGH);
  digitalWrite(in3,LOW);  digitalWrite(in4,HIGH);
  Serial.println("Right");
  delay(250);
  _mStop();
}

// Ultrasonic distance measurement
int Distance_test() {   
  digitalWrite(Trig, LOW);   
  delayMicroseconds(2);
  digitalWrite(Trig, HIGH);  
  delayMicroseconds(20);
  digitalWrite(Trig, LOW);   
  float Fdistance = pulseIn(Echo, HIGH);  
  Fdistance= Fdistance/58;       
  return (int)Fdistance;
}

void setup() {
  Serial.begin(9600);

  // Set motor pins as output
  pinMode(in1,OUTPUT); pinMode(in2,OUTPUT);
  pinMode(in3,OUTPUT); pinMode(in4,OUTPUT);
  pinMode(ENA,OUTPUT); pinMode(ENB,OUTPUT);

  // Set ultrasonic pins
  pinMode(Echo, INPUT);    
  pinMode(Trig, OUTPUT);  

  // Attach servo to pin 3
  myservo.attach(3);

  // Connect to Wi-Fi
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Start web server
  server.begin();
}

void loop() {
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    client.flush();

    // Respond to movement commands
    if (request.indexOf("GET /forward") >= 0) _mForward();
    else if (request.indexOf("GET /back") >= 0) _mBack();
    else if (request.indexOf("GET /left") >= 0) _mLeft();
    else if (request.indexOf("GET /right") >= 0) _mRight();

    // Send response to browser in JSON format
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println();

    // Create JSON object for the response
    String jsonResponse = "{\"distance_measurements\": [";
    for (servoAngle = 10; servoAngle <= 180; servoAngle += 30) {
      myservo.write(servoAngle);  // Move servo to position
      delay(500);  // Allow servo to settle

      int distance = Distance_test();  // Measure distance
      jsonResponse += "{\"angle\": " + String(servoAngle) + ", \"distance\": " + String(distance) + "},";
      delay(500);
    }
    
    // Remove the last comma and close the JSON array
    jsonResponse = jsonResponse.substring(0, jsonResponse.length() - 1);
    jsonResponse += "]}";

    // Send the JSON response
    client.println(jsonResponse);
    delay(1);
    client.stop();
  }

  delay(1000);  // Delay to avoid spamming Serial Monitor
}
