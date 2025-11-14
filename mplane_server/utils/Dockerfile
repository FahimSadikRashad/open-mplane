# Use Ubuntu 20.04 as base
FROM ubuntu:20.04

LABEL maintainer="mplane-developer@example.com"
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    wget \
    pkg-config \
    libpcre3-dev \
    libpcre2-dev \
    libssl-dev \
    libsystemd-dev \
    libtool \
    autoconf \
    libcmocka-dev \
    python3-dev \
    python3-pip \
    libcurl4-openssl-dev \
    libpam0g-dev \
    && rm -rf /var/lib/apt/lists/*

# Working directory
WORKDIR /opt/mplane

# Copy scripts
COPY get_deps_server.sh build_deps_server.sh ./

# Make scripts executable
RUN chmod +x get_deps_server.sh build_deps_server.sh

# Create M-Plane server directory
RUN mkdir -p mplane_server

# Download dependencies
RUN ./get_deps_server.sh --no-fwdproxy --dir /opt/mplane/mplane_server

# Build dependencies
RUN ./build_deps_server.sh --dir /opt/mplane/mplane_server

# Verification: list install folder
RUN ls -la /opt/mplane/mplane_server/deps/install

# Interactive entrypoint
CMD ["bash"]
