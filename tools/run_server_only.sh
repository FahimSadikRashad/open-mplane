#!/usr/bin/env bash
set -euo pipefail

# Run M-Plane Server with isolated dependencies
# This script runs ONLY the server with its own dependency tree

ROOT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." &>/dev/null && pwd)

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}[Server] Starting M-Plane Server with isolated dependencies${NC}"
echo -e "${YELLOW}[Server] Note: This script requires sudo access for:${NC}"
echo -e "${YELLOW}         - Binding to privileged port 830 (NETCONF)${NC}"
echo -e "${YELLOW}         - Thread creation with elevated privileges${NC}"
echo -e "${YELLOW}         - Sysrepo operations${NC}"
echo ""

# Check if server is built
if [[ ! -f "$ROOT_DIR/build/server-sim/mplane-server-app" ]]; then
    echo -e "${RED}[Error] Server not built. Run: tools/sim/build_sim.sh${NC}"
    exit 1
fi

# Check if HAL shim is built
if [[ ! -f "$ROOT_DIR/mplane_server/utils/test_shim/build/server-test-shim" ]]; then
    echo -e "${RED}[Error] Test shim not built. Run: tools/sim/build_sim.sh${NC}"
    exit 1
fi

# Set SERVER-ONLY library paths
export LD_LIBRARY_PATH="$ROOT_DIR/mplane_server/deps/install/lib64:$ROOT_DIR/mplane_server/deps/install/lib:$ROOT_DIR/build/hal-x86:${LD_LIBRARY_PATH:-}"

# Set YANG module search path
export YANG_MODPATH="$ROOT_DIR/mplane_server/deps/install/share/yang/modules/libyang"

echo -e "${YELLOW}[Server] Library path: $LD_LIBRARY_PATH${NC}"

# Clean up stale socket
SOCK=/tmp/haltest.sock
if [[ -S "$SOCK" ]]; then
  echo -e "${YELLOW}[Server] Removing stale socket: $SOCK${NC}"
  rm -f "$SOCK"
fi

# Clean up sysrepo shared memory
echo -e "${YELLOW}[Server] Cleaning sysrepo shared memory${NC}"
sudo rm -rf /dev/shm/sr_* /dev/shm/srsub_* 2>/dev/null || true

# Create netopeer2 PID file with writable permissions
echo -e "${YELLOW}[Server] Setting up netopeer2 PID file${NC}"
sudo touch /var/run/netopeer2-server.pid 2>/dev/null || true
sudo chown $USER:$USER /var/run/netopeer2-server.pid 2>/dev/null || true
sudo chmod 666 /var/run/netopeer2-server.pid 2>/dev/null || true

# Start the test shim (HAL simulator)
echo -e "${GREEN}[Server] Starting HAL test shim...${NC}"
"$ROOT_DIR/mplane_server/utils/test_shim/build/server-test-shim" &
SHIM_PID=$!
echo $SHIM_PID > /tmp/server-test-shim.pid
echo -e "${GREEN}[Server] Shim PID: $SHIM_PID${NC}"

# Wait for shim to initialize
sleep 2

# Start the server with sudo for privileged operations
echo -e "${GREEN}[Server] Starting mplane-server-app with elevated privileges...${NC}"
sudo -E LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
YANG_MODPATH="$YANG_MODPATH" \
SYSREPO_REPOSITORY_PATH="/etc/sysrepo" \
"$ROOT_DIR/build/server-sim/mplane-server-app" \
    --cfg-data-path "$ROOT_DIR/mplane_server/yang-manager-server/yang-config" \
    --yang-mods-path /usr/share/mplane-server/modules \
    --netopeer-path "$ROOT_DIR/mplane_server/deps/install/bin" \
    --netopeerdbg 2 &
SERVER_PID=$!
echo $SERVER_PID > /tmp/mplane-server-app.pid
echo -e "${GREEN}[Server] Server PID: $SERVER_PID${NC}"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Server started successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "Shim PID: ${YELLOW}$SHIM_PID${NC}"
echo -e "Server PID: ${YELLOW}$SERVER_PID${NC}"
echo ""
echo -e "To stop the server:"
echo -e "  ${YELLOW}kill $SERVER_PID${NC}"
echo -e "  ${YELLOW}kill $SHIM_PID${NC}"
echo ""
echo -e "Or use: ${YELLOW}tools/stop_server.sh${NC}"
echo -e "${GREEN}========================================${NC}"

# Wait for processes
wait
