#!/bin/bash

# Nome do ambiente virtual
VENV_DIR="backend/venv"

# Verifica se o ambiente virtual já existe
if [ ! -d "$VENV_DIR" ]; then
    echo "[+] A criar ambiente virtual..."
    python3 -m venv $VENV_DIR
fi

# Ativa o ambiente virtual
source $VENV_DIR/bin/activate

# Instala os requisitos
echo "[+] A instalar dependências..."
pip install --upgrade pip setuptools wheel
#pip install numpy==1.24.4
pip install fastapi uvicorn deepface python-multipart tf-keras

# Corre o servidor FastAPI

echo "[+] A iniciar o servidor FastAPI em --host 0.0.0.0 --port 8000 "
uvicorn backend.server:app --host 0.0.0.0 --port 8000
