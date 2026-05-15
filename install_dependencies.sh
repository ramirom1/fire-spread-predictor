#!/bin/bash
# ============================================================
# Script de instalación de dependencias
# Fire Spread Predictor
# ============================================================

set -e

echo "=== Instalando dependencias ==="

# Compilador C++
echo "[1/3] Verificando g++..."
if ! command -v g++ &> /dev/null; then
    echo "  Instalando build-essential..."
    sudo apt install -y build-essential
else
    echo "  g++ ya instalado ($(g++ --version | head -1))"
fi

# SDL2 (visualización de la matriz)
echo "[2/3] Verificando SDL2..."
if ! pkg-config --exists sdl2 2>/dev/null; then
    echo "  Instalando libsdl2-dev..."
    sudo apt install -y libsdl2-dev
else
    echo "  SDL2 ya instalado ($(pkg-config --modversion sdl2))"
fi

# MPI (ejecución paralela)
echo "[3/3] Verificando MPI..."
if ! command -v mpic++ &> /dev/null; then
    echo "  Instalando OpenMPI..."
    sudo apt install -y libopenmpi-dev openmpi-bin
else
    echo "  MPI ya instalado ($(mpic++ --showme:version 2>&1 | head -1))"
fi

echo ""
echo "=== Todas las dependencias instaladas ==="
