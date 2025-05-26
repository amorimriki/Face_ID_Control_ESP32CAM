/*
ESP32_VideoStreem.cpp
*/

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "ZON-ZON_MEO";
const char* password = "4da883991d45zaQ239aZwpL";

//const char* ssid = "iPhone de Ricardo";
//const char* password = "mariagata";


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
  .pixel_format   = PIXFORMAT_RGB565,  // A Cor para PIXFORMAT_RGB565, PIXFORMAT_GRAYSCALE,
  .frame_size     = FRAMESIZE_HVGA,
  .fb_count       = 2,
  .grab_mode      = CAMERA_GRAB_WHEN_EMPTY
};


volatile bool streaming = true;  // Flag para controlar o streaming

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
    bool converted = frame2jpg(fb, 24, &jpg_buf, &jpg_buf_len);
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
    delay(5);  // Ajuste fino de fluidez
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
      delay(200); // Aguarda um pequeno tempo para garantir a interrupção do stream

      // Resposta ao cliente
      const char* ledStatus = estado ? "LED Ligado" : "LED Desligado";
      httpd_resp_sendstr(req, ledStatus);

      // Reativa o streaming após a mudança do LED
      delay(200);  // Aguarda a resposta ser enviada antes de reiniciar o streaming
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
<html lang="pt">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32-CAM Face Control</title>
  <style>
    body {
      background: #f5f5f5;
      font-family: Arial, sans-serif;
      text-align: center;
      padding: 1rem;
      margin: 0;
    }
    /* Container para o vídeo com tamanho fixo para evitar que tapem os botões */
    .stream {
      width: auto;
      height: auto;
      margin: 0 auto 1rem auto;
      border: 2px solid #ccc;
      border-radius: 10px;
    }
    /* Vídeo rotacionado */
    img#video {
      width: 600px;    /* inverso da rotação para manter proporção */
      height: auto;
      transform: rotate(270deg);
      transform-origin: center center;
      display: block;
      margin: auto;
    }
    /* Fotos capturadas rotacionadas igual ao vídeo */
    .photo-slot {
      width: 70px;
      height: 90px;
      margin: 5px;
      border-radius: 10px;
      border: 2px solid #ccc;
      object-fit: cover;
      cursor: pointer;
      transform: rotate(270deg);
      transform-origin: center center;
      display: inline-block;
    }

    button {
      padding: 8px 16px;
      margin: 5px 8px;
      font-size: 14px;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      background-color: #007bff;
      color: white;
      transition: background-color 0.3s ease;
    }

    button:disabled {
      background-color: #999;
      cursor: not-allowed;
    }
    button:hover:enabled {
      background-color: #0056b3;
    }
    #photos-container {
      margin-top: 1rem;
    }

    h1 {
      margin-bottom: 0.5rem;
    }
    p#ip-info {
      margin-top: 0;
      margin-bottom: 1.5rem;
      font-weight: bold;
      color: #333;
    }
    /* Container dos botões */
    .controls {
      margin-bottom: 1rem;
    }
    p#server-info {
     margin-top: 0;
      margin-bottom: 1.5rem;
      font-weight: bold;
      color: #333;
}
  </style>
</head>
<body>
  <h1>ESP32-CAM Reconhecimento Facial</h1>
  <br>
  <p id="ip-info"></p>
<br><br>
<br><br>
<br><br>
    <img id="video" src="/stream" alt="Stream" />
