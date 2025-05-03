#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// Configuração dos pinos da câmara AI-Thinker ESP32-CAM
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27
#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

#define CAMERA_MODEL_AI_THINKER // Has PSRAM

// Credenciais WiFi
//const char* ssid = "iPhone de Ricardo";
//const char* password = "mariagata";

const char* ssid = "ZON-ZON_MEO";
const char* password ="4da883991d45zaQ239aZwpL";

void startCameraServer();

void setup() {
    Serial.begin(9600);
    Serial.setDebugOutput(true);
    Serial.println();

    // Conectar ao WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi conectado!");
    Serial.print("IP do ESP32-CAM: ");
    Serial.println(WiFi.localIP());

    // Configuração da câmara
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
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    
    if(psramFound()){
        config.frame_size = FRAMESIZE_UXGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Erro ao iniciar a câmara: 0x%x\n", err);
        return;
    }

    startCameraServer();
}

void loop() {
  if (Serial.available()) {
        char command = Serial.read();  // Lê o comando do serial monitor
        if (command == 'i') {  // Se pressionar 'i', pedirá o IP
            if (WiFi.status() == WL_CONNECTED) {
                Serial.print("IP do ESP32-CAM: ");
                Serial.println(WiFi.localIP().toString());
            } else {
                Serial.println("Não conectado ao WiFi");
            }
        }
    }
    delay(100);
}

// --- SERVIDOR HTTP PARA STREAMING ---

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;

    res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    if (res != ESP_OK) return res;

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Falha ao capturar frame");
            res = ESP_FAIL;
        } else {
            char buffer[64];
            int header_len = snprintf(buffer, sizeof(buffer), 
                                      "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", 
                                      fb->len);
            res = httpd_resp_send_chunk(req, buffer, header_len);
            if (res == ESP_OK) {
                res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
                httpd_resp_send_chunk(req, "\r\n", 2);
            }
            esp_camera_fb_return(fb);
        }
        if (res != ESP_OK) break;
    }

    return res;
}

void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_uri_t uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri);
        Serial.println("Servidor de vídeo iniciado!");
    } else {
        Serial.println("Erro ao iniciar o servidor!");
    }
}
