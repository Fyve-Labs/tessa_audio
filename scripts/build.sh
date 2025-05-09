#!/bin/bash
# Build script for Tessa Audio
# This script consolidates the build steps from the GitHub workflow

# switch to the parent directory to run
cd "$(dirname "$0")/.."

set -e

# Default values
BUILD_TYPE="Release"
BUILD_DIR="build"
WORKSPACE_DIR="$(pwd)"
UPDATE_VERSION=false
NEW_VERSION=""
UPDATE_VERSION_H=false
DRY_RUN=false
TOOLCHAIN_FILE=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --build-type)
      BUILD_TYPE="$2"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --update-version)
      UPDATE_VERSION=true
      NEW_VERSION="$2"
      shift 2
      ;;
    --update-version-h)
      UPDATE_VERSION_H=true
      NEW_VERSION="$2"
      shift 2
      ;;
    --dry-run)
      DRY_RUN=true
      shift
      ;;
    --toolchain-file)
      TOOLCHAIN_FILE="$2"
      shift 2
      ;;
    --help)
      echo "Usage: $0 [options]"
      echo "Options:"
      echo "  --build-type TYPE     Set build type (Debug, Release) [default: Release]"
      echo "  --build-dir DIR       Set build directory [default: build]"
      echo "  --update-version VER  Update version in CMakeLists.txt (e.g., 1.0.0)"
      echo "  --update-version-h VER Update version in version.h (e.g., 1.0.0)"
      echo "  --toolchain-file FILE Path to CMake toolchain file for cross-compilation"
      echo "  --dry-run             Show commands without executing them"
      echo "  --help                Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Run '$0 --help' for usage"
      exit 1
      ;;
  esac
done

# Function to execute or echo commands based on dry run mode
run_cmd() {
  if [[ "$DRY_RUN" == true ]]; then
    echo "WOULD RUN: $@"
    return 0
  else
    echo "RUNNING: $@"
    "$@"
    return $?
  fi
}

# Only install dependencies if not cross-compiling
if [[ -z "$TOOLCHAIN_FILE" ]]; then
  # Detect OS for installing dependencies
  if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Detected Linux system"
    OS_TYPE="linux"
    
    # Check if we have sudo access
    if command -v sudo &> /dev/null; then
      echo "Installing dependencies for Ubuntu/Debian..."
      LIBSET="libportaudio2 libportaudio-dev libzmq3-dev cmake build-essential"
      if [[ "$DRY_RUN" == true ]]; then
        echo "WOULD RUN: sudo apt-get update"
        echo "WOULD RUN: sudo apt-get install -y $LIBSET"
      else
        sudo apt-get update
        sudo apt-get install -y $LIBSET
      fi
    else
      echo "WARNING: Cannot install dependencies without sudo access."
      echo "Please make sure the following packages are installed:"
      echo "  libportaudio2 libportaudio-dev libzmq3-dev cmake build-essential"
    fi
  elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Detected macOS system"
    OS_TYPE="macos"
    
    # Check if Homebrew is installed
    if command -v brew &> /dev/null; then
      echo "Installing dependencies with Homebrew..."
      if [[ "$DRY_RUN" == true ]]; then
        echo "WOULD RUN: brew install portaudio zeromq cppzmq cmake"
      else
        brew install portaudio zeromq cppzmq cmake
      fi
    else
      echo "WARNING: Homebrew not found. Cannot install dependencies."
      echo "Please make sure the following packages are installed:"
      echo "  portaudio zeromq cppzmq cmake"
    fi
  else
    echo "Unsupported OS: $OSTYPE"
    echo "This script currently supports Linux and macOS"
    exit 1
  fi
else
  echo "Cross-compiling with toolchain: $TOOLCHAIN_FILE"
  # Skip dependency installation when cross-compiling
fi

# Create build directory
echo "Creating build directory: $BUILD_DIR"
run_cmd mkdir -p "$BUILD_DIR"

# Update version in CMakeLists.txt if requested
if [[ "$UPDATE_VERSION" == true && -n "$NEW_VERSION" ]]; then
  echo "Updating version in CMakeLists.txt to $NEW_VERSION"
  if [[ "$DRY_RUN" == true ]]; then
    echo "WOULD RUN: sed -i.bak \"s/project(tessa_audio VERSION [0-9]*\\.[0-9]*\\.[0-9]*)/project(tessa_audio VERSION $NEW_VERSION)/g\" CMakeLists.txt"
    echo "WOULD RUN: rm -f CMakeLists.txt.bak"
  else
    sed -i.bak "s/project(tessa_audio VERSION [0-9]*\.[0-9]*\.[0-9]*)/project(tessa_audio VERSION $NEW_VERSION)/g" CMakeLists.txt
    rm -f CMakeLists.txt.bak
  fi
fi

# Update version in version.h if requested
if [[ "$UPDATE_VERSION_H" == true && -n "$NEW_VERSION" ]]; then
  echo "Updating version in include/version.h to $NEW_VERSION"
  if [[ "$DRY_RUN" == true ]]; then
    echo "WOULD RUN: scripts/update-version.sh $NEW_VERSION"
  else
    # Make sure the script is executable
    chmod +x scripts/update-version.sh
    scripts/update-version.sh $NEW_VERSION
  fi
fi

# Configure with CMake
echo "Configuring with CMake (Build type: $BUILD_TYPE)"
CMAKE_ARGS="-S \"$WORKSPACE_DIR\" -B . -DCMAKE_BUILD_TYPE=\"$BUILD_TYPE\" -DBUILD_TESTING=ON"

# Add toolchain file if specified
if [[ -n "$TOOLCHAIN_FILE" ]]; then
  CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=\"$TOOLCHAIN_FILE\""
fi

if [[ "$DRY_RUN" == true ]]; then
  echo "WOULD RUN: pushd \"$BUILD_DIR\" > /dev/null"
  echo "WOULD RUN: cmake $CMAKE_ARGS"
  echo "WOULD RUN: popd > /dev/null"
else
  pushd "$BUILD_DIR" > /dev/null
  # Using eval to handle the quotes in CMAKE_ARGS
  eval cmake $CMAKE_ARGS
  popd > /dev/null
fi

# Build
echo "Building project"
if [[ "$DRY_RUN" == true ]]; then
  echo "WOULD RUN: pushd \"$BUILD_DIR\" > /dev/null"
  echo "WOULD RUN: cmake --build . --config \"$BUILD_TYPE\""
  echo "WOULD RUN: popd > /dev/null"
else
  pushd "$BUILD_DIR" > /dev/null
  cmake --build . --config "$BUILD_TYPE"
  popd > /dev/null
fi


echo "Build completed successfully!"
echo "Executable available at: $(pwd)/$BUILD_DIR/tessa_audio" 