<br><br><br>
<br><br><br>
  <div class="controls">
    <button id="captureBtn">Capturar Foto</button>
    <button id="trainBtn" disabled>Treinar</button>
    <button id="recognizeBtn">Reconhecer</button>
    <button id="ledBtn">Ligar LED</button>
  </div>
  <div id="photos-container">
    <img id="photo0" class="photo-slot" src="" alt="Slot 1" title="Clique para apagar" style="display:none;" />
    <img id="photo1" class="photo-slot" src="" alt="Slot 2" title="Clique para apagar" style="display:none;" />
    <img id="photo2" class="photo-slot" src="" alt="Slot 3" title="Clique para apagar" style="display:none;" />
    <img id="photo3" class="photo-slot" src="" alt="Slot 4" title="Clique para apagar" style="display:none;" />
  </div>
  <canvas id="canvas" style="display:none;"></canvas>
  <h1>Nomes Registados</h1>
  <br>
  <p id="server-info"></p>

  <ul id="lista-nomes"></ul>

  <script>

    // URLs do backend (usar strings normais em JS)
    const serverURL = "http://192.168.1.210:8000"
    const trainUrl = serverURL + "/train";
    const recognizeUrl = serverURL + "/recognize";
    const namesUrl = serverURL + "/known_names";

    const video = document.getElementById('video');
    const canvas = document.getElementById('canvas');
    const captureBtn = document.getElementById('captureBtn');
    const trainBtn = document.getElementById('trainBtn');
    const ledBtn = document.getElementById('ledBtn');
    const photosContainer = document.getElementById('photos-container');
    let ledOn = false;
    const esp32IP = window.location.origin; // IP base da página

    let capturedPhotos = [null, null, null, null];
    document.getElementById("ip-info").textContent = `Conectado ao ESP32 em: ${esp32IP}`;

    captureBtn.onclick = () => {
      const firstEmptyIndex = capturedPhotos.findIndex(photo => photo === null);
      if (firstEmptyIndex === -1) {
        alert("Já tens 4 fotos capturadas. Apaga alguma para substituir.");
        return;
      }
      capturePhoto(firstEmptyIndex);
    };

    trainBtn.onclick = () => {
      if (capturedPhotos.some(photo => photo === null)) {
        alert("Tens de capturar 4 fotos antes de treinar.");
        return;
      }
      sendPhotosForTraining();
    };

    function capturePhoto(index) {
      // Ajustar canvas para o vídeo
      canvas.width = video.videoWidth || video.naturalWidth;
      canvas.height = video.videoHeight || video.naturalHeight;
      const ctx = canvas.getContext('2d');
      ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
      canvas.toBlob(blob => {
        capturedPhotos[index] = blob;
        updatePhotoSlot(index, URL.createObjectURL(blob));
        updateTrainButton();
      }, 'image/jpeg');
    }

    function updatePhotoSlot(index, url) {
      const img = document.getElementById('photo' + index);
      img.src = url;
      img.style.display = 'inline-block';
    }

    for (let i = 0; i < 4; i++) {
      document.getElementById('photo' + i).onclick = () => {
        if (capturedPhotos[i] !== null) {
          if (confirm("Apagar foto deste slot?")) {
            capturedPhotos[i] = null;
            const img = document.getElementById('photo' + i);
            img.src = "";
            img.style.display = 'none';
            updateTrainButton();
          }
        }
      };
    }

    function updateTrainButton() {
      trainBtn.disabled = !capturedPhotos.every(photo => photo !== null);
    }

   function sendPhotosForTraining() {
  const formData = new FormData();
  const name = prompt("Indique o nome da pessoa para treinar:");
  if (!name) {
    alert("Nome obrigatório.");
    return;
  }
  formData.append("name", name);
  capturedPhotos.forEach((blob, i) => {
    formData.append("files", blob, `photo${i}.jpg`);
  });

  // Debug do conteúdo do FormData
  for (let pair of formData.entries()) {
    console.log(pair[0], pair[1]);
  }

  fetch(trainUrl, {
    method: "POST",
    body: formData
  })
  .then(res => {
    if (!res.ok) throw new Error(`Erro HTTP ${res.status}`);
    return res.text();
  })
  .then(txt => alert("Resposta do servidor: " + txt))
  .catch(err => alert("Erro no envio: " + err));
}

    function toggleLED() {
      const src = video.src;
      video.src = '';
      const newState = !ledOn;
      fetch(`${esp32IP}/led?on=${newState ? 1 : 0}`)
        .then(res => res.text())
        .then(txt => {
          ledOn = newState;
          updateLEDButton();
          video.src = src;
        })
        .catch(err => {
          alert("Erro LED: " + err);
          video.src = src;
        });
    }

    function updateLEDButton() {
      ledBtn.textContent = ledOn ? "Desligar LED" : "Ligar LED";
    }

    ledBtn.onclick = toggleLED;

    // Função para carregar nomes registados do backend e mostrar na lista
    async function carregarNomesRegistados() {
      try {
        const resposta = " "
        resposta = await fetch(namesUrl, { cache: "no-store" });
        console.log("Resposta recebida:", resposta);

        const data = await resposta.json();
        console.log("JSON parseado:", data);

        const nomes = data.known_names;
        console.log("Nomes extraídos:", nomes);

        const lista = document.getElementById('lista-nomes');
        lista.innerHTML = '';
        nomes.forEach(nome => {
          const item = document.createElement('li');
          item.textContent = nome;
          lista.appendChild(item);
        });
      } catch (err) {
        console.error("Erro ao carregar nomes:", err);
      }
    }

    function init() {
    console.log("Página carregada ou reativada");
    document.getElementById("server-info").textContent = `Conectado ao API FaceID em: ${serverURL}`;
    carregarNomesRegistados();
    console.log("Nomes carregados");
  }
const recognizeBtn = document.getElementById('recognizeBtn');

recognizeBtn.onclick = () => {
  captureAndRecognize();
};

function captureAndRecognize() {
  // Ajustar canvas para o vídeo
  canvas.width = video.videoWidth || video.naturalWidth;
  canvas.height = video.videoHeight || video.naturalHeight;
  const ctx = canvas.getContext('2d');
  ctx.drawImage(video, 0, 0, canvas.width, canvas.height);

  canvas.toBlob(async (blob) => {
    if (!blob) {
      alert("Erro ao capturar a imagem.");
      return;
    }
    const formData = new FormData();
    formData.append("file", blob, `recognize.jpg`);

    try {
      const response = await fetch(recognizeUrl, {
        method: "POST",
        body: formData
      });
      if (!response.ok) throw new Error(`Erro HTTP ${response.status}`);

      const result = await response.json();
      alert("Reconhecimento: " + JSON.stringify(result));
    } catch (err) {
      alert("Erro no reconhecimento: " + err);
    }
  }, 'image/jpeg');
}



  window.onload = init;
</script>
</body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, MAIN_page, strlen(MAIN_page));
}

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

void loop() {
  delay(10);
}


