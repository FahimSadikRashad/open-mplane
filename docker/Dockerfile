# Multi-Stage Dockerfile for O-RAN M-Plane Simulation
# This Dockerfile builds the complete simulation in isolated stages
# Follows the exact process from tools/sim/build_sim.sh

# =============================================================================
# Stage 1: Build Dependencies (libyang, sysrepo, netopeer2, etc.)
# =============================================================================
FROM ubuntu:20.04 AS deps-builder

LABEL maintainer="mplane-developer@example.com"
LABEL description="O-RAN M-Plane Server Dependencies Builder"
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies (matching SIMULATION_BUILD_AND_RUN_GUIDE.md)
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
    zlib1g-dev \
    curl \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/mplane

# Copy dependency build scripts (EXACT same as mplane_server/utils/Dockerfile)
COPY mplane_server/utils/get_deps_server.sh \
     mplane_server/utils/build_deps_server.sh \
     ./utils/

RUN chmod +x ./utils/*.sh

# Download dependencies (libssh, libyang, sysrepo, netopeer2, tinyxml2)
RUN ./utils/get_deps_server.sh --no-fwdproxy --dir /opt/mplane/mplane_server

# Build all dependencies (includes netopeer2 - no patches needed)
RUN ./utils/build_deps_server.sh --dir /opt/mplane/mplane_server

# Verify installation
RUN ls -la /opt/mplane/mplane_server/deps/install && \
    test -f /opt/mplane/mplane_server/deps/install/bin/sysrepoctl && \
    test -f /opt/mplane/mplane_server/deps/install/bin/netopeer2-server && \
    echo "[Stage 1] ✓ Dependencies built successfully!"

# =============================================================================
# Stage 2: Build Simulation (halmplane, test-shim, mplane-server-app)
# =============================================================================
FROM deps-builder AS server-builder

LABEL description="O-RAN M-Plane Server Builder"

# Copy all source code
COPY libhalmplane/ /opt/mplane/libhalmplane/
COPY mplane_server/ /opt/mplane/mplane_server/
COPY tools/ /opt/mplane/tools/

# Set build environment (matching run_server_only.sh)
ENV ROOT_DIR=/opt/mplane
ENV DEPS_INSTALL=/opt/mplane/mplane_server/deps/install
ENV LD_LIBRARY_PATH=${DEPS_INSTALL}/lib64:${DEPS_INSTALL}/lib
ENV PKG_CONFIG_PATH=${DEPS_INSTALL}/lib64/pkgconfig:${DEPS_INSTALL}/lib/pkgconfig
ENV CMAKE_PREFIX_PATH=${DEPS_INSTALL}

# Build halmplane (x86) - EXACT parameters from build_sim.sh:9-11
RUN echo "[Stage 2] Building halmplane (x86)..." && \
    cmake -S "${ROOT_DIR}/libhalmplane" \
          -B "${ROOT_DIR}/build/hal-x86" \
          -DCONTEXT=YOCTO \
          -DBUILD_BOARD=x86 \
          -DBUILD_BOARD_STYLE=MONO \
          -DCMAKE_BUILD_TYPE=RelWithDebInfo && \
    cmake --build "${ROOT_DIR}/build/hal-x86" -j$(nproc) && \
    echo "[Stage 2] ✓ halmplane built successfully!"

# Build server-test-shim - EXACT parameters from build_sim.sh:13-27
RUN echo "[Stage 2] Building server-test-shim..." && \
    cmake -S "${ROOT_DIR}/mplane_server/utils/test_shim" \
          -B "${ROOT_DIR}/mplane_server/utils/test_shim/build" \
          -DHALMPLANE_LIB="${ROOT_DIR}/build/hal-x86/libhalmplane.so" \
          -DHALMPLANE_INCLUDE_DIR="${ROOT_DIR}/libhalmplane/inc" \
          -DHALMPLANE_X86_INCLUDE_DIR="${ROOT_DIR}/libhalmplane/x86/inc" \
          -DHALMPLANE_BUILD_INCLUDE_DIR="${ROOT_DIR}/build/hal-x86" && \
    cmake --build "${ROOT_DIR}/mplane_server/utils/test_shim/build" -j$(nproc) && \
    echo "[Stage 2] ✓ server-test-shim built successfully!"

# Build mplane-server-app - EXACT parameters from build_sim.sh:33-44
RUN echo "[Stage 2] Building mplane-server-app with HAL_TEST..." && \
    cmake -S "${ROOT_DIR}/mplane_server" \
          -B "${ROOT_DIR}/build/server-sim" \
          -DHAL_TEST=ON \
          -DCMAKE_BUILD_TYPE=RelWithDebInfo \
          -DHALMPLANE="${ROOT_DIR}/build/hal-x86/libhalmplane.so" \
          -DSYSREPO-CPP="${DEPS_INSTALL}/lib/libsysrepo-cpp.so" \
          -DYANG-CPP="${DEPS_INSTALL}/lib/libyang-cpp.so" \
          -DCMAKE_PREFIX_PATH="${DEPS_INSTALL}" \
          -DCMAKE_INCLUDE_PATH="${DEPS_INSTALL}/include" \
          -DCMAKE_LIBRARY_PATH="${DEPS_INSTALL}/lib" && \
    cmake --build "${ROOT_DIR}/build/server-sim" -j$(nproc) && \
    echo "[Stage 2] ✓ mplane-server-app built successfully!"

# Verify all binaries exist
RUN echo "[Stage 2] Verifying binaries:" && \
    ls -la "${ROOT_DIR}/mplane_server/utils/test_shim/build/server-test-shim" && \
    ls -la "${ROOT_DIR}/build/server-sim/mplane-server-app" && \
    ls -la "${ROOT_DIR}/build/hal-x86/libhalmplane.so" && \
    echo "[Stage 2] ✓ All binaries verified!"

# =============================================================================
# Stage 3: Runtime Container (minimal, production-ready)
# =============================================================================
FROM ubuntu:20.04 AS runtime

LABEL maintainer="mplane-developer@example.com"
LABEL description="O-RAN M-Plane Server Runtime"
LABEL version="1.0"
ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies only (matching system requirements)
RUN apt-get update && apt-get install -y --no-install-recommends \
    libssl1.1 \
    libpcre2-8-0 \
    libsystemd0 \
    libcurl4 \
    libpam0g \
    zlib1g \
    openssh-client \
    sudo \
    procps \
    net-tools \
    iproute2 \
    && rm -rf /var/lib/apt/lists/*

# Copy built artifacts from server-builder stage
COPY --from=server-builder /opt/mplane/mplane_server/deps/install /opt/mplane/deps/
COPY --from=server-builder /opt/mplane/build/hal-x86/libhalmplane.so /opt/mplane/lib/
COPY --from=server-builder /opt/mplane/mplane_server/utils/test_shim/build/server-test-shim /opt/mplane/bin/
COPY --from=server-builder /opt/mplane/build/server-sim/mplane-server-app /opt/mplane/bin/

# Copy YANG modules and configuration
COPY --from=server-builder /opt/mplane/mplane_server/yang-models /opt/mplane/yang-models/
COPY --from=server-builder /opt/mplane/mplane_server/yang-manager-server/yang-config /opt/mplane/yang-config/
COPY --from=server-builder /opt/mplane/mplane_server/scripts /opt/mplane/scripts/

# Copy netopeer2 scripts (required for sysrepo setup)
COPY --from=server-builder /opt/mplane/mplane_server/deps/netopeer2/build /opt/mplane/netopeer2-scripts/

# Copy Docker runtime scripts 
COPY docker/scripts/docker-entrypoint.sh /opt/mplane/docker-scripts/
COPY docker/scripts/init-sysrepo.sh /opt/mplane/docker-scripts/
RUN chmod +x /opt/mplane/docker-scripts/*.sh

# Set environment variables (exact match with run_server_only.sh:35-38)
ENV ROOT_DIR=/opt/mplane
ENV DEPS_INSTALL=/opt/mplane/deps
ENV LD_LIBRARY_PATH=${DEPS_INSTALL}/lib64:${DEPS_INSTALL}/lib:/opt/mplane/lib
ENV YANG_MODPATH=${DEPS_INSTALL}/share/yang/modules/libyang
ENV PATH=${DEPS_INSTALL}/bin:${PATH}

# Create required directories
RUN mkdir -p /etc/sysrepo \
             /usr/share/mplane-server/modules \
             /var/log \
             /var/run \
             /dev/shm && \
    touch /var/log/console.log && \
    touch /var/log/app-state && \
    chmod 666 /var/log/console.log && \
    chmod 666 /var/log/app-state && \
    chown -R root:root /etc/sysrepo && \
    chown -R root:root /usr/share/mplane-server/modules && \
    chmod -R 775 /etc/sysrepo && \
    chmod -R 775 /usr/share/mplane-server/modules

# Create volumes for persistent data
VOLUME ["/etc/sysrepo", "/var/log", "/opt/mplane/config"]

# Expose NETCONF port
EXPOSE 830

WORKDIR /opt/mplane

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=40s --retries=3 \
    CMD pgrep -f mplane-server-app > /dev/null || exit 1

# Use entrypoint script
ENTRYPOINT ["/opt/mplane/docker-scripts/docker-entrypoint.sh"]
CMD ["server"]
