#!/bin/bash
# Sysrepo Initialization Script for Docker
# Mirrors the logic from tools/setup_mplane_server.sh

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

# Directories
ROOT_DIR=/opt/mplane
DEPS_INSTALL=/opt/mplane/deps
YANG_MODULES_DIR=/usr/share/mplane-server/modules
SYSREPO_REPO_DIR=/etc/sysrepo
NETOPEER2_SCRIPTS=/opt/mplane/netopeer2-scripts

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Sysrepo Initialization${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Set environment (matching setup_mplane_server.sh:178-182)
export PATH="${DEPS_INSTALL}/bin:${PATH}"
export LD_LIBRARY_PATH="${DEPS_INSTALL}/lib64:${DEPS_INSTALL}/lib:${LD_LIBRARY_PATH:-}"
export NP2_MODULE_DIR="${YANG_MODULES_DIR}"
export NP2_MODULE_PERMS="600"
export SYSREPO_REPOSITORY_PATH="${SYSREPO_REPO_DIR}"

# Phase 0: Initialize sysrepo repository structure (critical for v1.4.x)
echo -e "${YELLOW}[Setup] Phase 0: Initializing sysrepo repository structure...${NC}"

# Create repository subdirectories (sysrepo v1.4.x expects these)
mkdir -p "${SYSREPO_REPO_DIR}/yang" \
         "${SYSREPO_REPO_DIR}/data" \
         "${SYSREPO_REPO_DIR}/data/notif" \
         "${SYSREPO_REPO_DIR}/data/yang" || {
    echo -e "${RED}[Setup] ✗ Failed to create repository directories${NC}"
    exit 1
}

# Set proper permissions (sysrepo needs write access)
chmod -R 777 "${SYSREPO_REPO_DIR}" 2>/dev/null || {
    echo -e "${YELLOW}[Setup] ! Warning: Could not set all permissions${NC}"
}

# Verify YANG modules source directory and copy to repository
if [ -d "${ROOT_DIR}/yang-models" ]; then
    echo -e "${YELLOW}[Setup] Copying YANG modules to repository...${NC}"
    cp -r "${ROOT_DIR}/yang-models"/* "${YANG_MODULES_DIR}/" 2>/dev/null || true
    chmod -R 644 "${YANG_MODULES_DIR}"/*.yang 2>/dev/null || true
fi

echo -e "${GREEN}[Setup] ✓ Repository structure initialized${NC}"
echo -e "${GREEN}[Setup]   - ${SYSREPO_REPO_DIR}/yang${NC}"
echo -e "${GREEN}[Setup]   - ${SYSREPO_REPO_DIR}/data${NC}"
echo -e "${GREEN}[Setup]   - ${SYSREPO_REPO_DIR}/data/notif${NC}"
echo ""

# Verify tools exist
echo -e "${YELLOW}[Setup] Verifying tools...${NC}"
if [ ! -x "${DEPS_INSTALL}/bin/sysrepoctl" ]; then
    echo -e "${RED}[Error] sysrepoctl not found at ${DEPS_INSTALL}/bin/sysrepoctl${NC}"
    exit 1
fi
echo -e "${GREEN}[Setup] ✓ sysrepoctl found${NC}"

if [ ! -x "${DEPS_INSTALL}/bin/sysrepo-plugind" ]; then
    echo -e "${RED}[Error] sysrepo-plugind not found at ${DEPS_INSTALL}/bin/sysrepo-plugind${NC}"
    exit 1
fi
echo -e "${GREEN}[Setup] ✓ sysrepo-plugind found${NC}"

if [ ! -d "${NETOPEER2_SCRIPTS}" ]; then
    echo -e "${RED}[Error] netopeer2 scripts not found at ${NETOPEER2_SCRIPTS}${NC}"
    exit 1
fi
echo -e "${GREEN}[Setup] ✓ netopeer2 scripts found${NC}"
echo ""

# Critical: Start sysrepo-plugind daemon (required for sysrepoctl to work in v1.4.x)
echo -e "${YELLOW}[Setup] Starting sysrepo-plugind daemon...${NC}"
echo -e "${BLUE}[Setup] (sysrepo v1.4.x requires daemon for sysrepoctl commands)${NC}"

# Kill any existing daemon
killall sysrepo-plugind 2>/dev/null || true
rm -f /var/run/sysrepo-plugind.pid 2>/dev/null || true

# Start daemon in background with verbose logging
${DEPS_INSTALL}/bin/sysrepo-plugind -d -v 3 -p /var/run/sysrepo-plugind.pid

# Wait for daemon to fully initialize
sleep 3

# Verify daemon is running
if ! pgrep -f sysrepo-plugind > /dev/null; then
    echo -e "${RED}[Setup] ✗ Failed to start sysrepo-plugind daemon${NC}"
    exit 1
fi

echo -e "${GREEN}[Setup] ✓ sysrepo-plugind daemon started (PID: $(pgrep -f sysrepo-plugind))${NC}"
echo ""

# Phase 1: Install YANG modules (matching setup_mplane_server.sh:186-197)
echo -e "${YELLOW}[Setup] Phase 1: Installing YANG modules...${NC}"
cd "${NETOPEER2_SCRIPTS}"
if ./setup.sh; then
    echo -e "${GREEN}[Setup] ✓ YANG modules installed${NC}"
else
    echo -e "${RED}[Setup] ✗ Failed to install YANG modules${NC}"
    exit 1
fi
echo ""

# Phase 2: Generate SSH keys (matching setup_mplane_server.sh:199-211)
echo -e "${YELLOW}[Setup] Phase 2: Generating SSH host keys...${NC}"
cd "${NETOPEER2_SCRIPTS}"
if ./merge_hostkey.sh; then
    echo -e "${GREEN}[Setup] ✓ SSH keys generated${NC}"
else
    echo -e "${RED}[Setup] ✗ Failed to generate SSH keys${NC}"
    exit 1
fi
echo ""

# Phase 3: Configure NETCONF endpoint (matching setup_mplane_server.sh:213-225)
echo -e "${YELLOW}[Setup] Phase 3: Configuring NETCONF endpoint (port 830)...${NC}"
cd "${NETOPEER2_SCRIPTS}"
if ./merge_config.sh; then
    echo -e "${GREEN}[Setup] ✓ NETCONF endpoint configured${NC}"
else
    echo -e "${RED}[Setup] ✗ Failed to configure NETCONF endpoint${NC}"
    exit 1
fi
echo ""

# Phase 4: Configure O-RAN users (matching setup_mplane_server.sh:227-239)
echo -e "${YELLOW}[Setup] Phase 4: Configuring O-RAN user management...${NC}"
cd "${ROOT_DIR}/scripts"
if ./o-ran-user-config.sh \
    --sysrepo-path "${DEPS_INSTALL}/bin" \
    --modules "${YANG_MODULES_DIR}"; then
    echo -e "${GREEN}[Setup] ✓ O-RAN users configured${NC}"
else
    echo -e "${RED}[Setup] ✗ Failed to configure O-RAN users${NC}"
    exit 1
fi
echo ""

# Phase 5: Clean up shared memory
echo -e "${YELLOW}[Setup] Phase 5: Cleaning up shared memory...${NC}"
rm -rf /dev/shm/sr_* /dev/shm/srsub_* 2>/dev/null || true
echo -e "${GREEN}[Setup] ✓ Shared memory cleaned${NC}"
echo ""

# Verification
echo -e "${YELLOW}[Setup] Verifying installation...${NC}"
MODULE_COUNT=$(${DEPS_INSTALL}/bin/sysrepoctl -l 2>/dev/null | grep -c "^[[:space:]]*[a-zA-Z]" || echo "0")
echo -e "${GREEN}[Setup] ✓ Installed YANG modules: ${MODULE_COUNT}${NC}"
echo ""

# Stop sysrepo-plugind daemon (will be restarted by main server)
echo -e "${YELLOW}[Setup] Stopping sysrepo-plugind daemon...${NC}"
if pgrep -f sysrepo-plugind > /dev/null; then
    killall sysrepo-plugind 2>/dev/null || true
    sleep 1
    echo -e "${GREEN}[Setup] ✓ Daemon stopped${NC}"
else
    echo -e "${YELLOW}[Setup] ! Daemon already stopped${NC}"
fi
echo ""

echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}  Sysrepo initialization complete!${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Summary:"
echo "  - YANG modules: ${MODULE_COUNT} installed"
echo "  - SSH keys: generated"
echo "  - NETCONF port: 830"
echo "  - O-RAN users: configured"
echo "  - Daemon: stopped (ready for server to start)"
echo ""
