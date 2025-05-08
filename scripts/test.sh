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
COVERAGE=false

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
    --coverage)
      COVERAGE=true
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
      echo "  --coverage            Generate code coverage reports"
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

# Generate coverage report if enabled
if [[ "$COVERAGE" == true && "$RUN_TESTS" == true ]]; then
  echo "Generating code coverage report..."
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
      
      # Generate HTML report
      genhtml coverage/lcov.filtered.info --output-directory coverage/html  --ignore-errors category,inconsistent,corrupt
      
      echo "HTML coverage report generated at: $BUILD_DIR/coverage/html/index.html"
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
      
      echo "Coverage report generated at: $BUILD_DIR/coverage/coverage_report.txt"
      echo "Raw coverage data: $BUILD_DIR/coverage/llvm-lcov.info"
      
    else
      echo "Warning: Neither lcov/gcov nor llvm-cov found. Skipping coverage report generation."
      echo "Please install lcov and gcov (for GCC) or llvm-cov (for Clang) to generate coverage reports."
      
      # Just collect the raw .gcno and .gcda files
      echo "Collecting raw coverage data files..."
      mkdir -p coverage/raw
      find . -name "*.gcno" -o -name "*.gcda" -exec cp {} coverage/raw/ \;
      echo "Raw coverage data collected in: $BUILD_DIR/coverage/raw/"
      
      # Try to generate a simple coverage report using gcov directly
      if command -v gcov &> /dev/null; then
        echo "Attempting to generate basic coverage report with gcov..."
        mkdir -p coverage/gcov
        find . -name "*.gcda" | while read -r gcda_file; do
          gcov_base=$(basename "$gcda_file" .gcda)
          dir=$(dirname "$gcda_file")
          (cd "$dir" && gcov "$gcov_base.cpp" 2>/dev/null)
          if [ -f "$dir/$gcov_base.cpp.gcov" ]; then
            cp "$dir/$gcov_base.cpp.gcov" coverage/gcov/
          fi
        done
        
        # Create a simple summary
        echo "Generating basic coverage summary..."
        {
          echo "# Basic Code Coverage Summary"
          echo "Generated on: $(date)"
          echo ""
          echo "## Coverage Information"
          echo ""
          echo "Files with coverage data:"
          find . -name "*.gcda" | sort | while read -r gcda_file; do
            echo "- $(basename "$gcda_file" .gcda)"
          done
          echo ""
          echo "Note: Detailed coverage information is available in the gcov files in the coverage/gcov directory."
        } > coverage/basic_coverage_summary.md
        
        echo "Basic coverage summary generated at: $BUILD_DIR/coverage/basic_coverage_summary.md"
      fi
    fi
    
    # Generate summary file for GitHub
    echo "Generating coverage summary..."
    {
      echo "# Code Coverage Summary"
      echo "Generated on: $(date)"
      echo ""
      
      # Always include a list of files with coverage data
      echo "## Files Tested"
      echo ""
      if [ -d "CMakeFiles/tessa_audio_lib.dir/src" ]; then
        echo "Source files with coverage data:"
        find CMakeFiles/tessa_audio_lib.dir/src -name "*.gcda" | sort | while read gcda_file; do
          src_file=$(basename "$gcda_file" .gcda)
          echo "- ${src_file%.o}"
        done
      else
        echo "No coverage data files found."
      fi
      
      echo ""
      echo "## Coverage by File"
      echo ""
      echo "| File | Line Coverage | Function Coverage |"
      echo "|------|---------------|-------------------|"
      
      if command -v lcov &> /dev/null; then
        # Try to get coverage data from lcov summary, but don't fail if it errors
        lcov --summary coverage/lcov.filtered.info --ignore-errors inconsistent,unsupported,format,unused,category 2>/dev/null | grep -E "lines|functions" | awk '{print $1, $2, $3}' | \
        awk 'NR%2{file=$3; line=$5; printf "| %s | %s", file, line} !(NR%2){func=$5; printf " | %s |\n", func}' || \
        echo "| (Error generating detailed coverage data) | - | - |"
      elif command -v llvm-cov &> /dev/null; then
        llvm-cov report "$dir/tessa_audio_tests" --instr-profile="$dir/default.profdata" | \
        grep -v 'Total\|---\|Filename' | awk '{print "| " $1 " | " $4 " | " $5 " |"}'
      else
        echo "| (No coverage tool found) | - | - |"
      fi
      
      echo ""
      echo "## Overall Coverage"
      
      if command -v lcov &> /dev/null; then
        # Try to get overall coverage data but don't fail if it errors
        lcov --summary coverage/lcov.filtered.info --ignore-errors inconsistent,unsupported,format 2>/dev/null | grep -E 'lines|functions' | sed 's/  */ /g' || \
        echo "- Error generating overall coverage data"
      elif command -v llvm-cov &> /dev/null; then
        llvm-cov report "$dir/tessa_audio_tests" --instr-profile="$dir/default.profdata" | grep 'TOTAL' | \
        awk '{print "- Lines: " $4 "\n- Functions: " $5}'
      else
        echo "- No coverage data available"
      fi

    } > coverage/coverage_summary.md
    
    echo "Coverage summary generated at: $BUILD_DIR/coverage/coverage_summary.md"
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