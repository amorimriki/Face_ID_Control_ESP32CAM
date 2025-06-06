/*
ESP32_VideoStreem.cpp
*/

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "index_html.h"

const char *ssid = "ZON-ZON_MEO";
const char *password = "4da883991d45zaQ239aZwpL";

// const char* ssid = "iPhone de Ricardo";
// const char* password = "mariagata";

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

#define LED_GPIO_PIN 4 // Pino do LED embutido

const int LED_VERDE = 12;
const int LED_AMARELO = 13;
const int LED_VERMELHO = 15;

// =================== CONFIG DA CÂMERA ===================
camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sccb_sda = SIOD_GPIO_NUM,
    .pin_sccb_scl = SIOC_GPIO_NUM,

    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,

    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_RGB565, // A Cor para PIXFORMAT_RGB565, PIXFORMAT_GRAYSCALE,
    .frame_size = FRAMESIZE_HVGA,
    .fb_count = 2,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY};

volatile bool streaming = true; // Flag para controlar o streaming

static esp_err_t stream_handler(httpd_req_t *req)
{
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
  if (res != ESP_OK)
    return res;

  while (streaming)
  { // Verifica a flag de streaming
    fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("Erro ao obter frame");
      continue;
    }

    // Converte o frame para JPEG
    size_t jpg_buf_len = 0;
    uint8_t *jpg_buf = NULL;
    bool converted = frame2jpg(fb, 24, &jpg_buf, &jpg_buf_len);
    esp_camera_fb_return(fb);

    if (!converted)
    {
      Serial.println("Erro ao converter para JPEG");
      continue;
    }

    // Envia o JPEG como resposta
    char part[64];
    snprintf(part, sizeof(part), "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpg_buf_len);
    res = httpd_resp_send_chunk(req, part, strlen(part));
    if (res == ESP_OK)
    {
      res = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_buf_len);
    }
    res = httpd_resp_send_chunk(req, "\r\n", 2);

    free(jpg_buf);

    if (res != ESP_OK)
      break;
    delay(5); // Ajuste fino de fluidez
  }

  return res;
}

// =================== HANDLER PARA LED ===================
static esp_err_t led_handler(httpd_req_t *req)
{
  char query[64];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
  {
    char param[8];
    if (httpd_query_key_value(query, "on", param, sizeof(param)) == ESP_OK)
    {
      int estado = atoi(param);
      digitalWrite(LED_GPIO_PIN, estado ? HIGH : LOW);

      // Interrompe o stream por um momento quando o LED for alterado
      streaming = false;
      delay(100); // Aguarda um pequeno tempo para garantir a interrupção do stream

      // Resposta ao cliente
      const char *ledStatus = estado ? "LED Ligado" : "LED Desligado";
      httpd_resp_sendstr(req, ledStatus);

      // Reativa o streaming após a mudança do LED
      delay(200); // Aguarda a resposta ser enviada antes de reiniciar o streaming
      streaming = true;

      return ESP_OK;
    }

    if (httpd_query_key_value(query, "status", param, sizeof(param)) == ESP_OK)
    {
      int estado = digitalRead(LED_GPIO_PIN);
      return httpd_resp_sendstr(req, estado ? "1" : "0");
    }
  }
  return httpd_resp_sendstr(req, "Parâmetro inválido");
}

// =================== HANDLER PARA A PÁGINA PRINCIPAL ===================
static esp_err_t index_handler(httpd_req_t *req)
{
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, MAIN_page, strlen(MAIN_page));
}

// =================== HANDLER PARA O SEMÁFORO ===================
static esp_err_t semaforo_handler(httpd_req_t *req)
{
  char query[32];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
  {
    char cor[12];

    if (httpd_query_key_value(query, "cor", cor, sizeof(cor)) == ESP_OK)
    {
      // Interrompe o stream por um momento quando o LED for alterado
      streaming = false;
      delay(100); // Aguarda um pequeno tempo para garantir a interrupção do stream
      // Desliga todos antes
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_AMARELO, LOW);
      digitalWrite(LED_VERMELHO, LOW);

      String corStr = String(cor);
      esp_err_t resp;
      if (corStr == "verde")
      {
        digitalWrite(LED_VERDE, HIGH);
        delay(1000);
        digitalWrite(LED_VERDE, LOW);
        delay(50);
        resp = httpd_resp_sendstr(req, "Verde ligado");
      }
      else if (corStr == "amarelo")
      {
        digitalWrite(LED_AMARELO, HIGH);
        delay(2000);
        digitalWrite(LED_AMARELO, LOW);
        delay(50);
        resp = httpd_resp_sendstr(req, "Amarelo ligado");
      }
      else if (corStr == "vermelho")
      {
        digitalWrite(LED_VERMELHO, HIGH);
        delay(1000);
        digitalWrite(LED_VERMELHO, LOW);
        delay(50);
        resp = httpd_resp_sendstr(req, "Vermelho ligado");
      }
      else
      {
        resp = httpd_resp_sendstr(req, "Cor inválida");
      }
      streaming = true;
    }
  }

  return httpd_resp_sendstr(req, "Parâmetro ausente");
}

void startCameraServer()
{
  httpd_handle_t server = NULL; // <-- mover isto para o topo

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  // Registro da URI para a página principal (index)
  httpd_uri_t index_uri = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = index_handler,
      .user_ctx = NULL};

  // Registro da URI para o stream de vídeo
  httpd_uri_t stream_uri = {
      .uri = "/stream",
      .method = HTTP_GET,
      .handler = stream_handler,
      .user_ctx = NULL};

  // Handler para controle do LED
  httpd_uri_t led_uri = {
      .uri = "/led",
      .method = HTTP_GET,
      .handler = led_handler,
      .user_ctx = NULL};

  // Handler para semáforo
  httpd_uri_t semaforo_uri = {
      .uri = "/semaforo",
      .method = HTTP_GET,
      .handler = semaforo_handler,
      .user_ctx = NULL};

  if (httpd_start(&server, &config) == ESP_OK)
  {
    httpd_register_uri_handler(server, &led_uri);
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &stream_uri);
    httpd_register_uri_handler(server, &semaforo_uri);
    Serial.println("Servidor de vídeo ativo!");
  }
  else
  {
    Serial.println("Erro ao iniciar servidor HTTP");
  }
}

void connectToWiFi()
{
  WiFi.begin(ssid, password);
  Serial.print("Conectando");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  pinMode(LED_GPIO_PIN, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  digitalWrite(LED_GPIO_PIN, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARELO, LOW);
  digitalWrite(LED_VERMELHO, LOW);
  connectToWiFi();

  if (esp_camera_init(&camera_config) == ESP_OK)
  {
    sensor_t *s = esp_camera_sensor_get();

    Serial.printf("Sensor ID: 0x%x\n", s->id.PID);
    startCameraServer();
  }
  else
  {
    Serial.println("Falha ao iniciar câmera");
  }
}

void loop()
{
  delay(10);
}
