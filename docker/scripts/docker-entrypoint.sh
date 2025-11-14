#!/bin/bash
# Docker Entrypoint Script for M-Plane Server
# Mirrors the behavior of tools/run_server_only.sh

set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Directories
ROOT_DIR=/opt/mplane
DEPS_INSTALL=/opt/mplane/deps

# Ensure environment variables are set (matching run_server_only.sh:35-38)
export LD_LIBRARY_PATH="${DEPS_INSTALL}/lib64:${DEPS_INSTALL}/lib:/opt/mplane/lib:${LD_LIBRARY_PATH:-}"
export YANG_MODPATH="${DEPS_INSTALL}/share/yang/modules/libyang"
export PATH="${DEPS_INSTALL}/bin:${PATH}"
export SYSREPO_REPOSITORY_PATH="/etc/sysrepo"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  O-RAN M-Plane Server (Docker)${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Initialize sysrepo on first run
if [ ! -f /etc/sysrepo/.initialized ]; then
    echo -e "${GREEN}[Docker] First run detected - initializing sysrepo...${NC}"
    /opt/mplane/docker-scripts/init-sysrepo.sh
    touch /etc/sysrepo/.initialized
    echo -e "${GREEN}[Docker] ✓ Sysrepo initialization complete!${NC}"
    echo ""
fi

# Clean up stale resources (matching run_server_only.sh:43-57)
echo -e "${YELLOW}[Docker] Cleaning up stale resources...${NC}"

# Remove stale HAL socket
SOCK=/tmp/haltest.sock
if [[ -S "$SOCK" ]]; then
    echo -e "${YELLOW}[Docker] Removing stale socket: $SOCK${NC}"
    rm -f "$SOCK"
fi

# Clean sysrepo shared memory
echo -e "${YELLOW}[Docker] Cleaning sysrepo shared memory${NC}"
rm -rf /dev/shm/sr_* /dev/shm/srsub_* 2>/dev/null || true

# Setup netopeer2 PID file
echo -e "${YELLOW}[Docker] Setting up netopeer2 PID file${NC}"
touch /var/run/netopeer2-server.pid 2>/dev/null || true
chmod 666 /var/run/netopeer2-server.pid 2>/dev/null || true

echo ""

# Handle different startup modes
case "$1" in
    server)
        echo -e "${GREEN}[Docker] Starting M-Plane Server...${NC}"
        echo -e "${YELLOW}[Docker] Library path: $LD_LIBRARY_PATH${NC}"
        echo ""

        # Start HAL test shim in background (matching run_server_only.sh:60-64)
        echo -e "${GREEN}[Docker] Starting HAL test shim...${NC}"
        ${ROOT_DIR}/bin/server-test-shim &
        SHIM_PID=$!
        echo $SHIM_PID > /tmp/server-test-shim.pid
        echo -e "${GREEN}[Docker] ✓ Shim PID: ${SHIM_PID}${NC}"

        # Wait for shim to initialize
        sleep 2

        # Start mplane-server-app (matching run_server_only.sh:70-80)
        echo -e "${GREEN}[Docker] Starting mplane-server-app...${NC}"
        echo -e "${BLUE}========================================${NC}"
        echo -e "${BLUE}  Server is starting...${NC}"
        echo -e "${BLUE}  NETCONF port: 830${NC}"
        echo -e "${BLUE}  Logs: /var/log/console.log${NC}"
        echo -e "${BLUE}========================================${NC}"
        echo ""

        # Execute server (foreground)
        exec ${ROOT_DIR}/bin/mplane-server-app \
            --cfg-data-path "${ROOT_DIR}/yang-config" \
            --yang-mods-path /usr/share/mplane-server/modules \
            --netopeer-path "${DEPS_INSTALL}/bin" \
            --netopeerdbg 2
        ;;

    setup)
        echo -e "${GREEN}[Docker] Running sysrepo setup only...${NC}"
        /opt/mplane/docker-scripts/init-sysrepo.sh
        echo -e "${GREEN}[Docker] ✓ Setup complete!${NC}"
        ;;

    shell)
        echo -e "${GREEN}[Docker] Starting interactive shell${NC}"
        echo -e "${YELLOW}Environment:${NC}"
        echo -e "  LD_LIBRARY_PATH: $LD_LIBRARY_PATH"
        echo -e "  PATH: $PATH"
        echo ""
        exec /bin/bash
        ;;

    test)
        echo -e "${GREEN}[Docker] Running system tests...${NC}"
        echo -e "${YELLOW}[Test] Checking binaries...${NC}"
        ls -lh ${ROOT_DIR}/bin/
        echo ""
        echo -e "${YELLOW}[Test] Checking dependencies...${NC}"
        ldd ${ROOT_DIR}/bin/mplane-server-app | head -10
        echo ""
        echo -e "${YELLOW}[Test] Checking sysrepo...${NC}"
        ${DEPS_INSTALL}/bin/sysrepoctl -l 2>&1 | head -20 || echo "Sysrepo not initialized (run 'setup' first)"
        echo ""
        echo -e "${GREEN}[Test] ✓ Tests complete!${NC}"
        ;;

    version)
        echo -e "${GREEN}[Docker] M-Plane Server Version Info${NC}"
        echo ""
        echo "  Container: mplane-server:latest"
        echo "  Base OS: Ubuntu 20.04"
        echo "  Build: Multi-stage Docker build"
        echo ""
        echo "Components:"
        ${DEPS_INSTALL}/bin/sysrepoctl --version 2>/dev/null || echo "  sysrepo: (check setup)"
        echo ""
        ;;

    *)
        echo -e "${YELLOW}[Docker] Running custom command: $@${NC}"
        exec "$@"
        ;;
esac
