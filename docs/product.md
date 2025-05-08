## Product Requirements Document: Audio Capture & Streaming Service

**1. Introduction**

This document outlines the requirements for a new service, provisionally named "AudioZMQ," designed to capture audio from a system's specified input device, encode it into a simple binary format, stream the data over a ZMQ publisher socket, and respond to requests received on a separate ZMQ dealer socket. This service will provide a lightweight, flexible, and platform-agnostic way to capture and distribute audio data for various applications, with a strong focus on low latency and seamless audio delivery.

**2. Goals**

* Provide a simple and reliable method for capturing audio from a specific system audio input device.
* Offer a low-latency audio streaming solution using ZMQ.
* Enable remote control and query capabilities through ZMQ requests.
* Minimize resource consumption (CPU and memory footprint), balanced with the need for seamless audio.
* Be easily deployable on a variety of operating systems (Linux, macOS, Windows).
* Ensure continuous audio output, even during brief capture interruptions.

**3. Target Audience**

* Developers building audio analysis tools.
* System integrators requiring remote audio monitoring capabilities.
* Researchers conducting audio-related experiments.
* Users seeking a lightweight alternative to more complex audio streaming solutions.

**4. Product Overview**

AudioZMQ is a command-line tool that captures audio from a specified audio input device, encodes it into a raw binary stream with timecode indexes, and publishes it over a ZMQ PUB socket. It also monitors a separate ZMQ DEALER socket for incoming requests, allowing for remote configuration and status inquiries.

**5. Key Features**

* **Audio Capture:**
    *  Captures audio from a specified audio input device.
        *   `--input-device <device_name>`: Specifies the audio input device to use. If not specified, uses the system default. The implementation must allow for listing available audio devices (via a separate CLI option).
    *  Configurable sample rate (e.g., 44.1kHz, 48kHz).
    *  Configurable number of channels (e.g., Mono, Stereo).
    *  Configurable bit depth (e.g., 16-bit, 32-bit).
    *  Encodes audio into a raw binary format (e.g., PCM).
    *   **Buffering:** Implements a small, configurable buffer to ensure seamless audio output in case of brief capture interruptions.
* **ZMQ Publishing:**
    *  Publishes the captured audio data over a configurable ZMQ PUB socket (TCP or IPC).
    *  `--pub-address <address:port>`: ZMQ PUB socket address including port.
    *  `--pub-topic <topic>`: ZMQ topic name for the published audio stream. If not provided, a default topic (e.g., "audio") will be used.
    *   **Timecode Indexing:** Includes a timestamp (e.g., milliseconds since epoch) with each audio data chunk published to the ZMQ PUB socket. This allows subscribers to reconstruct a continuous audio stream even if packets are lost or arrive out of order.
* **ZMQ Request Handling:**
    *  Listens on a configurable ZMQ DEALER socket (TCP or IPC) for incoming requests.
    *  `--dealer-address <address:port>`: ZMQ DEALER socket address including port.
    *  `--dealer-topic <topic>`: ZMQ topic name to listen for requests on the DEALER socket. If not provided, a default topic (e.g., "control") will be used.
    *  Supports the following request types:
        *  `STATUS`: Returns the current status of the service (e.g., running, sample rate, channels, current timecode).
        *  `SET_SAMPLE_RATE <rate>`: Sets the audio sample rate (e.g., `SET_SAMPLE_RATE 48000`).
        *  `STOP`: Stops the audio capture and streaming.
        *  `START`: Starts the audio capture and streaming.
        *  `GET_DEVICES`: Returns a list of available audio input devices.
* **Command-Line Interface (CLI):**
    *  Provides a CLI for configuring and running the service.
    *  Arguments:
        *  `--input-device <device_name>`: Specifies the audio input device.
        *  `--pub-address <address:port>`: ZMQ PUB socket address and port.
        *  `--pub-topic <topic>`: ZMQ PUB topic.
        *  `--dealer-address <address:port>`: ZMQ DEALER socket address and port.
        *  `--dealer-topic <topic>`: ZMQ DEALER topic.
        *  `--sample-rate <rate>`: Audio sample rate.
        *  `--channels <number>`: Number of audio channels.
        *  `--bit-depth <depth>`: Audio bit depth.
        *  `--buffer-size <size>`: Size of the audio buffer (in milliseconds).
        *   `--list-devices`: Lists available audio input devices and exits.
* **Error Handling:**
    *  Gracefully handles errors, such as invalid configuration parameters, audio device failures, or ZMQ connection issues.
    *  Logs errors to standard error (stderr).
    *  Provides informative error messages.

**6. Out of Scope**

*  Audio encoding beyond raw binary (e.g., MP3, AAC).
*  Graphical User Interface (GUI).
*  Built-in data compression.

**7. Release Criteria**

*  All features listed in section 5 are implemented and functional.
*  The service can capture and stream audio reliably for extended periods with minimal latency and no audio dropouts.
*  The CLI is user-friendly and provides clear error messages.
*  Basic documentation is provided, including installation instructions and example usage.
*  Unit tests cover core functionality, including buffer management and timecode accuracy.

**8. Metrics for Success**

*  Number of downloads and active users.
*  Positive user feedback and reviews, particularly regarding audio quality and latency.
*  Adoption by target audience segments.
*  Measured latency of audio stream (end-to-end).

**9. Future Considerations (Post-MVP)**

*  Support for additional audio encoding formats (e.g., FLAC).
*  Implement data compression (e.g., LZ4).
*  Add support for multiple audio devices.
*  Provide pre-built packages for common operating systems.
*  Integration with popular audio processing libraries.
*  Implement adaptive buffering based on network conditions.

**10. Open Questions**

*  Specific binary format to use for audio data (e.g., PCM little-endian, big-endian).
*  Default values for configuration parameters.
*  Specific error codes and handling strategies.
*  Optimal buffer size for minimizing latency while ensuring seamless audio.
*  Format of the timecode (e.g., milliseconds, nanoseconds).
*  Mechanism for listing audio devices across different operating systems.
