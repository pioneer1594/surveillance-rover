#include "esp_camera.h"
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>

// Disable Brownout Detector Header
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// --- PIN DEFINITIONS (FIXED PIN CONFLICTS) ---
#define MOTOR_IN1 13  // Right Motor Forward
#define MOTOR_IN2 14  // Right Motor Backward
#define MOTOR_IN3 15  // Left Motor Forward
#define MOTOR_IN4 16   // Left Motor Backward (Moved from GPIO 2 to GPIO 4)

#define LIGHT_PIN 4   // Flash LED Pin (Note: If using GPIO 4 for motor, change light pin or share safely)
#define SERVO_PIN 12  // Servo Pin 

#define STOP 0
#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4

// Camera Pin Mapping (AI-Thinker)
#define PWDN_GPIO_NUM      32
#define RESET_GPIO_NUM     -1
#define XCLK_GPIO_NUM       0
#define SIOD_GPIO_NUM      26
#define SIOC_GPIO_NUM      27
#define Y9_GPIO_NUM        35
#define Y8_GPIO_NUM        34
#define Y7_GPIO_NUM        39
#define Y6_GPIO_NUM        36
#define Y5_GPIO_NUM        21
#define Y4_GPIO_NUM        19
#define Y3_GPIO_NUM        18
#define Y2_GPIO_NUM         5
#define VSYNC_GPIO_NUM     25
#define HREF_GPIO_NUM      23
#define PCLK_GPIO_NUM      22

const char* ssid     = "pio";
const char* password = "15092004";

AsyncWebServer server(80);
AsyncWebSocket wsCamera("/Camera");
AsyncWebSocket wsCarInput("/CarInput");
uint32_t cameraClientId = 0;

Servo cameraServo;

// Safety Watchdog Variables
unsigned long lastCmdTime = 0;
const unsigned long CMD_TIMEOUT = 600; // after 0.6 sec stop

