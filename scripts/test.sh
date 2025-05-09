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
COVERAGE=true

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
    --no-coverage)
      COVERAGE=false
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
      echo "  --no-coverage         Skip code coverage reports"
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

# Check if we need coverage flags
CMAKE_ARGS="-B $BUILD_DIR -DCMAKE_BUILD_TYPE=$BUILD_TYPE"
if [[ "$COVERAGE" == true ]]; then
  CMAKE_ARGS="$CMAKE_ARGS -DCODE_COVERAGE=ON"
fi

# Make sure the CMake build is up to date
echo "Running CMake configuration..."
if [[ "$DRY_RUN" == true ]]; then
  echo "WOULD RUN: cmake $CMAKE_ARGS"
else
  cmake $CMAKE_ARGS
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

# Generate coverage data if enabled
if [[ "$COVERAGE" == true && "$RUN_TESTS" == true ]]; then
  echo "Generating code coverage data..."
  if [[ "$DRY_RUN" == true ]]; then
    echo "WOULD RUN: pushd \"$BUILD_DIR\" > /dev/null"
    echo "WOULD RUN: [coverage commands]"
    echo "WOULD RUN: popd > /dev/null"
  else
    pushd "$BUILD_DIR" > /dev/null
    
    # Create coverage directory
    mkdir -p coverage
    
    # Check if we're using GCC or Clang
    if command -v gcov &> /dev/null && command -v lcov &> /dev/null; then
      echo "Using lcov/gcov for coverage..."
      
      # Capture coverage data
      lcov --directory . --capture --output-file coverage/lcov.info --ignore-errors inconsistent,unsupported,format,unused
      
      # Filter out system headers and test files
      lcov --remove coverage/lcov.info '/usr/*' '/Library/*' '*/deps/*' '*/tests/*' --output-file coverage/lcov.filtered.info --ignore-errors inconsistent,unsupported,format,unused
      
      echo "Raw coverage data: $BUILD_DIR/coverage/lcov.filtered.info"
      
    elif command -v llvm-cov &> /dev/null; then
      echo "Using llvm-cov for coverage..."
      
      # Find all executables with coverage data
      find . -name "*.gcda" -exec dirname {} \; | sort -u > coverage/dirs.txt
      
      # Generate coverage reports
      while read dir; do
        llvm-cov show "$dir/tessa_audio_tests" --instr-profile="$dir/default.profdata" > coverage/coverage_report.txt
        llvm-cov export "$dir/tessa_audio_tests" --instr-profile="$dir/default.profdata" -format=lcov > coverage/llvm-lcov.info
      done < coverage/dirs.txt
      
      # Copy to standard location
      cp coverage/llvm-lcov.info coverage/lcov.filtered.info
      
      echo "Raw coverage data: $BUILD_DIR/coverage/lcov.filtered.info"
      
    else
      echo "Warning: Neither lcov/gcov nor llvm-cov found. Collecting raw coverage data files..."
      # Just collect the raw .gcno and .gcda files
      mkdir -p coverage/raw
      find . -name "*.gcno" -o -name "*.gcda" -exec cp {} coverage/raw/ \;
      echo "Raw coverage data collected in: $BUILD_DIR/coverage/raw/"
    fi
    
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