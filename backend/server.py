from fastapi import FastAPI, UploadFile, File
from fastapi.responses import PlainTextResponse
import os
import shutil
import face_recognition

app = FastAPI()

# Diretório onde guardamos rostos conhecidos
KNOWN_DIR = "known_faces"
os.makedirs(KNOWN_DIR, exist_ok=True)

# Variáveis globais
known_faces_encodings = []
known_faces_names = []

# Carrega as faces conhecidas da pasta
def load_known_faces():
    known_faces_encodings.clear()
    known_faces_names.clear()
    
    for file in os.listdir(KNOWN_DIR):
        path = os.path.join(KNOWN_DIR, file)
        image = face_recognition.load_image_file(path)
        encodings = face_recognition.face_encodings(image)
        if encodings:
            known_faces_encodings.append(encodings[0])
            known_faces_names.append(os.path.splitext(file)[0])
        else:
            print(f"[!] Nenhum rosto encontrado em {file}")

# --------------------------
# ENDPOINT 1: Treino
# --------------------------
@app.post("/train", response_class=PlainTextResponse)
async def train_face(name: str, file: UploadFile = File(...)):
    # Guardar a imagem com o nome da pessoa
    filename = f"{name}.jpg"
    filepath = os.path.join(KNOWN_DIR, filename)
    with open(filepath, "wb") as buffer:
        shutil.copyfileobj(file.file, buffer)

    # Recarregar todos os rostos conhecidos
    load_known_faces()

    return f"Rosto de {name} treinado com sucesso!"

# --------------------------
# ENDPOINT 2: Reconhecimento
# --------------------------
@app.post("/recognize", response_class=PlainTextResponse)
async def recognize_face(file: UploadFile = File(...)):
    # Guardar a imagem temporária
    temp_path = "temp.jpg"
    with open(temp_path, "wb") as buffer:
        shutil.copyfileobj(file.file, buffer)

    # Carregar imagem e extrair encoding
    unknown_image = face_recognition.load_image_file(temp_path)
    unknown_encodings = face_recognition.face_encodings(unknown_image)

    if not unknown_encodings:
        return "Nenhum rosto detectado"

    unknown_encoding = unknown_encodings[0]

    # Comparar com os rostos conhecidos
    load_known_faces()
    results = face_recognition.compare_faces(known_faces_encodings, unknown_encoding)
    distances = face_recognition.face_distance(known_faces_encodings, unknown_encoding)

    if True in results:
        best_match_index = distances.argmin()
        return f"Rosto reconhecido: {known_faces_names[best_match_index]}"
    
    return "Rosto não reconhecido"
