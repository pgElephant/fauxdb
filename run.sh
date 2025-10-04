#!/bin/bash

# FauxDB Run Script
# Starts the FauxDB server with proper configuration

echo "Starting FauxDB Server..."
echo "============================="

# Check if PostgreSQL is running
if ! systemctl is-active --quiet postgresql; then
    echo "Starting PostgreSQL..."
    sudo systemctl start postgresql
fi

# Check if MongoDB test container is running
if ! podman ps | grep -q mongodb-test; then
    echo "Starting MongoDB test container..."
    podman run -d --name mongodb-test -p 27017:27017 docker.io/mongo:latest
    sleep 3
fi

# Start FauxDB
echo "Starting FauxDB server on port 27018..."
cd /home/pgedge/pge/fauxdb
RUSTUP_TOOLCHAIN=stable /home/pgedge/.rustup/toolchains/stable-aarch64-unknown-linux-gnu/bin/cargo run --release -- --config config/fauxdb.toml
