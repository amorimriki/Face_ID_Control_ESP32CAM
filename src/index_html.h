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
        width: 600px; /* inverso da rotação para manter proporção */
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
    <br />
    <p id="ip-info"></p>
    <br /><br />
    <br /><br />
    <br /><br />
    <img id="video" src="/stream" alt="Stream" />
    <br /><br /><br />
    <br /><br /><br />
    <div class="controls">
      <button id="captureBtn">Capturar Foto</button>
      <button id="trainBtn" disabled>Treinar</button>
      <button id="recognizeBtn">Reconhecer</button>
      <button id="ledBtn">Ligar LED</button>
    </div>
    <div id="photos-container">
      <img
        id="photo0"
        class="photo-slot"
        src=""
        alt="Slot 1"
        title="Clique para apagar"
        style="display: none"
      />
      <img
        id="photo1"
        class="photo-slot"
        src=""
        alt="Slot 2"
        title="Clique para apagar"
        style="display: none"
      />
      <img
        id="photo2"
        class="photo-slot"
        src=""
        alt="Slot 3"
        title="Clique para apagar"
        style="display: none"
      />
      <img
        id="photo3"
        class="photo-slot"
        src=""
        alt="Slot 4"
        title="Clique para apagar"
        style="display: none"
      />
    </div>
    <canvas id="canvas" style="display: none"></canvas>
    <h1>Nomes Registados</h1>
    <br />
    <p id="server-info"></p>

    <ul id="lista-nomes"></ul>

    <script>
      // URLs do backend (usar strings normais em JS)
      const serverURL = "http://192.168.1.210:8000";
      const trainUrl = serverURL + "/train";
      const recognizeUrl = serverURL + "/recognize";
      const namesUrl = serverURL + "/known_names";

      const video = document.getElementById("video");
      const canvas = document.getElementById("canvas");
      const captureBtn = document.getElementById("captureBtn");
      const trainBtn = document.getElementById("trainBtn");
      const ledBtn = document.getElementById("ledBtn");
      const photosContainer = document.getElementById("photos-container");
      let ledOn = false;
      const esp32IP = window.location.origin; // IP base da página

      let capturedPhotos = [null, null, null, null];
      document.getElementById(
        "ip-info"
      ).textContent = `Conectado ao ESP32 em: ${esp32IP}`;

      captureBtn.onclick = () => {
        const firstEmptyIndex = capturedPhotos.findIndex(
          (photo) => photo === null
        );
        if (firstEmptyIndex === -1) {
          //alert("Já tens 4 fotos capturadas. Apaga alguma para substituir.");
          return;
        }
        capturePhoto(firstEmptyIndex);
      };

      trainBtn.onclick = () => {
        if (capturedPhotos.some((photo) => photo === null)) {
          //alert("Tens de capturar 4 fotos antes de treinar.");
          return;
        }
        sendPhotosForTraining();
      };

      function setSemaforo(cor) {
        const src = video.src;
        video.src = ""; // Interrompe o vídeo

       fetch(`${esp32IP}/semaforo?cor=${cor}`)
        .then((res) => {
          if (!res.ok) throw new Error(`Erro HTTP ${res.status}`);
          return res.text();
        })
        .then((msg) => {
          video.src = src; // Só retoma o vídeo após receber resposta
        })
        .catch((err) => {
          video.src = src; // Retoma mesmo em caso de erro
          //alert("Erro no semáforo: " + err);
        });
      }
      function capturePhoto(index) {
        // Ajustar canvas para o vídeo
        canvas.width = video.videoWidth || video.naturalWidth;
        canvas.height = video.videoHeight || video.naturalHeight;
        const ctx = canvas.getContext("2d");
        ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
        canvas.toBlob((blob) => {
          capturedPhotos[index] = blob;
          updatePhotoSlot(index, URL.createObjectURL(blob));
          updateTrainButton();
        }, "image/jpeg");
      }

      function updatePhotoSlot(index, url) {
        const img = document.getElementById("photo" + index);
        img.src = url;
        img.style.display = "inline-block";
      }

      for (let i = 0; i < 4; i++) {
        document.getElementById("photo" + i).onclick = () => {
          if (capturedPhotos[i] !== null) {
            if (confirm("Apagar foto deste slot?")) {
              capturedPhotos[i] = null;
              const img = document.getElementById("photo" + i);
              img.src = "";
              img.style.display = "none";
              updateTrainButton();
            }
          }
        };
      }

      function updateTrainButton() {
        trainBtn.disabled = !capturedPhotos.every((photo) => photo !== null);
      }

      function sendPhotosForTraining() {
        setSemaforo("amarelo");
        const formData = new FormData();
        const name = prompt("Indique o nome da pessoa para treinar:");
        if (!name) {
          //alert("Nome obrigatório.");
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
          body: formData,
        })
          .then((res) => {
            if (!res.ok) throw new Error(`Erro HTTP ${res.status}`);
            return res.text();
          })
          .then((txt) => {
            //alert("Resposta do servidor: " + txt);
            setSemaforo("verde");
          })
          .catch((err) => {
            //alert("Erro no envio: " + err);
            setSemaforo("vermelho");
          });
      }

      function toggleLED() {
        const src = video.src;
        video.src = "";
        const newState = !ledOn;
        fetch(`${esp32IP}/led?on=${newState ? 1 : 0}`)
          .then((res) => res.text())
          .then((txt) => {
            ledOn = newState;
            updateLEDButton();
            video.src = src;
          })
          .catch((err) => {
            //alert("Erro LED: " + err);
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
          var resposta = await fetch(namesUrl);
          console.log(resposta);
          if (!resposta.ok) throw new Error(`HTTP ${resposta.status}`);
          var data = await resposta.json();
          console.log(data);
          var nomes = data.known_names;
          console.log(data);
          var lista = document.getElementById("lista-nomes");
          lista.innerHTML = "";
          nomes.forEach((nome) => {
            var li = document.createElement("li");
            li.textContent = nome;
            lista.appendChild(li);
          });
        } catch (err) {
          console.error("Erro ao carregar nomes:", err);
        }
      }
      console.log("Nomes carregados");

      function init() {
        console.log("Página carregada ou reativada");
        document.getElementById(
          "server-info"
        ).textContent = `Conectado ao API FaceID em: ${serverURL}`;
        carregarNomesRegistados();
      }
      const recognizeBtn = document.getElementById("recognizeBtn");

      recognizeBtn.onclick = () => {
        carregarNomesRegistados();
        captureAndRecognize();
      };

      function captureAndRecognize() {
        // Ajustar canvas para o vídeo
        canvas.width = video.videoWidth || video.naturalWidth;
        canvas.height = video.videoHeight || video.naturalHeight;
        const ctx = canvas.getContext("2d");
        ctx.drawImage(video, 0, 0, canvas.width, canvas.height);

        canvas.toBlob(async (blob) => {
          if (!blob) {
            //alert("Erro ao capturar a imagem.");
            return;
          }
          const formData = new FormData();
          formData.append("file", blob, `recognize.jpg`);

          try {
            setSemaforo("amarelo");
            const response = await fetch(recognizeUrl, {
              method: "POST",
              body: formData,
            });
            if (!response.ok) {
              setSemaforo("vermelho");
              throw new Error(`Erro HTTP ${response.status}`);
            } else {
              const result = await response.json();

              if (result.result) {
                setSemaforo("verde");
                //alert("Reconhecimento: " + result.result);
              } else if (result.error) {
                setSemaforo("vermelho");
                //alert("Erro no reconhecimento: " + result.error);
              } else {
                setSemaforo("vermelho");
                //alert("Resposta inesperada do servidor: " + JSON.stringify(result));
              }
            }
          } catch (err) {
            setSemaforo("vermelho");
            //alert("Erro no reconhecimento: " + err);
          }
        }, "image/jpeg");
      }

      window.onload = init;
    </script>
  </body>
</html>

)rawliteral";
