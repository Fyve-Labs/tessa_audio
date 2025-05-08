#!/bin/bash
set -e

# Detect operating system
OS=$(uname -s)

# Directory of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

echo "Building tessa_audio for $OS..."

# Install dependencies based on OS
if [ "$OS" = "Darwin" ]; then
    # macOS
    if ! command -v brew &> /dev/null; then
        echo "Homebrew not found. Please install Homebrew first."
        exit 1
    fi

    # Check and install dependencies
    echo "Checking dependencies..."
    
    if ! brew ls --versions portaudio > /dev/null; then
        echo "Installing PortAudio..."
        brew install portaudio
    fi
    
    if ! brew ls --versions zeromq > /dev/null; then
        echo "Installing ZeroMQ..."
        brew install zeromq
    fi
    
    if ! brew ls --versions cppzmq > /dev/null; then
        echo "Installing cppzmq headers..."
        brew install cppzmq
    fi
    
    # Build with CMake
    cd "$BUILD_DIR"
    echo "Configuring project..."
    cmake -DCMAKE_BUILD_TYPE=Release ..
    
    echo "Building project..."
    make -j$(sysctl -n hw.logicalcpu)
    
elif [ "$OS" = "Linux" ]; then
    # Linux (assuming Debian/Ubuntu)
    # Check package manager
    if command -v apt-get &> /dev/null; then
        # Debian/Ubuntu
        echo "Checking dependencies..."
        
        if ! dpkg -s libportaudio2 libportaudio-dev &> /dev/null; then
            echo "Installing PortAudio..."
            sudo apt-get update
            sudo apt-get install -y libportaudio2 libportaudio-dev
        fi
        
        if ! dpkg -s libzmq3-dev &> /dev/null; then
            echo "Installing ZeroMQ..."
            sudo apt-get update
            sudo apt-get install -y libzmq3-dev
        fi
        
        if ! dpkg -s cmake build-essential &> /dev/null; then
            echo "Installing build tools..."
            sudo apt-get update
            sudo apt-get install -y cmake build-essential
        fi
    elif command -v yum &> /dev/null; then
        # RHEL/CentOS/Fedora
        echo "Checking dependencies..."
        
        echo "Installing PortAudio..."
        sudo yum install -y portaudio portaudio-devel
        
        echo "Installing ZeroMQ..."
        sudo yum install -y zeromq zeromq-devel
        
        echo "Installing build tools..."
        sudo yum install -y cmake gcc-c++ make
    else
        echo "Unsupported Linux distribution. Please install dependencies manually."
        echo "Required: PortAudio, ZeroMQ, CMake, and C++ compiler."
        exit 1
    fi
    
    # Build with CMake
    cd "$BUILD_DIR"
    echo "Configuring project..."
    cmake -DCMAKE_BUILD_TYPE=Release ..
    
    echo "Building project..."
    make -j$(nproc)
else
    echo "Unsupported operating system: $OS"
    exit 1
fi

echo "Build complete. Binary is located at $BUILD_DIR/tessa_audio" 