// ================= HTML, CSS, JS to store in PROGMEM =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <title>Smart Rover - Command Center</title>
    <style>
        :root {
            --primary-color: #3498db; 
            --danger-color: #e74c3c;  
            --bg-color: #121212;      
            --panel-bg: #1e1e1e;      
            --border-color: #2c3e50;  
        }
        body {
            font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
            background-color: var(--bg-color);
            color: #ffffff;
            text-align: center;
            margin: 0;
            padding: 20px;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }
        .container {
            width: 100%;
            max-width: 850px;
            background: var(--panel-bg);
            padding: 30px;
            border-radius: 12px;
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.4);
            border: 1px solid var(--border-color);
        }
        h2 { font-size: 26px; letter-spacing: 1px; margin-bottom: 10px; color: var(--primary-color); }
        p { color: #a0aabf; font-size: 14px; margin-bottom: 20px; }
        
        input[type="text"], input[type="password"] {
            width: 100%; max-width: 320px; padding: 14px 20px; margin: 10px 0;
            background: #2a2a2a; border: 1px solid #444; border-radius: 6px;
            color: #fff; font-size: 16px; outline: none; transition: 0.3s;
        }
        input[readonly] {
            background: #1a1a1a; color: #888; cursor: not-allowed; border-color: #333;
        }
        input:focus:not([readonly]) { border-color: var(--primary-color); background: #333; }
        .btn {
            background: var(--primary-color); color: #ffffff; border: none;
            padding: 12px 30px; font-size: 16px; font-weight: bold; border-radius: 6px;
            cursor: pointer; text-transform: uppercase; letter-spacing: 1px; margin-top: 15px;
        }
        .btn:hover { background: #2980b9; }

        #video-container {
            margin: 15px auto; width: 100%; max-width: 500px; height: auto; min-height: 280px;
            background-color: #000; border-radius: 8px; overflow: hidden; border: 1px solid var(--primary-color);
            display: flex; align-items: center; justify-content: center; position: relative;
        }
        #camera-status { color: var(--danger-color); font-weight: bold; letter-spacing: 1px; }
        #video-stream { width: 100%; height: auto; display: none; }
        
        .slider-group { margin: 15px auto; max-width: 300px; text-align: left; }
        .slider-group label { display: block; font-size: 14px; margin-bottom: 5px; color: #a0aabf; }
        input[type=range] { width: 100%; cursor: pointer; }
        .toggle-btn {
            background: var(--danger-color); color: white; border: none; padding: 10px 20px;
            font-size: 14px; font-weight: bold; border-radius: 6px; cursor: pointer; margin-bottom: 15px;
        }

        .control-panel {
            display: grid; grid-template-columns: repeat(3, 70px); grid-template-rows: repeat(3, 70px);
            gap: 12px; justify-content: center; align-items: center; margin: 10px auto;
        }
        .d-btn {
            width: 100%; height: 100%; background: #2c3e50; border: 1px solid #34495e;
            border-radius: 8px; color: #ecf0f1; font-size: 18px; font-weight: bold; cursor: pointer;
            display: flex; justify-content: center; align-items: center; user-select: none; -webkit-user-select: none;
        }
        .d-btn:active, .d-btn.active { background: var(--primary-color); color: #ffffff; transform: scale(0.95); }
        .btn-stop { background: #c0392b; border-color: #e74c3c; font-size: 14px; }
        .btn-stop:active, .btn-stop.active { background: #a1281b; }
        
        .logout-btn { background: transparent; border: 1px solid #7f8c8d; color: #bdc3c7; font-size: 12px; padding: 8px 16px; margin-top: 20px;}
        .error { color: var(--danger-color); font-size: 14px; margin-top: 15px; }
    </style>
</head>
<body>

    <div class="container">
        <div id="loginSection">
            <h2>SYSTEM LOGIN</h2>
            <p>Smart Rover Secure Authentication</p>
            <form id="loginForm">
                <input type="text" id="username" value="admin" readonly required><br>
                <input type="password" id="password" placeholder="Enter Password" required><br>
                <button type="submit" class="btn">CONNECT</button>
            </form>
            <div id="loginMessage" class="error"></div>
        </div>

        <div id="dashboardSection" style="display: none;">
            <h2>SMART ROVER HUD</h2>
            
            <div id="video-container">
                <span id="camera-status">CONNECTING CAMERA...</span>
                <img id="video-stream" src="" alt="Live Stream" />
            </div>

            <div class="slider-group">
                <label>Camera Tilt (Servo): <span id="servoVal">90</span>°</label>
                <input type="range" min="0" max="180" value="90" oninput="updateSlider('Servo', this.value, 'servoVal')">
            </div>
            <button class="toggle-btn" id="lightBtn" onclick="toggleLight()">Flash Light: OFF</button>

            <div class="control-panel">
                <div></div>
                <button class="d-btn" onmousedown="sendCommand('forward')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('forward')" ontouchend="sendCommand('stop')">▲</button>
                <div></div>
                <button class="d-btn" onmousedown="sendCommand('left')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('left')" ontouchend="sendCommand('stop')">◄</button>
                <button class="d-btn btn-stop" onmousedown="sendCommand('stop')" ontouchstart="sendCommand('stop')">■</button>
                <button class="d-btn" onmousedown="sendCommand('right')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('right')" ontouchend="sendCommand('stop')">►</button>
                <div></div>
                <button class="d-btn" onmousedown="sendCommand('backward')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('backward')" ontouchend="sendCommand('stop')">▼</button>
                <div></div>
            </div>
            
            <button class="btn logout-btn" onclick="logout()">DISCONNECT</button>
        </div>
    </div>

    <script>
        const loginSection = document.getElementById('loginSection');
        const dashboardSection = document.getElementById('dashboardSection');
        const loginForm = document.getElementById('loginForm');
        const loginMessage = document.getElementById('loginMessage');
        const videoStream = document.getElementById('video-stream');
        const cameraStatus = document.getElementById('camera-status');

        var wsCamUrl = 'ws://' + window.location.hostname + '/Camera';
        var wsCarUrl = 'ws://' + window.location.hostname + '/CarInput';
        var wsCam, wsCar;
        var lightState = 0;

        loginForm.addEventListener('submit', (e) => {
            e.preventDefault();
            const pass = document.getElementById('password').value;
            if(pass === '1234') {
                loginSuccess();
            } else {
                loginMessage.innerText = "ACCESS DENIED. Invalid Password.";
                document.getElementById('password').value = "";
            }
        });

        function loginSuccess() {
            loginSection.style.display = 'none';
            dashboardSection.style.display = 'block';
            loginMessage.innerText = "";
            initWebSockets();
        }

        function logout() {
            dashboardSection.style.display = 'none';
            loginSection.style.display = 'block';
            document.getElementById('password').value = ""; 
            if(wsCam) wsCam.close();
            if(wsCar) wsCar.close();
            cameraStatus.style.display = 'inline';
            videoStream.style.display = 'none';
        }

        function initWebSockets() {
            wsCam = new WebSocket(wsCamUrl);
            wsCar = new WebSocket(wsCarUrl);

            wsCam.binaryType = 'arraybuffer';
            wsCam.onmessage = function(event) {
                var blob = new Blob([event.data], {type: 'image/jpeg'});
                videoStream.src = URL.createObjectURL(blob);
                videoStream.style.display = 'block';
                cameraStatus.style.display = 'none';
            };
        }

        function sendCmd(key, value) {
            if (wsCar && wsCar.readyState === WebSocket.OPEN) {
                wsCar.send(key + ',' + value);
            }
        }

        function sendCommand(action) {
            if (action === 'forward') sendCmd('MoveCar', 1);
            else if (action === 'backward') sendCmd('MoveCar', 2);
            else if (action === 'left') sendCmd('MoveCar', 3);
            else if (action === 'right') sendCmd('MoveCar', 4);
            else if (action === 'stop') sendCmd('MoveCar', 0);
        }

        function updateSlider(key, value, labelId) {
            document.getElementById(labelId).innerText = value;
            sendCmd(key, value);
        }

        function toggleLight() {
            lightState = lightState === 0 ? 1 : 0;
            var btn = document.getElementById('lightBtn');
            if (lightState === 1) {
                btn.innerText = "Flash Light: ON";
                btn.style.background = "#2a9d8f";
            } else {
                btn.innerText = "Flash Light: OFF";
                btn.style.background = "#e74c3c";
            }
            sendCmd('Light', lightState);
        }

        window.addEventListener('keydown', (event) => {
            if (event.repeat || dashboardSection.style.display === 'none') return; 
            const key = event.key.toLowerCase();
            if (key === 'w') sendCommand('forward');
            if (key === 'a') sendCommand('left');
            if (key === 's') sendCommand('backward');
            if (key === 'd') sendCommand('right');
        });

        window.addEventListener('keyup', (event) => {
            if (dashboardSection.style.display === 'none') return;
            const key = event.key.toLowerCase();
            if (['w', 'a', 's', 'd'].includes(key)) {
                sendCommand('stop');
            }
        });
    </script>
</body>
</html>
)rawliteral";

// ================= MOTOR AND HARDWARE CONTROL =================

void rotateMotors(int in1, int in2, int in3, int in4) {
  digitalWrite(MOTOR_IN1, in1);
  digitalWrite(MOTOR_IN2, in2);
  digitalWrite(MOTOR_IN3, in3);
  digitalWrite(MOTOR_IN4, in4);
}

void moveCar(int inputValue) {
  switch(inputValue) {
    case UP:    rotateMotors(HIGH, LOW, LOW, HIGH); break;
    case DOWN:  rotateMotors(LOW, HIGH, HIGH, LOW); break;
    case RIGHT: rotateMotors(HIGH, LOW, HIGH, LOW); break;
    case LEFT:  rotateMotors(LOW, HIGH, LOW, HIGH); break;
    case STOP:
    default:    rotateMotors(LOW, LOW, LOW, LOW);   break;
  }
}

void onCarInputWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {                       
  if (type == WS_EVT_DISCONNECT) {
    moveCar(STOP);
    digitalWrite(LIGHT_PIN, LOW);  
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = '\0';
      char* key = strtok((char*)data, ",");
      char* valueStr = strtok(NULL, ",");

      if (key && valueStr) {
        int valueInt = atoi(valueStr);
        lastCmdTime = millis(); // Refresh Safety Timer

        if (strcmp(key, "MoveCar") == 0) {
          moveCar(valueInt);         
        } else if (strcmp(key, "Light") == 0) {
          digitalWrite(LIGHT_PIN, valueInt > 0 ? HIGH : LOW);          
        } else if (strcmp(key, "Servo") == 0) {
          cameraServo.write(valueInt); 
        }      
      }
    }
  }
}

void onCameraWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {                       
  if (type == WS_EVT_CONNECT) cameraClientId = client->id();
  else if (type == WS_EVT_DISCONNECT) cameraClientId = 0;
}

// Camera setup
void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 15;
  config.fb_count = 2;

  esp_camera_init(&config);
}

void sendCameraPicture() {
  if (cameraClientId == 0) return;
  AsyncWebSocketClient *client = wsCamera.client(cameraClientId);
  if (!client || client->queueIsFull()) return;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  wsCamera.binary(cameraClientId, fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void setUpPinModes() {
  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(MOTOR_IN3, OUTPUT);
  pinMode(MOTOR_IN4, OUTPUT);
  
  pinMode(LIGHT_PIN, OUTPUT);    
  digitalWrite(LIGHT_PIN, LOW);

  ESP32PWM::allocateTimer(2);
  cameraServo.setPeriodHertz(50);
  cameraServo.attach(SERVO_PIN, 540, 2400);
  cameraServo.write(90);

  moveCar(STOP);
}

void setup(void) {
  // Disable Brownout Detector to prevent boot crashes
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  setUpPinModes();

  // Configure Soft Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println("Wi-Fi Access Point Started!");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  wsCamera.onEvent(onCameraWebSocketEvent);
  server.addHandler(&wsCamera);

  wsCarInput.onEvent(onCarInputWebSocketEvent);
  server.addHandler(&wsCarInput);

  server.begin();
  setupCamera();
}

void loop() {
  wsCamera.cleanupClients(); 
  wsCarInput.cleanupClients(); 
  sendCameraPicture(); 

  if (millis() - lastCmdTime > CMD_TIMEOUT) {
    moveCar(STOP);
  }
}