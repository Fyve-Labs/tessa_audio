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

## Python Examples

### Listening to Audio and Saving to WAV File

This example shows how to receive audio data from Tessa Audio and save it to a WAV file:

```python
#!/usr/bin/env python3
import zmq
import json
import wave
import sys
import time
import argparse
from datetime import datetime

def main():
    parser = argparse.ArgumentParser(description="Listen to Tessa Audio and save to WAV file")
    parser.add_argument("--sub-address", default="tcp://localhost:5555", help="ZMQ SUB socket address")
    parser.add_argument("--topic", default="audio", help="ZMQ topic to subscribe to")
    parser.add_argument("--output", default=f"recording_{datetime.now().strftime('%Y%m%d_%H%M%S')}.wav", help="Output WAV file")
    parser.add_argument("--duration", type=int, default=10, help="Recording duration in seconds (0 for infinite)")
    args = parser.parse_args()
    
    # Connect to the ZMQ PUB socket
    context = zmq.Context()
    socket = context.socket(zmq.SUB)
    socket.connect(args.sub_address)
    socket.setsockopt_string(zmq.SUBSCRIBE, args.topic)
    
    print(f"Connecting to {args.sub_address}, topic: {args.topic}")
    print(f"Will record to {args.output} for {args.duration if args.duration > 0 else 'infinite'} seconds")
    
    # Audio parameters will be extracted from the first received message
    sample_rate = None
    channels = None
    format_bytes = None
    wav_file = None
    
    start_time = time.time()
    try:
        while True:
            if args.duration > 0 and (time.time() - start_time) > args.duration:
                break
                
            # Each message consists of 3 parts: topic, metadata, audio data
            packet = socket.recv_multipart()
            topic = packet[0].decode()
            packet_data = json.loads(packet[1].decode())
            audio_data = packet[2]
            # print(f"Packet: {packet_data}, Length: {len(audio_data)}")
            metadata = packet_data['metadata']
            
            # Initialize WAV file on first message
            if wav_file is None:
                sample_rate = metadata.get("sample_rate", 44100)
                channels = metadata.get("channels", 1)
                format_bytes = metadata.get("bit_depth", 16) // 8
                
                print(f"Object: {metadata}")
                print(f"Audio format: {sample_rate} Hz, {channels} channels, {format_bytes*8} bits")
                
                wav_file = wave.open(args.output, 'wb')
                wav_file.setnchannels(channels)
                wav_file.setsampwidth(format_bytes)
                wav_file.setframerate(sample_rate)
            
            # Write audio data to WAV file
            wav_file.writeframes(audio_data)
            sys.stdout.write(".")
            sys.stdout.flush()
            
    except KeyboardInterrupt:
        print("\nRecording stopped")
    finally:
        if wav_file:
            wav_file.close()
            print(f"\nSaved recording to {args.output}")

if __name__ == "__main__":
    main()

```

### Sending a Status Request

This example demonstrates how to send a status request to the Tessa Audio service:

```python
#!/usr/bin/env python3
import zmq
import json
import argparse

def main():
    parser = argparse.ArgumentParser(description="Send a status request to Tessa Audio")
    parser.add_argument("--dealer-address", default="tcp://localhost:5556", help="ZMQ DEALER socket address")
    parser.add_argument("--topic", default="control", help="ZMQ topic for control messages")
    parser.add_argument("--command", default="STATUS", help="Command to send (STATUS, START, STOP, GET_DEVICES)")
    args = parser.parse_args()
    
    # Connect to the ZMQ ROUTER socket using a DEALER socket
    context = zmq.Context()
    socket = context.socket(zmq.DEALER)
    socket.connect(args.dealer_address)
    
    print(f"Connecting to {args.dealer_address}")
    print(f"Sending {args.command} command with topic: {args.topic}")
    
    try:
        # Send command
        socket.send_string("", zmq.SNDMORE)  # Empty delimiter frame
        socket.send_string(args.topic, zmq.SNDMORE)  # Topic
        socket.send_string(args.command)  # Command
        
        # Wait for response
        delimiter = socket.recv_string()  # Empty delimiter
        response_topic = socket.recv_string()  # Response topic
        response = socket.recv_string()  # Response payload
        
        # Pretty print the response
        try:
            json_response = json.loads(response)
            print(json.dumps(json_response, indent=2))
        except json.JSONDecodeError:
            print(f"Response: {response}")
            
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
```

## License

This project is licensed under the Mozilla Public License 2.0 - see the LICENSE file for details.

Developed by Fyve Labs. 