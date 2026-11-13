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
int servoAngle = 90;  // Initial position of servo (используется старым сканом /scan)

// --- Голова Няни: серво как «шея» (TASK голова, 13.11.2026) ---
// Зачем: Няня поворачивается к лицу и к голосу. Серво двигается из loop() маленькими шагами, а HTTP-ответ уходит
// сразу, поэтому Няня не ждёт конца движения и не рвёт Wi-Fi соединение. Колёса эти команды не трогают.
const int HEAD_MIN = 10;               // дальше краёв серво упирается в корпус и гудит
const int HEAD_MAX = 170;
const int HEAD_HOME = 90;              // прямо
const int HEAD_STEP = 5;               // шаг не больше 5 градусов...
const unsigned long HEAD_STEP_MS = 15; // ...каждые 15 мс — плавно, без рывка по току
const unsigned long HEAD_IDLE_MS = 60000UL;  // 60 с без команд — плавно вернуться в 90
const int NOD_DEPTH = 10;              // кивок: +-10 градусов
const int NOD_MAX_STEPS = 5;           // вверх, вниз, вверх, вниз, назад в исходный угол
int headCur = HEAD_HOME;               // где серво сейчас
int headTarget = HEAD_HOME;            // куда плавно едет
int nodQueue[NOD_MAX_STEPS];           // очередь точек кивка
int nodLen = 0, nodPos = 0;
unsigned long headStepAt = 0;          // когда делали последний шаг
unsigned long headCmdAt = 0;           // когда в последний раз пришла команда головы (для возврата в 90)
unsigned long ipPrintAt = 0;

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

// ---- Голова: безопасные углы и плавное движение ----
int headClamp(int a) {
  if (a < HEAD_MIN) return HEAD_MIN;
  if (a > HEAD_MAX) return HEAD_MAX;
  return a;
}

// Один маленький шаг к цели; вызывается из loop() — ничего не блокирует.
void headTick() {
  unsigned long now = millis();
  if (now - headStepAt < HEAD_STEP_MS) return;
  headStepAt = now;
  if (headCur == headTarget) {
    if (nodPos < nodLen) headTarget = nodQueue[nodPos++];   // следующая точка кивка
    else if (now - headCmdAt > HEAD_IDLE_MS && headCur != HEAD_HOME) headTarget = HEAD_HOME;  // тишина — домой
    return;
  }
  int d = headTarget - headCur;
  if (d > HEAD_STEP) d = HEAD_STEP;
  if (d < -HEAD_STEP) d = -HEAD_STEP;
  headCur += d;
  myservo.write(headCur);
}

// Повтор того же угла серво не дёргает: цель не меняется, движения нет.
bool headLook(int angle) {
  headCmdAt = millis();
  nodLen = nodPos = 0;                  // новая цель отменяет кивок
  angle = headClamp(angle);
  if (angle == headTarget) return false;
  headTarget = angle;
  return true;
}

// Кивок вокруг текущего угла; пока кивает, повторный /nod игнорируется.
bool headNod() {
  headCmdAt = millis();
  if (nodPos < nodLen || headCur != headTarget) return false;
  int base = headClamp(headCur);
  nodQueue[0] = headClamp(base + NOD_DEPTH);
  nodQueue[1] = headClamp(base - NOD_DEPTH);
  nodQueue[2] = headClamp(base + NOD_DEPTH);
  nodQueue[3] = headClamp(base - NOD_DEPTH);
  nodQueue[4] = base;
  nodLen = NOD_MAX_STEPS;
  nodPos = 0;
  return true;
}

void sendJson(WiFiClient &client, int code, const String &body) {
  client.print("HTTP/1.1 ");
  client.print(code);
  client.println(code == 200 ? " OK" : " Bad Request");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();
  client.println(body);
}

String headStatusJson() {
  return "{\"angle\": " + String(headCur) + ", \"target\": " + String(headTarget) +
         ", \"ok\": true, \"uptime\": " + String(millis() / 1000) + "}";
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
  myservo.write(HEAD_HOME);  // голова стартует прямо
  headCmdAt = millis();

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
  headTick();  // серво едет к цели всегда, даже когда нет клиентов

  if (millis() - ipPrintAt > 10000UL) {   // раньше печаталось каждую секунду и тормозило loop
    ipPrintAt = millis();
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }

  WiFiClient client = server.available();
  if (!client) return;
  client.setTimeout(200);  // молчащий клиент не должен вешать голову на секунду
  String request = client.readStringUntil('\r');
  client.flush();

  // --- Голова: ответ сразу, моторы колёс не трогаем ---
  if (request.indexOf("GET /look") >= 0) {
    int p = request.indexOf("angle=");
    int digit = (p >= 0) ? request.charAt(p + 6) : 0;
    if (p < 0 || digit < '0' || digit > '9') {   // нет угла или не число — не угадываем
      sendJson(client, 400, "{\"ok\": false, \"error\": \"angle=10..170 required\"}");
    } else {
      int req = request.substring(p + 6).toInt();
      bool moved = headLook(req);
      sendJson(client, 200, "{\"ok\": true, \"moved\": " + String(moved ? "true" : "false") +
               ", \"target\": " + String(headTarget) + ", \"angle\": " + String(headCur) + "}");
    }
    delay(1);
    client.stop();
    return;
  }
  if (request.indexOf("GET /nod") >= 0) {
    bool started = headNod();
    sendJson(client, 200, "{\"ok\": true, \"started\": " + String(started ? "true" : "false") + "}");
    delay(1);
    client.stop();
    return;
  }
  if (request.indexOf("GET /status") >= 0) {
    sendJson(client, 200, headStatusJson());
    delay(1);
    client.stop();
    return;
  }

  // Respond to movement commands
  if (request.indexOf("GET /forward") >= 0) _mForward();
  else if (request.indexOf("GET /back") >= 0) _mBack();
  else if (request.indexOf("GET /left") >= 0) _mLeft();
  else if (request.indexOf("GET /right") >= 0) _mRight();

  // Send response to browser in JSON format
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println();

  // Старый скан дальномером (долгий, блокирующий: ~9 с). Серво уходит до 180 — это единственное место вне 10..170.
  String jsonResponse = "{\"distance_measurements\": [";
  for (servoAngle = 10; servoAngle <= 180; servoAngle += 30) {
    myservo.write(servoAngle);  // Move servo to position
    delay(500);  // Allow servo to settle

    int distance = Distance_test();  // Measure distance
    jsonResponse += "{\"angle\": " + String(servoAngle) + ", \"distance\": " + String(distance) + "},";
    delay(500);
  }
  // После скана серво физически в последней точке: запоминаем и плавно возвращаем голову домой.
  headCur = servoAngle - 30;  // последняя измеренная точка (цикл выходит на шаг дальше)
  headTarget = HEAD_HOME;
  headCmdAt = millis();
  nodLen = nodPos = 0;

  // Remove the last comma and close the JSON array
  jsonResponse = jsonResponse.substring(0, jsonResponse.length() - 1);
  jsonResponse += "]}";

  // Send the JSON response
  client.println(jsonResponse);
  delay(1);
  client.stop();
}
