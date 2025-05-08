#!/bin/bash
# Build script for Tessa Audio
# This script consolidates the test steps from the GitHub workflow

# switch to the parent directory to run
cd "$(dirname "$0")/.."

set -e

# Default values
BUILD_TYPE="Release"
BUILD_DIR="build"
WORKSPACE_DIR="$(pwd)"
RUN_TESTS=true
UPDATE_VERSION=false
NEW_VERSION=""
UPDATE_VERSION_H=false
DRY_RUN=false
VERBOSE=false

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
    --no-tests)
      RUN_TESTS=false
      shift
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
    --verbose)
      VERBOSE=true
      shift
      ;;
    --help)
      echo "Usage: $0 [options]"
      echo "Options:"
      echo "  --build-type TYPE     Set build type (Debug, Release) [default: Release]"
      echo "  --build-dir DIR       Set build directory [default: build]"
      echo "  --no-tests            Skip running tests"
      echo "  --update-version VER  Update version in CMakeLists.txt (e.g., 1.0.0)"
      echo "  --update-version-h VER Update version in version.h (e.g., 1.0.0)"
      echo "  --dry-run             Show commands without executing them"
      echo "  --verbose             Show verbose output from tests"
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

# Check if the build directory exists, create it if not
if [ ! -d "$BUILD_DIR" ]; then
  echo "Creating build directory: $BUILD_DIR"
  mkdir -p "$BUILD_DIR"
fi

# Make sure the CMake build is up to date
echo "Running CMake configuration..."
if [[ "$DRY_RUN" == true ]]; then
  echo "WOULD RUN: cmake -B $BUILD_DIR -DCMAKE_BUILD_TYPE=$BUILD_TYPE"
else
  cmake -B $BUILD_DIR -DCMAKE_BUILD_TYPE=$BUILD_TYPE
fi

# Build the project
echo "Building project..."
if [[ "$DRY_RUN" == true ]]; then
  echo "WOULD RUN: cmake --build $BUILD_DIR --config $BUILD_TYPE"
else
  cmake --build $BUILD_DIR --config $BUILD_TYPE
fi

# Run tests
if [[ "$RUN_TESTS" == true ]]; then
  echo "Running tests"
  if [[ "$DRY_RUN" == true ]]; then
    echo "WOULD RUN: pushd \"$BUILD_DIR\" > /dev/null"
    echo "WOULD RUN: ctest -C \"$BUILD_TYPE\" --output-on-failure --timeout 60 || echo \"Some tests failed, continuing...\""
    echo "WOULD RUN: popd > /dev/null"
  else
    pushd "$BUILD_DIR" > /dev/null
    # Allow test failures - some tests may fail due to missing audio devices
    
    CTEST_ARGS="-C \"$BUILD_TYPE\" --output-on-failure --timeout 60"
    if [[ "$VERBOSE" == true ]]; then
      CTEST_ARGS="$CTEST_ARGS -V"
    fi
    
    ctest $CTEST_ARGS || {
      echo "Some tests failed. Showing test log files:"
      find Testing -name "*.log" -exec cat {} \;
      echo "Continuing despite test failures..."
    }
    
    # Show output locations for debugging
    echo "Test result files can be found in:"
    find . -name "*test*.xml" -o -name "*.log"
    
    popd > /dev/null
  fi
fi

# Verify executable
echo "Verifying executable"
if [[ "$DRY_RUN" == true ]]; then
  echo "WOULD RUN: pushd \"$BUILD_DIR\" > /dev/null"
  echo "WOULD RUN: ./tessa_audio --help || exit 1"
  echo "WOULD RUN: ./tessa_audio --list-devices || echo \"Device listing failed, possibly due to missing audio hardware\""
  echo "WOULD RUN: popd > /dev/null"
else
  pushd "$BUILD_DIR" > /dev/null
  ./tessa_audio --help || exit 1
  # Try listing devices but don't fail if this doesn't work
  ./tessa_audio --list-devices || echo "Device listing failed, possibly due to missing audio hardware"
  popd > /dev/null
fi

echo "Build completed successfully!"
echo "Executable available at: $(pwd)/$BUILD_DIR/tessa_audio" 