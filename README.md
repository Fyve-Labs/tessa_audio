# Tessa Audio (AudioZMQ)

A cross-platform audio capture and streaming service based on PortAudio and ZeroMQ.

## Features

- Audio capture from system input devices using PortAudio
- Low-latency streaming over ZeroMQ
- Cross-platform support (macOS, Linux)
- Remote control via ZMQ ROUTER/DEALER sockets
- Configurable audio parameters (sample rate, channels, bit depth)
- Environment variable configuration
- Automatic versioning and releases via GitHub Actions

## Requirements

- PortAudio
- ZeroMQ
- C++17 compatible compiler
- CMake 3.10+

## Building

### Using the Build Script

The easiest way to build Tessa Audio is to use the provided build script:

```bash
# Basic build
./scripts/build.sh

# Build with options
./scripts/build.sh --build-type Debug --no-tests

# See all available options
./scripts/build.sh --help
```

Options:
- `--build-type TYPE` - Set build type (Debug, Release) [default: Release]
- `--build-dir DIR` - Set build directory [default: build]
- `--no-tests` - Skip running tests
- `--update-version VER` - Update version in CMakeLists.txt (e.g., 1.0.0)
- `--dry-run` - Show commands without executing them
- `--help` - Show help message

### Manual Build

```bash
# Create build directory
mkdir -p build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Run tests (optional)
ctest -C Release
```

## Usage

```bash
# Show help
./build/tessa_audio --help

# List available audio devices
./build/tessa_audio --list-devices

# Run with specific device and addresses
./build/tessa_audio --input-device "Built-in Microphone" \
                    --pub-address tcp://*:5555 \
                    --dealer-address tcp://*:5556
```

## Environment Variables

All command-line options can be set via environment variables:

```bash
# Set environment variables
export INPUT_DEVICE="Built-in Microphone"
export PUB_ADDRESS="tcp://*:5555"
export SAMPLE_RATE=48000

# Run the application (will use the above environment variables)
./build/tessa_audio
```

You can also use a `.env` file to set environment variables:

```bash
# Create .env file
echo "INPUT_DEVICE=Built-in Microphone" > .env
echo "SAMPLE_RATE=48000" >> .env

# Run with .env file
./build/tessa_audio --env-file .env
```

## License

This project is licensed under the Mozilla Public License 2.0 - see the LICENSE file for details.

Developed by Fyve Labs. 