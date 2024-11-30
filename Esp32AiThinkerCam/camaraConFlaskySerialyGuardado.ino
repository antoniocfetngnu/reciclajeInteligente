#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi Credentials
const char* ssid = "Mpa";
const char* password = "pablo123";
const char* serverUrl = "http://192.168.43.57:4001/predict_esp32";
const char* serverUrl2 = "http://192.168.43.57:4001/Nivel/";
const char* serverUrl3 = "http://192.168.43.57:4001/Estado/";

// ESP32-CAM Pin Configuration
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

void setup() {
  Serial.begin(115200);

  // Camera configuration
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

  config.frame_size = FRAMESIZE_VGA;  // 640x480
  config.jpeg_quality = 10;           // 0-63 lower number means higher quality
  config.fb_count = 1;

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.println("ERROR");  // Send error to Arduino
    return;
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("READY");  // Tell Arduino we're ready
}

void sendImageToPrediction() {
  HTTPClient http;
  String fullUrl = String(serverUrl3) + String(1);
  http.begin(fullUrl);

  // Envía la solicitud GET
  int httpResponseCodePreliminar = http.GET();

  // Procesa la respuesta
  if (httpResponseCodePreliminar > 0) {
    String response = http.getString();
  }

  http.end();
  delay(100);
  // Primer buffer que se descarta
  camera_fb_t* fb = nullptr;
  while (fb == nullptr) {
    fb = esp_camera_fb_get();
    delay(100);  // Ajusta si es necesario
  }
  if (!fb) {
    Serial.println("ERROR");  // Envía error al Arduino
    return;
  }
  esp_camera_fb_return(fb);  // Libera el primer buffer

  // Segundo buffer que se envía
  fb = nullptr;
  while (fb == nullptr) {
    fb = esp_camera_fb_get();
    delay(100);  // Ajusta si es necesario
  }
  if (!fb) {
    Serial.println("ERROR");  // Envía error al Arduino
    return;
  }

  // Debugging frame size
  Serial.print("Frame size: ");
  Serial.println(fb->len);

  // Prepara la solicitud HTTP
  // HTTP http;
  http.begin(serverUrl);
  // Establece el tipo de contenido para la carga de archivos
  http.addHeader("Content-Type", "image/jpeg");

  // Envía la solicitud POST con la imagen
  int httpResponseCode = http.POST(fb->buf, fb->len);

  // Libera el buffer de frame
  esp_camera_fb_return(fb);

  // Procesa la respuesta
  if (httpResponseCode > 0) {
    String response = http.getString();
    // Analiza la respuesta JSON
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, response);
    if (!error) {
      const char* prediction = doc["prediction"];
      Serial.println(prediction);  // Envía la predicción al Arduino
    } else {
      Serial.println("JSON Error");
    }
  } else {
    Serial.println("HTTP Error");
  }
  http.end();

  // Retardo para asegurar frames nuevos en la siguiente captura
  delay(500);  // Ajusta según tu aplicación
}

void sendLevelToPrediction(int level) {
  // Prepara la solicitud HTTP
  HTTPClient http;
  String fullUrl = String(serverUrl2) + String(level);
  http.begin(fullUrl);

  // Envía la solicitud GET
  int httpResponseCode = http.GET();

  // Procesa la respuesta
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Nivel Actualizado en Monitoreo");  // Envía la respuesta al Arduino
  } else {
    Serial.println("HTTP Error");
  }
  http.end();

  // Retardo para evitar solicitudes frecuentes
  delay(500);
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "true") {
      sendImageToPrediction();
    } else if (command == "llenotrue") {
      sendLevelToPrediction(3);
    } else if (command == "llenofalse") {
      sendLevelToPrediction(2);
    } else if (command == "mitadtrue") {
      sendLevelToPrediction(2);
    } else if (command == "mitadfalse") {
      sendLevelToPrediction(1);
    }
  }
}