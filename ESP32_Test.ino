#include "esp_camera.h"
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>

const char* LOGIN_USERNAME = "admin";
const char* LOGIN_PASSWORD = "Rover1234";
const char* WIFI_SSID = "Pio";
const char* WIFI_PASSWORD = "15092004";

#define MOTOR_IN1 14
#define MOTOR_IN2 15
#define MOTOR_IN3 2
#define MOTOR_IN4 4
#define LIGHT_PIN 4
#define SERVO_PIN 13

#define STOP 0
#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

AsyncWebServer server(80);
AsyncWebSocket wsCamera("/Camera");
AsyncWebSocket wsCarInput("/CarInput");
uint32_t cameraClientId = 0;
Servo cameraServo;
unsigned long lastCmdTime = 0;
const unsigned long CMD_TIMEOUT = 600;
bool cameraReady = false;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>Smart Rover</title><style>
:root { --primary-color: #3498db; --danger-color: #e74c3c; --success-color: #2a9d8f; --bg-color: #121212; --panel-bg: #1e1e1e; --border-color: #2c3e50; }
* { box-sizing: border-box; }
body { margin: 0; padding: 20px; font-family: 'Segoe UI', Arial, sans-serif; background: var(--bg-color); color: white; text-align: center; }
.container { width: 100%; max-width: 850px; margin: auto; background: var(--panel-bg); padding: 30px; border-radius: 12px; border: 1px solid var(--border-color); }
h2 { color: var(--primary-color); letter-spacing: 1px; }
p { color: #a0aabf; }
input[type="text"], input[type="password"] { width: 100%; max-width: 320px; padding: 14px 20px; margin: 10px 0; background: #2a2a2a; border: 1px solid #444; border-radius: 6px; color: white; font-size: 16px; }
.btn { background: var(--primary-color); color: white; border: none; padding: 12px 30px; font-size: 16px; font-weight: bold; border-radius: 6px; cursor: pointer; margin-top: 15px; }
.btn:hover { background: #2980b9; }
.error { color: var(--danger-color); margin-top: 15px; font-size: 14px; }
#video-container { margin: 15px auto; width: 100%; max-width: 500px; min-height: 280px; background: black; border-radius: 8px; border: 1px solid var(--primary-color); display: flex; justify-content: center; align-items: center; }
#camera-status { color: var(--danger-color); font-weight: bold; }
#video-stream { width: 100%; height: auto; display: none; }
.slider-group { margin: 20px auto; max-width: 300px; text-align: left; }
.slider-group label { display: block; margin-bottom: 8px; color: #a0aabf; }
input[type=range] { width: 100%; }
.toggle-btn { background: var(--danger-color); border: none; color: white; padding: 10px 20px; font-size: 14px; font-weight: bold; border-radius: 6px; cursor: pointer; }
.control-panel { display: grid; grid-template-columns: repeat(3, 70px); grid-template-rows: repeat(3, 70px); gap: 12px; justify-content: center; align-items: center; margin: 20px auto; }
.d-btn { width: 70px; height: 70px; background: #2c3e50; border: 1px solid #34495e; border-radius: 8px; color: white; font-size: 22px; font-weight: bold; cursor: pointer; user-select: none; }
.d-btn:active { background: var(--primary-color); transform: scale(0.95); }
.btn-stop { background: #c0392b; }
.status { margin-top: 20px; padding: 10px; border-radius: 6px; background: #181818; color: #aaa; font-size: 13px; }
.status.ok { color: #2ecc71; }
.status.error { color: #e74c3c; }
</style></head><body>
<div class="container">
  <div id="loginSection">
    <h2>SYSTEM LOGIN</h2>
    <p>Smart Rover Secure Authentication</p>
    <form id="loginForm">
      <input type="text" id="username" placeholder="Username" required><br>
      <input type="password" id="password" placeholder="Password" required><br>
      <button type="submit" class="btn">CONNECT</button>
    </form>
    <div id="loginMessage" class="error"></div>
  </div>
  <div id="dashboardSection" style="display:none;">
    <h2>SMART ROVER HUD</h2>
    <div id="video-container">
      <span id="camera-status">CONNECTING CAMERA...</span>
      <img id="video-stream" alt="Live Camera">
    </div>
    <div class="slider-group">
      <label>Camera Tilt: <span id="servoVal">90</span>°</label>
      <input id="servoSlider" type="range" min="0" max="180" value="90">
    </div>
    <button class="toggle-btn" id="lightBtn">Flash Light: OFF</button>
    <div class="control-panel">
      <div></div>
      <button class="d-btn" onmousedown="sendCommand('forward')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('forward')" ontouchend="sendCommand('stop')">▲</button>
      <div></div>
      <button class="d-btn" onmousedown="sendCommand('left')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('left')" ontouchend="sendCommand('stop')">◀</button>
      <button class="d-btn btn-stop" onclick="sendCommand('stop')">■</button>
      <button class="d-btn" onmousedown="sendCommand('right')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('right')" ontouchend="sendCommand('stop')">▶</button>
      <div></div>
      <button class="d-btn" onmousedown="sendCommand('backward')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('backward')" ontouchend="sendCommand('stop')">▼</button>
      <div></div>
    </div>
    <div id="systemStatus" class="status">Connecting...</div>
  </div>
</div>
<script>
const loginSection = document.getElementById("loginSection"), dashboardSection = document.getElementById("dashboardSection");
const loginForm = document.getElementById("loginForm"), loginMessage = document.getElementById("loginMessage");
const videoStream = document.getElementById("video-stream"), cameraStatus = document.getElementById("camera-status");
const systemStatus = document.getElementById("systemStatus"), servoSlider = document.getElementById("servoSlider");
const servoVal = document.getElementById("servoVal"), lightBtn = document.getElementById("lightBtn");
let wsCam = null, wsCar = null, lightState = 0;

loginForm.addEventListener("submit", function(e) {
  e.preventDefault();
  if (document.getElementById("username").value === "admin" && document.getElementById("password").value === "Rover1234") {
    loginMessage.innerText = ""; loginSection.style.display = "none"; dashboardSection.style.display = "block";
    systemStatus.innerText = "Connecting to rover..."; initWebSockets();
  } else { loginMessage.innerText = "ACCESS DENIED - Invalid username or password."; }
});

function initWebSockets() {
  const host = window.location.hostname;
  wsCam = new WebSocket("ws://" + host + "/Camera");
  wsCam.binaryType = "arraybuffer";
  wsCam.onopen = function() { cameraStatus.innerText = "CAMERA CONNECTED"; systemStatus.innerText = "System connected"; systemStatus.className = "status ok"; };
  wsCam.onmessage = function(e) {
    videoStream.src = URL.createObjectURL(new Blob([e.data], { type: "image/jpeg" }));
    videoStream.style.display = "block"; cameraStatus.style.display = "none";
  };
  wsCam.onerror = function() { cameraStatus.innerText = "CAMERA CONNECTION ERROR"; };
  wsCam.onclose = function() { videoStream.style.display = "none"; cameraStatus.style.display = "inline"; cameraStatus.innerText = "CAMERA DISCONNECTED"; };

  wsCar = new WebSocket("ws://" + host + "/CarInput");
  wsCar.onopen = function() { systemStatus.innerText = "Rover control connected"; systemStatus.className = "status ok"; };
  wsCar.onerror = function() { systemStatus.innerText = "Rover control connection error"; systemStatus.className = "status error"; };
  wsCar.onclose = function() { systemStatus.innerText = "Rover control disconnected"; systemStatus.className = "status error"; };
}

function sendCmd(key, value) {
  if (wsCar && wsCar.readyState === WebSocket.OPEN) wsCar.send(key + "," + value);
}

function sendCommand(action) {
  let val = 0;
  if (action === "forward") val = 1;
  else if (action === "backward") val = 2;
  else if (action === "left") val = 3;
  else if (action === "right") val = 4;
  sendCmd("MoveCar", val);
}

servoSlider.addEventListener("input", function() {
  servoVal.innerText = servoSlider.value;
  sendCmd("Servo", servoSlider.value);
});

lightBtn.addEventListener("click", function() {
  lightState = lightState === 0 ? 1 : 0;
  lightBtn.innerText = lightState === 1 ? "Flash Light: ON" : "Flash Light: OFF";
  lightBtn.style.background = lightState === 1 ? "#2a9d8f" : "#e74c3c";
  sendCmd("Light", lightState);
});

document.addEventListener("keydown", function(e) {
  if (e.repeat || dashboardSection.style.display === "none") return;
  const k = e.key.toLowerCase();
  if (k === "w") sendCommand("forward"); else if (k === "s") sendCommand("backward");
  else if (k === "a") sendCommand("left"); else if (k === "d") sendCommand("right");
});

document.addEventListener("keyup", function(e) {
  if (["w", "s", "a", "d"].includes(e.key.toLowerCase())) sendCommand("stop");
});
</script></body></html>
)rawliteral";

void rotateMotors(int in1, int in2, int in3, int in4) {
  digitalWrite(MOTOR_IN1, in1); digitalWrite(MOTOR_IN2, in2);
  digitalWrite(MOTOR_IN3, in3); digitalWrite(MOTOR_IN4, in4);
}

void moveCar(int command) {
  switch(command) {
    case UP:    rotateMotors(HIGH, LOW, LOW, HIGH); Serial.println("MOVE: FORWARD"); break;
    case DOWN:  rotateMotors(LOW, HIGH, HIGH, LOW); Serial.println("MOVE: BACKWARD"); break;
    case LEFT:  rotateMotors(LOW, HIGH, LOW, HIGH); Serial.println("MOVE: LEFT"); break;
    case RIGHT: rotateMotors(HIGH, LOW, HIGH, LOW); Serial.println("MOVE: RIGHT"); break;
    default:    rotateMotors(LOW, LOW, LOW, LOW);   Serial.println("MOVE: STOP"); break;
  }
}

void onCarInputWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) { Serial.printf("Car client connected: %u\n", client->id()); return; }
  if (type == WS_EVT_DISCONNECT) { Serial.println("Car client disconnected"); moveCar(STOP); digitalWrite(LIGHT_PIN, LOW); return; }
  if (type != WS_EVT_DATA) return;

  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT || len >= 64) return;

  char message[64];
  memcpy(message, data, len);
  message[len] = '\0';

  char *key = strtok(message, ",");
  char *valueStr = strtok(NULL, ",");
  if (!key || !valueStr) return;

  int valueInt = atoi(valueStr);
  lastCmdTime = millis();

  if (strcmp(key, "MoveCar") == 0) moveCar(valueInt);
  else if (strcmp(key, "Light") == 0) digitalWrite(LIGHT_PIN, valueInt > 0 ? HIGH : LOW);
  else if (strcmp(key, "Servo") == 0) {
    cameraServo.write(constrain(valueInt, 0, 180));
    Serial.printf("SERVO: %d\n", valueInt);
  }
}

void onCameraWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) { cameraClientId = client->id(); Serial.printf("Camera client connected: %u\n", cameraClientId); }
  else if (type == WS_EVT_DISCONNECT && cameraClientId == client->id()) { cameraClientId = 0; Serial.println("Camera client disconnected"); }
}

bool setupCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0; config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM; config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
#if ESP_IDF_VERSION_MAJOR >= 5
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM;
#else
  config.pin_sscb_sda = SIOD_GPIO_NUM; config.pin_sscb_scl = SIOC_GPIO_NUM;
#endif
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) { config.frame_size = FRAMESIZE_QVGA; config.jpeg_quality = 12; config.fb_count = 2; }
  else { config.frame_size = FRAMESIZE_QQVGA; config.jpeg_quality = 15; config.fb_count = 1; }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) { Serial.printf("Camera init FAILED: 0x%x\n", err); return false; }
  cameraReady = true;
  return true;
}

void sendCameraPicture() {
  if (!cameraReady || cameraClientId == 0) return;
  AsyncWebSocketClient *client = wsCamera.client(cameraClientId);
  if (!client || client->queueIsFull()) return;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  wsCamera.binary(cameraClientId, fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void setupPins() {
  pinMode(MOTOR_IN1, OUTPUT); pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_IN3, OUTPUT); pinMode(MOTOR_IN4, OUTPUT);
  moveCar(STOP);

  pinMode(LIGHT_PIN, OUTPUT); digitalWrite(LIGHT_PIN, LOW);

  ESP32PWM::allocateTimer(2);
  cameraServo.setPeriodHertz(50);
  if (cameraServo.attach(SERVO_PIN, 540, 2400)) { cameraServo.write(90); }
}

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("       SMART ROVER BOOTING");
  Serial.println("================================");

  // ---------------- PIN SETUP ----------------
  setupPins();

  Serial.println("PIN SETUP: OK");

  // ---------------- CAMERA ----------------
  Serial.println("Starting camera...");

  if (!setupCamera()) {
    Serial.println("ERROR: CAMERA FAILED");
    Serial.println("WiFi will NOT start because camera failed.");
    return;
  }

  Serial.println("CAMERA: OK");

  // ---------------- WIFI MODE ----------------
  Serial.println("Setting WiFi AP mode...");

  if (!WiFi.mode(WIFI_AP)) {
    Serial.println("ERROR: WiFi.mode(WIFI_AP) FAILED");
    return;
  }

  Serial.println("WiFi AP mode: OK");

  // ---------------- WIFI AP ----------------
  Serial.println("Starting WiFi Access Point...");

  if (!WiFi.softAP(WIFI_SSID, WIFI_PASSWORD)) {
    Serial.println("ERROR: WiFi.softAP() FAILED");
    return;
  }

  Serial.println("WiFi AP: OK");

  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);

  Serial.print("Password: ");
  Serial.println(WIFI_PASSWORD);

  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  // ---------------- WEB SERVER ----------------

  server.on("/", HTTP_GET,
    [](AsyncWebServerRequest *request) {
      request->send_P(
        200,
        "text/html",
        index_html
      );
    }
  );

  // Camera WebSocket
  wsCamera.onEvent(onCameraWebSocketEvent);
  server.addHandler(&wsCamera);

  // Motor WebSocket
  wsCarInput.onEvent(onCarInputWebSocketEvent);
  server.addHandler(&wsCarInput);

  server.begin();

  lastCmdTime = millis();

  Serial.println("WEB SERVER: OK");
  Serial.println("SYSTEM READY");
}

void loop() {
  wsCamera.cleanupClients();
  wsCarInput.cleanupClients();

  static unsigned long lastFrameTime = 0;
  if (millis() - lastFrameTime >= 100) {
    sendCameraPicture();
    lastFrameTime = millis();
  }

  if (millis() - lastCmdTime > CMD_TIMEOUT) {
    moveCar(STOP);
  }

  delay(1);
}


