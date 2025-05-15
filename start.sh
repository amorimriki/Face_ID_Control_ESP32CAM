#!/bin/bash

# Nome do ambiente virtual
VENV_DIR="venv"

# Verifica se o ambiente virtual já existe
if [ ! -d "$VENV_DIR" ]; then
    echo "[+] A criar ambiente virtual..."
    python3 -m venv $VENV_DIR
fi

# Ativa o ambiente virtual
source $VENV_DIR/bin/activate

# Instala os requisitos
echo "[+] A instalar dependências..."
pip install --upgrade pip
pip install fastapi uvicorn git+https://github.com/ageitgey/face_recognition_models face_recognition python-multipart 

# Corre o servidor FastAPI
cd backend
echo "[+] A iniciar o servidor FastAPI em http://0.0.0.0:8000 ..."
uvicorn server:app --host 0.0.0.0 --port 8000 --reload
