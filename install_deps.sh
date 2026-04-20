#!/usr/bin/env bash
# Run once with sudo to install all build dependencies for tpms_solver Module 1.
set -e

echo "==> Installing TPMS Solver build dependencies..."

sudo apt-get update -qq
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    gh \
    libglfw3-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libxext-dev \
    libvtk9-dev

echo ""
echo "==> All dependencies installed."
echo "    Now run:  bash build.sh"
