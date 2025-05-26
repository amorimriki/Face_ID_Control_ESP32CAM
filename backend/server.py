from fastapi import FastAPI, UploadFile, File, Form
from fastapi.responses import PlainTextResponse
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware  # <--- importa o middleware
from PIL import Image

import os
import uuid
import shutil
from deepface import DeepFace
import numpy as np

app = FastAPI()

# -----------------------------
# Configurar CORS
origins = [
    "http://192.168.1.200",  # IP permitido
    "http://192.168.1.200:80",  # caso o pedido venha com porta explícita
    "http://192.168.1.200:3000",  # se usares React/Vite localmente
]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)
# -----------------------------



BACKUP_DIR = "known_faces/backup_faces"
TEMP_DIR = "known_faces/temp"


os.makedirs(BACKUP_DIR, exist_ok=True)
os.makedirs(TEMP_DIR, exist_ok=True)

EMBEDDINGS_FILE = "known_faces/embeddings.npy"
NAMES_FILE = "known_faces/names.txt"

def save_embeddings(embeddings, names):
    np.save(EMBEDDINGS_FILE, np.array(embeddings))
    with open(NAMES_FILE, "w") as f:
        for name in names:
            f.write(name + "\n")

def load_embeddings():
    if not os.path.exists(EMBEDDINGS_FILE) or not os.path.exists(NAMES_FILE):
        return [], []

    embeddings = np.load(EMBEDDINGS_FILE)
    with open(NAMES_FILE, "r") as f:
        names = [line.strip() for line in f.readlines()]

    # Convertemos embeddings para lista para podermos usar append depois
    embeddings_list = embeddings.tolist() if embeddings.ndim > 1 else [embeddings.tolist()]
    return embeddings_list, names

@app.post("/train", response_class=PlainTextResponse)
async def train_face(name: str = Form(...), files: list[UploadFile] = File(...)):
    print(f"Recebido pedido de treino para: {name}")
    print(f"Número de ficheiros recebidos: {len(files)}")

    embeddings, names = load_embeddings()
    saved = 0

    for idx, file in enumerate(files):
        print(f"\nGuardar ficheiro #{idx+1}: {file.filename}")
        unique_filename = f"{name}__{uuid.uuid4().hex}.jpg"
        backup_filepath = os.path.join(BACKUP_DIR, unique_filename)
        temp_filepath = os.path.join(TEMP_DIR, unique_filename)
        # Guardar backup da imagem primeiro
        with open(backup_filepath, "wb") as backup_buffer:
            shutil.copyfileobj(file.file, backup_buffer)
        # Abrir, rodar e guardar
        img = Image.open(backup_filepath)
        img_rotated = img.rotate(90, expand=True)
        img_rotated.save(backup_filepath)
        img_rotated.save(temp_filepath)
        print(f"Backup gravado e rodado em: {backup_filepath}")
   
   
    for filename in os.listdir(TEMP_DIR):
        filepath = os.path.join(TEMP_DIR, filename)
        print(f"\nProcessando ficheiro para treino: {filepath}")

        try:
            emb = DeepFace.represent(img_path=filepath, model_name="Facenet")[0]["embedding"]
            embeddings.append(emb)
            names.append(name)
            saved += 1
            # Apagar ficheiro do TEMP_DIR após treino bem sucedido
            os.remove(filepath)
            print(f"Imagem {filepath} processada e apagada da pasta TEMP.")
        except Exception as e:
            print(f"Erro ao extrair embedding da imagem {filepath}: {e}")
            

    # 3. Guardar embeddings e nomes atualizados
        

    save_embeddings(embeddings, names)
    print(f"Treino concluído: {saved} imagens treinadas para {name}")
    return f"{saved} imagens de {name} treinadas com sucesso!"


@app.post("/recognize")
async def recognize_face(file: UploadFile = File(...)):
    temp_path = "known_faces/temp/temp.jpg"


    with open(temp_path, "wb") as buffer:
        shutil.copyfileobj(file.file, buffer)
    
    img = Image.open(temp_path)
    img_rotated = img.rotate(90, expand=True)
    img_rotated.save(temp_path)
    img_rotated.save(temp_path)

    try:
        unknown_embedding = DeepFace.represent(img_path=temp_path, model_name="Facenet")[0]["embedding"]
        unknown_embedding = np.array(unknown_embedding)
    except Exception as e:
        return JSONResponse(content={"error": "Nenhum rosto detectado ou erro: " + str(e)})

    embeddings, names = load_embeddings()
    if len(embeddings) == 0:
        return JSONResponse(content={"error": "Nenhum rosto conhecido carregado."})

    distances = [np.linalg.norm(np.array(emb) - unknown_embedding) for emb in embeddings]
    threshold = 5.5

    best_match_index = np.argmin(distances)
    os.remove(temp_path)
    if distances[best_match_index] < threshold:
        return JSONResponse(content={"result": f"Rosto reconhecido: {names[best_match_index]}"})
    else:
        return JSONResponse(content={"result": f"Rosto não reconhecido (min distance: {distances[best_match_index]:.4f})"})


@app.get("/known_names", response_class=JSONResponse)
async def get_known_names():
    if not os.path.exists(NAMES_FILE):
        return []
    with open(NAMES_FILE, "r") as f:
        names_ = [line.strip() for line in f.readlines()]
        names = set(names_)
    return {"known_names": names}