# server.py
from fastapi import FastAPI, UploadFile, File
from fastapi.responses import PlainTextResponse
import face_recognition
import numpy as np
import cv2
import os
from io import BytesIO
from PIL import Image

app = FastAPI()

known_faces = []
known_names = []

# Pasta para imagens de treino
TRAIN_DIR = "treino"

def carregar_treinamento():
    for nome in os.listdir(TRAIN_DIR):
        caminho = os.path.join(TRAIN_DIR, nome)
        img = face_recognition.load_image_file(caminho)
        encodings = face_recognition.face_encodings(img)
        if encodings:
            known_faces.append(encodings[0])
            known_names.append(os.path.splitext(nome)[0])

@app.post("/treinar")
def treinar(file: UploadFile = File(...)):
    image = face_recognition.load_image_file(BytesIO(file.file.read()))
    encodings = face_recognition.face_encodings(image)
    if encodings:
        nome = file.filename.split('.')[0]
        with open(f"{TRAIN_DIR}/{file.filename}", "wb") as f:
            f.write(file.file.read())
        return {"status": f"{nome} adicionado com sucesso!"}
    return {"erro": "Nenhum rosto detectado."}

@app.post("/photo")
async def reconhecer(file: UploadFile = File(...)):
    image = face_recognition.load_image_file(BytesIO(await file.read()))
    unknown_encoding = face_recognition.face_encodings(image)

    if unknown_encoding:
        matches = face_recognition.compare_faces(known_faces, unknown_encoding[0])
        if True in matches:
            idx = matches.index(True)
            return PlainTextResponse(f"Reconhecido: {known_names[idx]}")
        else:
            return PlainTextResponse("Rosto desconhecido.")
    return PlainTextResponse("Nenhum rosto detectado.")

@app.on_event("startup")
def startup_event():
    carregar_treinamento()
