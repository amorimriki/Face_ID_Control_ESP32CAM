#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// =================== CONFIG WI-FI ===================
const char* ssid = "ZON-ZON_MEO";
const char* password = "4da883991d45zaQ239aZwpL";

// =================== PINAGEM ESP32-CAM AI-THINKER ===================
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

#define LED_GPIO_PIN      4  // Pino do LED embutido

// =================== CONFIG DA CÂMERA ===================
camera_config_t camera_config = {
  .pin_pwdn       = PWDN_GPIO_NUM,
  .pin_reset      = RESET_GPIO_NUM,
  .pin_xclk       = XCLK_GPIO_NUM,
  .pin_sccb_sda   = SIOD_GPIO_NUM,
  .pin_sccb_scl   = SIOC_GPIO_NUM,

  .pin_d7         = Y9_GPIO_NUM,
  .pin_d6         = Y8_GPIO_NUM,
  .pin_d5         = Y7_GPIO_NUM,
  .pin_d4         = Y6_GPIO_NUM,
  .pin_d3         = Y5_GPIO_NUM,
  .pin_d2         = Y4_GPIO_NUM,
  .pin_d1         = Y3_GPIO_NUM,
  .pin_d0         = Y2_GPIO_NUM,
  .pin_vsync      = VSYNC_GPIO_NUM,
  .pin_href       = HREF_GPIO_NUM,
  .pin_pclk       = PCLK_GPIO_NUM,

  .xclk_freq_hz   = 20000000,
  .ledc_timer     = LEDC_TIMER_0,
  .ledc_channel   = LEDC_CHANNEL_0,
  .pixel_format   = PIXFORMAT_GRAYSCALE,  // Mudando para RGB565
  .frame_size     = FRAMESIZE_HVGA,
  .fb_count       = 2,
  .grab_mode      = CAMERA_GRAB_WHEN_EMPTY
};

// =================== FLAG DE STREAMING ===================
volatile bool streaming = true;  // Flag para controlar o streaming

// =================== STREAM HANDLER ===================
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
  if (res != ESP_OK) return res;

  while (streaming) {  // Verifica a flag de streaming
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Erro ao obter frame");
      continue;
    }

    // Converte o frame para JPEG
    size_t jpg_buf_len = 0;
    uint8_t *jpg_buf = NULL;
    bool converted = frame2jpg(fb, 30, &jpg_buf, &jpg_buf_len);
    esp_camera_fb_return(fb);

    if (!converted) {
      Serial.println("Erro ao converter para JPEG");
      continue;
    }

    // Envia o JPEG como resposta
    char part[64];
    snprintf(part, sizeof(part), "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpg_buf_len);
    res = httpd_resp_send_chunk(req, part, strlen(part));
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_buf_len);
    }
    res = httpd_resp_send_chunk(req, "\r\n", 2);

    free(jpg_buf);

    if (res != ESP_OK) break;
    delay(10);  // Ajuste fino de fluidez
  }

  return res;
}

