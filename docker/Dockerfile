# Multi-stage build for FauxDB
FROM rust:1.70-slim as builder

# Install system dependencies
RUN apt-get update && apt-get install -y \
    pkg-config \
    libssl-dev \
    libpq-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy Cargo files first for better caching
COPY Cargo.toml Cargo.lock ./

# Create a dummy main.rs to build dependencies
RUN mkdir src && echo "fn main() {}" > src/main.rs

# Build dependencies
RUN cargo build --release

# Remove dummy main.rs and copy actual source code
RUN rm -rf src
COPY src ./src
COPY config ./config

# Build the actual application
RUN cargo build --release

# Runtime stage
FROM debian:bookworm-slim

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    libpq5 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN groupadd -r fauxdb && useradd -r -g fauxdb fauxdb

# Set working directory
WORKDIR /app

# Copy binary from builder stage
COPY --from=builder /app/target/release/fauxdb /usr/local/bin/fauxdb

# Copy configuration
COPY --from=builder /app/config ./config

# Create data directory
RUN mkdir -p /app/data && chown -R fauxdb:fauxdb /app

# Switch to non-root user
USER fauxdb

# Expose port
EXPOSE 27018

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD mongosh --host localhost --port 27018 --eval "db.runCommand({ping: 1})" || exit 1

# Run FauxDB
CMD ["fauxdb"]
