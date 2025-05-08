# Tessa Audio Capture & Streaming

A lightweight, cross-platform audio capture and streaming service that provides low-latency audio capture from system audio devices and streams it over ZeroMQ.

## Features

- Audio capture from any system input device
- Low-latency audio streaming via ZMQ PUB socket
- Remote control through ZMQ ROUTER socket
- Standardized message format with JSON and binary data
- Configurable audio parameters (sample rate, channels, bit depth)
- Seamless audio delivery with buffering
- Environment variable support and .env file loading
- Cross-platform support (macOS and Linux)

## Requirements

- C++17 compatible compiler
- CMake 3.10 or higher
- PortAudio library
- ZeroMQ library
- nlohmann/json (automatically fetched during build)

## Building

### Using Build Script

The easiest way to build the project is using the provided build script:

```bash
# Make the script executable
chmod +x scripts/build.sh

# Run the build script
./scripts/build.sh
```

The script will:
1. Detect your operating system
2. Install necessary dependencies (requires sudo on Linux)
3. Build the project using CMake
4. Place the binary in the `build` directory

### Manual Build

If you prefer to build manually:

1. Install dependencies:
   - **macOS (with Homebrew):**
     ```bash
     brew install portaudio zeromq cppzmq cmake
     ```
   - **Ubuntu/Debian:**
     ```bash
     sudo apt-get install libportaudio2 libportaudio-dev libzmq3-dev cmake build-essential
     ```
   - **CentOS/RHEL/Fedora:**
     ```bash
     sudo yum install portaudio portaudio-devel zeromq zeromq-devel cmake gcc-c++ make
     ```

2. Build with CMake:
   ```bash
   mkdir -p build
   cd build
   cmake -DCMAKE_BUILD_TYPE=Release ..
   make
   ```

## Usage

### Basic Usage

```bash
./build/tessa_audio --pub-address tcp://*:5555 --dealer-address tcp://*:5556
```

### Environment Variables

All command-line options can be set using environment variables. This is useful for containerized environments or when you don't want to specify options on the command line.

Variable names are derived from option names by:
1. Removing leading dashes
2. Converting to uppercase
3. Replacing dashes with underscores

For example:
- `--pub-address` corresponds to `PUB_ADDRESS`
- `--sample-rate` corresponds to `SAMPLE_RATE`

You can set these environment variables directly:

```bash
export PUB_ADDRESS=tcp://*:5555
export DEALER_ADDRESS=tcp://*:5556
./build/tessa_audio
```

### Using .env Files

You can also create a `.env` file with your configuration and load it using the `--env-file` option:

```bash
./build/tessa_audio --env-file /path/to/.env
```

An example `.env` file (`env.example`) is provided in the repository. Copy it to `.env` and modify as needed:

```bash
cp env.example .env
# Edit .env with your preferred editor
```

### Command Line Options

Command line options take precedence over environment variables.

- `--input-device <device_name>`: Audio input device name (uses default if not specified)
- `--pub-address <address:port>`: ZMQ PUB socket address (e.g., tcp://*:5555)
- `--pub-topic <topic>`: ZMQ PUB topic (default: audio)
- `--dealer-address <address:port>`: ZMQ DEALER socket address (e.g., tcp://*:5556)
- `--dealer-topic <topic>`: ZMQ DEALER topic (default: control)
- `--service-name <name>`: Service name for messages (default: tessa_audio)
- `--stream-id <id>`: Stream ID for messages (optional)
- `--sample-rate <rate>`: Audio sample rate (default: 44100)
- `--channels <number>`: Number of audio channels (default: 2)
- `--bit-depth <depth>`: Audio bit depth (default: 16)
- `--buffer-size <size>`: Audio buffer size in ms (default: 100)
- `--echo-status`: Echo status messages to stdout
- `--list-devices`: List available audio devices and exit
- `--env-file <file>`: Load environment variables from file
- `--help`: Show help message

### Available Commands

The service accepts the following commands on the DEALER socket:

- `STATUS`: Returns the current status of the service
- `SET_SAMPLE_RATE <rate>`: Sets the audio sample rate
- `STOP`: Stops the audio capture and streaming
- `START`: Starts the audio capture and streaming
- `GET_DEVICES`: Returns a list of available audio input devices
- `SET_ECHO_STATUS <on|off>`: Enable or disable status echo to stdout

## Message Format

The service uses a standardized message format for all communication:

### Audio Data Messages

Audio data is published as a multi-part ZMQ message with the following frames:
1. Topic frame: contains the topic string (e.g., "audio")
2. JSON frame: contains the message metadata in JSON format
3. Binary frame: contains the raw audio data

The JSON format follows this structure:
```json
{
  "message_type": "data",
  "timestamp": "2023-10-27T15:30:45.123Z",
  "service": "tessa_audio",
  "stream_id": "optional_stream_id",
  "metadata": {
    "unix_timestamp_ms": 1679890123456
  }
}
```

### Status Messages

Status messages are published on the same PUB socket as audio data, with the following format:
```json
{
  "message_type": "status",
  "timestamp": "2023-10-27T15:30:45.123Z",
  "service": "tessa_audio",
  "stream_id": "optional_stream_id",
  "status": {
    "running": true,
    "sample_rate": 44100,
    "channels": 2,
    "bit_depth": 16,
    "device": "Default Input Device",
    "event": "started"
  }
}
```

## Example Clients

### Python Example - Subscribe to Audio and Status Messages

```python
import zmq
import json
import time

context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect("tcp://127.0.0.1:5555")
socket.setsockopt_string(zmq.SUBSCRIBE, "audio")

print("Listening for audio and status data...")
while True:
    # Receive topic
    topic = socket.recv_string()
    
    # Receive JSON message
    json_str = socket.recv_string()
    msg_data = json.loads(json_str)
    
    if msg_data["message_type"] == "data":
        # For data messages, receive the binary payload
        audio_data = socket.recv()
        print(f"Received {len(audio_data)} bytes of audio data with timestamp {msg_data['timestamp']}")
    elif msg_data["message_type"] == "status":
        # Status messages don't have a binary payload
        print(f"Status update: {json_str}")
```

### Python Example - Send Command

```python
import zmq
import time

context = zmq.Context()
socket = context.socket(zmq.DEALER)
socket.connect("tcp://127.0.0.1:5556")

# Send STATUS command
socket.send_multipart([b"control", b"STATUS"])

# Receive response
response = socket.recv_multipart()
print(f"Response: {response[1].decode()}")
```

## License

This project is licensed under the MIT License - see the LICENSE file for details. 