// =================== HANDLER PARA LED ===================
static esp_err_t led_handler(httpd_req_t *req) {
  char query[64];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    char param[8];
    if (httpd_query_key_value(query, "on", param, sizeof(param)) == ESP_OK) {
      int estado = atoi(param);
      digitalWrite(LED_GPIO_PIN, estado ? HIGH : LOW);
      
      // Interrompe o stream por um momento quando o LED for alterado
      streaming = false;
      delay(500); // Aguarda um pequeno tempo para garantir a interrupção do stream

      // Resposta ao cliente
      const char* ledStatus = estado ? "LED Ligado" : "LED Desligado";
      httpd_resp_sendstr(req, ledStatus);

      // Reativa o streaming após a mudança do LED
      delay(500);  // Aguarda a resposta ser enviada antes de reiniciar o streaming
      streaming = true;

      return ESP_OK;
    }

    if (httpd_query_key_value(query, "status", param, sizeof(param)) == ESP_OK) {
      int estado = digitalRead(LED_GPIO_PIN);
      return httpd_resp_sendstr(req, estado ? "1" : "0");
    }
  }
  return httpd_resp_sendstr(req, "Parâmetro inválido");
}
// =================== HANDLER PARA A PÁGINA PRINCIPAL ===================
static const char MAIN_page[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>ESP32-CAM Face Control</title>
    <style>
      body {
        background: #f5f5f5;
        font-family: Arial, sans-serif;
        text-align: center;
        padding: 2rem;
      }
      canvas {
        width: 100%;
        max-width: 400px;
        border-radius: 10px;
        margin-bottom: 1rem;
      }
        img {
      width: 80%;
      height: auto;
    }
      .controls {
        margin-top: 10px;
      }
      button {
        padding: 10px 20px;
        margin: 5px;
        font-size: 16px;
        border: none;
        border-radius: 8px;
        cursor: pointer;
        background-color: #007bff;
        color: white;
      }
      button:hover {
        background-color: #0056b3;
      }
    </style>
  </head>
<body>
  <h1>ESP32-CAM Reconhecimento Facial</h1>

  <img id="video" src="/stream" alt="Stream">
  <canvas id="canvas" style="display:none;"></canvas>

  <div class="controls">
    <button onclick="capture()">Capturar Foto</button>
    <button id="ledBtn" onclick="toggleLED()">Ligar LED</button>
  </div>

  <p id="ip-info"></p>

  <script>
    const video = document.getElementById('video');
    const canvas = document.getElementById('canvas');
    const ledBtn = document.getElementById("ledBtn");
    let ledOn = false;
    const esp32IP = window.location.origin;

    document.getElementById("ip-info").textContent = `Conectado ao ESP32 em: ${esp32IP}`;

    function capture() {
      canvas.style.display = 'block';
      const context = canvas.getContext('2d');
      canvas.width = video.width;
      canvas.height = video.height;
      context.drawImage(video, 0, 0, canvas.width, canvas.height);
      canvas.toBlob(sendImage, 'image/jpeg');
    }

    function sendImage(blob) {
      const formData = new FormData();
      formData.append("file", blob, "capture.jpg");

      fetch("http://192.168.1.100:8000/photo", {
        method: "POST",
        body: formData
      }).then(res => res.text())
        .then(txt => alert("Resposta: " + txt))
        .catch(err => alert("Erro: " + err));
    }

    function toggleLED() {
      const src = video.src;
      video.src = ''; // Interrompe o stream

      const newState = !ledOn;

      fetch(`${esp32IP}/led?on=${newState ? 1 : 0}`)
        .then(res => res.text())
        .then(txt => {
          ledOn = newState;
          updateLEDButton();
          video.src = src; // Retoma o stream
        })
        .catch(err => {
          alert("Erro LED: " + err);
          video.src = src; // Retoma mesmo se erro
        });
    }

    function updateLEDButton() {
      ledBtn.textContent = ledOn ? "Desligar LED" : "Ligar LED";
    }
  </script>
</body>
  </html>
  )rawliteral";


// =================== FUNÇÃO PARA SERVIR O HTML ===================
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, MAIN_page, strlen(MAIN_page));
}

// =================== START SERVER ===================
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

   // Registro da URI para a página principal (index)
   httpd_uri_t index_uri = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = index_handler,
    .user_ctx = NULL
  };

  // Registro da URI para o stream de vídeo
  httpd_uri_t stream_uri = {
    .uri      = "/stream",
    .method   = HTTP_GET,
    .handler  = stream_handler,
    .user_ctx = NULL
  };
  
  // Handler para controle do LED
  httpd_uri_t led_uri = {
    .uri      = "/led",
    .method   = HTTP_GET,
    .handler  = led_handler,
    .user_ctx = NULL
  };


  httpd_handle_t server = NULL;
  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &led_uri);
    httpd_register_uri_handler(server, &index_uri);
    httpd_register_uri_handler(server, &stream_uri);
    Serial.println("Servidor de vídeo ativo!");
  } else {
    Serial.println("Erro ao iniciar servidor HTTP");
  }
}

// =================== WIFI ===================
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// =================== SETUP ===================
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  pinMode(LED_GPIO_PIN, OUTPUT);
  digitalWrite(LED_GPIO_PIN, LOW); 
  connectToWiFi();

  if (esp_camera_init(&camera_config) == ESP_OK) {
    sensor_t *s = esp_camera_sensor_get();
    Serial.printf("Sensor ID: 0x%x\n", s->id.PID);
    startCameraServer();
  } else {
    Serial.println("Falha ao iniciar câmera");
  }
}

// =================== LOOP ===================
void loop() {
  delay(10);
}


