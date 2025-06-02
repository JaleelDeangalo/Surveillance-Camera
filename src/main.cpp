#include <Arduino.h>
#include <esp_camera.h>
#include <iostream>
#include <sstream>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <WiFiClient.h> 
#include <ESPAsyncWebServer.h>

#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27

#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

const char* ssid = "CamServer";
const char* password = "Tech1212!";

AsyncWebServer server(80);
AsyncWebSocket wsCamera("/camera");
AsyncWebSocket wsServo("/servo");
Servo servo;

const int servoPin = 13; // Pin for the servo motor

uint32_t cameraClientId = 0;

void onCameraWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
 
  switch(type) {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        cameraClientId = client->id();
        break;
      case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        cameraClientId = 0;
        break;
      case WS_EVT_DATA:
        break;
      default:
        break;
    }
}

void onServoWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch(type) {
    case WS_EVT_CONNECT:
      Serial.printf("Servo WebSocket client #%u connected\n", client->id());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("Servo WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      if (data && len > 0) {
        String msg = "";
        for (size_t i = 0; i < len; i++) msg += (char)data[i];
        int angle = msg.toInt();
        angle = constrain(angle, 0, 180); // Servo safe range
        servo.write(angle);
        Serial.printf("Servo angle set to %d\n", angle);
      }
      break;
    default:
      break;
  }
}

void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_4;
  config.ledc_timer = LEDC_TIMER_2;
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }  

    sensor_t * s = esp_camera_sensor_get();
    s->set_vflip(s, 1); //flip it back
  if (psramFound()) {
    heap_caps_malloc_extmem_enable(20000);  
    Serial.printf("PSRAM initialized. malloc to take memory from psram above this size");    
  }  
}

void sendCameraPicture() {
  if (cameraClientId == 0) {
    return;
  }
  unsigned long  startTime1 = millis();
  //capture a frame
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
      Serial.println("Frame buffer could not be acquired");
      return;
  }

  unsigned long  startTime2 = millis();
  wsCamera.binary(cameraClientId, fb->buf, fb->len);
  esp_camera_fb_return(fb);
    
  //Wait for message to be delivered
  while (true) {
    AsyncWebSocketClient * clientPointer = wsCamera.client(cameraClientId);
    if (!clientPointer || !(clientPointer->queueIsFull())) {
      break;
    }
    delay(1);
  }
  
  unsigned long  startTime3 = millis();  
  Serial.printf("Time taken Total: %d|%d|%d\n",startTime3 - startTime1, startTime2 - startTime1, startTime3-startTime2 );
}


void setup() {
 
Serial.begin(115200);
  servo.attach(servoPin);
  servo.write(90); // Set initial position to 90 degrees
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("Camera Server");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.println("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  wsCamera.onEvent(onCameraWebSocketEvent);
  server.addHandler(&wsCamera); 

  wsServo.onEvent(onServoWebSocketEvent);
  server.addHandler(&wsServo);

  server.begin();

  setupCamera();
  
}

void loop() {
  wsCamera.cleanupClients();
  sendCameraPicture();
  delay(100);
}

