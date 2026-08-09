#include "esp_camera.h"
#include <WiFi.h>

// Compiler fix to prevent HTTP macro overlaps from WiFi.h
#ifdef HTTP_GET
#undef HTTP_GET
#endif
#ifdef HTTP_POST
#undef HTTP_POST
#endif

#include "esp_http_server.h"
#include "soc/soc.h"             // Brownout Detector control
#include "soc/rtc_cntl_reg.h"    // Brownout Register control

// 🚨 Hotspot Settings: ဖုန်းကနေ ချိတ်ဆက်ရမယ့် Wi-Fi နာမည်နှင့် စကားဝှက်
const char* ap_ssid = "shein_cam";
const char* ap_password = "15092004"; // အနည်းဆုံး ဂဏန်း ၈ လုံး ရှိရပါမည်

// Camera Pins mapped directly (AI-Thinker Board Layout)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

httpd_handle_t camera_httpd = NULL;

const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Pioneer Hotspot Camera Monitor</title>
  <style>
    body { 
      font-family: Arial, sans-serif; 
      text-align: center; 
      background-color: #0f172a; 
      color: #f1f5f9; 
      margin: 0; 
      padding: 20px; 
    }
    h2 { color: #38bdf8; margin-bottom: 5px; }
    .status-badge {
      display: inline-block;
      padding: 5px 12px;
      background-color: #38bdf8;
      color: #0f172a;
      border-radius: 15px;
      font-size: 12px;
      font-weight: bold;
      margin-bottom: 20px;
    }
    .stream-container { 
      max-width: 480px; 
      margin: auto; 
      border: 4px solid #38bdf8; 
      border-radius: 12px; 
      overflow: hidden; 
      background-color: #000; 
      box-shadow: 0 8px 24px rgba(0,0,0,0.5); 
    }
    img { width: 100%; height: auto; display: block; }
    .note { margin-top: 15px; font-size: 13px; color: #94a3b8; }
  </style>
</head>
<body>
  <h2>Pioneer Hotspot Monitor</h2>
  <span class="status-badge">DIRECT AP MODE</span>
  <div class="stream-container">
    <img src="/stream" id="video">
  </div>
  <p class="note">Direct Wifi connection - Secure DRAM Only Stream</p>
</body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char part_buf[64];
  
  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=123456789000000000000987654321");
  if(res != ESP_OK) return res;

  while(true){
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
      break;
    }
    
    _jpg_buf_len = fb->len;
    _jpg_buf = fb->buf;

    size_t hlen = snprintf(part_buf, 64, "\r\n--123456789000000000000987654321\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
    res = httpd_resp_send_chunk(req, part_buf, hlen);
    if(res == ESP_OK){
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    esp_camera_fb_return(fb);
    if(res != ESP_OK) break;
    
    taskYIELD(); 
  }
  return res;
}

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_uri_t index_uri;
    index_uri.uri       = "/";
    index_uri.method    = HTTP_GET;
    index_uri.handler   = index_handler;
    index_uri.user_ctx  = NULL;

    httpd_uri_t stream_uri;
    stream_uri.uri      = "/stream";
    stream_uri.method   = HTTP_GET;
    stream_uri.handler  = stream_handler;
    stream_uri.user_ctx = NULL;
    
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &stream_uri);
    Serial.println("Camera Web Server Started successfully!");
  }
}

void setup() {
  // *** [CRITICAL] Disable Brownout Detector ***
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- Starting AP Hotspot DRAM-Only Test Boot ---");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;   config.pin_d1 = Y3_GPIO_NUM;   config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;   config.pin_d4 = Y6_GPIO_NUM;   config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;   config.pin_d7 = Y9_GPIO_NUM;   config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // 🚨 Strict DRAM Overrides (PSRAM ရှောင်ကွင်းရန်)
  config.frame_size = FRAMESIZE_QVGA;     // Safe lightweight resolution for DRAM
  config.fb_location = CAMERA_FB_IN_DRAM;  // Force frame buffer allocation in internal memory
  config.jpeg_quality = 14;               // Compress image size to fit DRAM
  config.fb_count = 1;

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera initialization failed! Error: 0x%x\n", err);
    return;
  }
  Serial.println("✅ Success: Camera successfully connected via Internal DRAM!");

  // 🚨 Creating Hotspot (Access Point Mode)
  Serial.print("Launching Access Point: ");
  Serial.println(ap_ssid);
  
  WiFi.softAP(ap_ssid, ap_password);
  
  Serial.println("Hotspot is active!");
  Serial.print("Connect to Wi-Fi: ");
  Serial.println(ap_ssid);
  
  // Launch Server
  startCameraServer();

  // Hotspot IP Default is 192.168.4.1
  Serial.print("Camera Ready! Access: http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  delay(10000);
}
