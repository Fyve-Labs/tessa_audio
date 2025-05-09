# Builder stage
FROM debian:bullseye AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++ \
    libportaudio2 \
    libportaudiocpp0 \
    portaudio19-dev \
    libzmq3-dev \
    git \
    lcov \
    && rm -rf /var/lib/apt/lists/*

# Set build arguments
ARG BUILD_TYPE=Release
ENV BUILD_TYPE=${BUILD_TYPE}
# Optional flag to run tests (--with-tests)
ARG TESTING_FLAG=""
ENV TESTING_FLAG="${TESTING_FLAG}"

# Create app directory
WORKDIR /app

# Copy source code
COPY . .

RUN if [ -n "${TESTING_FLAG}" ]; then ./scripts/test.sh --build-type ${BUILD_TYPE}; else ./scripts/build.sh --build-type ${BUILD_TYPE}; fi

# Final stage
FROM debian:bullseye-slim AS runner

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libportaudio2 \
    libzmq3-dev \
    && rm -rf /var/lib/apt/lists/*

# Create app directory
WORKDIR /app

# Copy the built binary and necessary files from builder
COPY --from=builder /app/build/tessa_audio /app/
COPY --from=builder /app/build/coverage* /app/
COPY --from=builder /app/LICENSE /app/
COPY --from=builder /app/env.example /app/

# Set environment variables
ENV PUB_ADDRESS="tcp://*:5555"
ENV DEALER_ADDRESS="tcp://*:5556"
ENV VERBOSE="true"

# Expose ports
EXPOSE 5555 5556

# Set entry point
CMD ["/app/tessa_audio", "--verbose"] 