# O-RAN M-Plane Server - Docker Build & Run Guide

Complete guide for building and running the O-RAN M-Plane simulation using Docker.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [Building the Docker Image](#building-the-docker-image)
- [Running the Server](#running-the-server)
- [Accessing the Server](#accessing-the-server)
- [Managing the Container](#managing-the-container)
- [Troubleshooting](#troubleshooting)
- [Advanced Usage](#advanced-usage)

---

## Prerequisites

### Required Software

1. **Docker** (version 20.10 or later)
   ```bash
   # Check Docker version
   docker --version

   # If not installed, install Docker:
   # Ubuntu/Debian
   curl -fsSL https://get.docker.com | sh
   sudo usermod -aG docker $USER
   # Log out and back in for group changes
   ```

2. **Docker Compose** (version 1.29 or later)
   ```bash
   # Check Docker Compose version
   docker-compose --version

   # Usually comes with Docker Desktop
   # Or install separately:
   sudo apt-get install docker-compose
   ```

### System Requirements

- **CPU**: 2+ cores (4+ recommended)
- **RAM**: 4GB minimum (8GB recommended for build)
- **Disk**: 10GB free space
- **OS**: Linux (tested on Ubuntu 20.04+)

### Verify Docker Installation

```bash
# Test Docker
docker run hello-world

# Test Docker Compose
docker-compose version
```

---

## Quick Start

**TL;DR - Get running in 3 commands:**

```bash
cd open-mplane

# Build the Docker image
docker-compose -f docker/docker-compose.yml build

# Start the server
docker-compose -f docker/docker-compose.yml up

# Server is now running on port 830!
```

---

## Building the Docker Image

### Step 1: Navigate to Project Root

```bash
cd /path/to/open-mplane
```

### Step 2: Build the Image

**Option A: Using Docker Compose (Recommended)**

```bash
docker-compose -f docker/docker-compose.yml build
```

**Option B: Using Docker directly**

```bash
docker build -f docker/Dockerfile -t mplane-server:latest .
```

### Build Process

The build happens in **3 stages**:

1. **Stage 1 (deps-builder)**: Builds dependencies (~10-15 minutes)
   - Downloads: libssh, libyang, sysrepo, netopeer2, tinyxml2
   - Builds all dependencies
   - Size: ~2GB

2. **Stage 2 (server-builder)**: Builds simulation (~5-10 minutes)
   - Builds: halmplane (x86), server-test-shim, mplane-server-app
   - Links against dependencies from Stage 1
   - Size: ~2.5GB

3. **Stage 3 (runtime)**: Creates minimal runtime image (~2 minutes)
   - Copies only binaries and libraries
   - Removes build tools
   - Final size: ~500MB

**Total build time**: 15-30 minutes (first time)

### Expected Output

```
[+] Building 1234.5s (25/25) FINISHED
 => [deps-builder  1/8] FROM ubuntu:20.04
 => [deps-builder  2/8] RUN apt-get update && apt-get install...
 => [deps-builder  3/8] COPY mplane_server/utils/...
 => [deps-builder  4/8] RUN ./utils/get_deps_server.sh...
 => [deps-builder  5/8] RUN ./utils/build_deps_server.sh...
 => [server-builder 1/5] COPY libhalmplane/ /opt/mplane/...
 => [server-builder 2/5] RUN cmake -S...
 => [runtime 1/6] FROM ubuntu:20.04
 => [runtime 2/6] RUN apt-get update...
 => exporting to image
 => => writing image sha256:abc123...
 => => naming to docker.io/library/mplane-server:latest
```

### Verify Build Success

```bash
# List built image
docker images | grep mplane-server

# Should show:
# mplane-server   latest   abc123def456   2 minutes ago   500MB
```

---

## Running the Server

### Method 1: Docker Compose (Recommended)

**Start in foreground** (see logs in terminal):

```bash
docker-compose -f docker/docker-compose.yml up
```

**Start in background** (detached mode):

```bash
docker-compose -f docker/docker-compose.yml up -d
```

**Expected output:**

```
[+] Running 3/3
 ✔ Network docker_mplane-net      Created
 ✔ Volume "docker_sysrepo-data"   Created
 ✔ Volume "docker_server-logs"    Created
 ✔ Container mplane-server        Started

Attaching to mplane-server
mplane-server  | ========================================
mplane-server  |   O-RAN M-Plane Server (Docker)
mplane-server  | ========================================
mplane-server  |
mplane-server  | [Docker] First run detected - initializing sysrepo...
mplane-server  | ========================================
mplane-server  |   Sysrepo Initialization
mplane-server  | ========================================
mplane-server  |
mplane-server  | [Setup] Phase 1: Installing YANG modules...
mplane-server  | [Setup] ✓ YANG modules installed
mplane-server  |
mplane-server  | [Setup] Phase 2: Generating SSH host keys...
mplane-server  | [Setup] ✓ SSH keys generated
mplane-server  |
mplane-server  | [Setup] Phase 3: Configuring NETCONF endpoint (port 830)...
mplane-server  | [Setup] ✓ NETCONF endpoint configured
mplane-server  |
mplane-server  | [Setup] Phase 4: Configuring O-RAN user management...
mplane-server  | [Setup] ✓ O-RAN users configured
mplane-server  |
mplane-server  | [Docker] ✓ Sysrepo initialization complete!
mplane-server  |
mplane-server  | [Docker] Starting M-Plane Server...
mplane-server  | [Docker] Starting HAL test shim...
mplane-server  | [Docker] ✓ Shim PID: 8
mplane-server  | [Docker] Starting mplane-server-app...
mplane-server  | ========================================
mplane-server  |   Server is starting...
mplane-server  |   NETCONF port: 830
mplane-server  |   Logs: /var/log/console.log
mplane-server  | ========================================
```

### Method 2: Docker Run (Manual)

```bash
docker run -d \
  --name mplane-server \
  -p 830:830 \
  --cap-add NET_BIND_SERVICE \
  -v mplane-sysrepo:/etc/sysrepo \
  -v mplane-logs:/var/log \
  mplane-server:latest
```

---

## Accessing the Server

### Check Server Status

```bash
# Using Docker Compose
docker-compose -f docker/docker-compose.yml ps

# Using Docker
docker ps | grep mplane-server
```

**Expected output:**

```
NAME            IMAGE                 STATUS         PORTS
mplane-server   mplane-server:latest  Up 2 minutes   0.0.0.0:830->830/tcp
```

### View Server Logs

```bash
# Using Docker Compose
docker-compose -f docker/docker-compose.yml logs -f mplane-server

# Using Docker
docker logs -f mplane-server

# View application log file
docker exec mplane-server tail -f /var/log/console.log
```

### Access Interactive Shell

```bash
# Using Docker Compose
docker-compose -f docker/docker-compose.yml exec mplane-server shell

# Using Docker
docker exec -it mplane-server /opt/mplane/docker-scripts/docker-entrypoint.sh shell
```

### Connect with NETCONF Client

**From host machine** (requires netopeer2-cli installed):

```bash
netopeer2-cli
> connect --host localhost --port 830 --login root
> get-config --source running
```

**From inside container**:

```bash
# Open shell in container
docker exec -it mplane-server bash

# Use netopeer2-cli
/opt/mplane/deps/bin/netopeer2-cli
> connect --host localhost --port 830 --login root
> get-config --source running
```

---

## Managing the Container

### Stop the Server

```bash
# Using Docker Compose
docker-compose -f docker/docker-compose.yml stop

# Using Docker
docker stop mplane-server
```

### Start the Server (if stopped)

```bash
# Using Docker Compose
docker-compose -f docker/docker-compose.yml start

# Using Docker
docker start mplane-server
```

### Restart the Server

```bash
# Using Docker Compose
docker-compose -f docker/docker-compose.yml restart

# Using Docker
docker restart mplane-server
```

### Stop and Remove Container

```bash
# Using Docker Compose
docker-compose -f docker/docker-compose.yml down

# Using Docker
docker stop mplane-server && docker rm mplane-server
```

### Remove Container AND Volumes (Clean State)

```bash
# ⚠️ WARNING: This deletes all sysrepo data!
docker-compose -f docker/docker-compose.yml down -v
```

---

## Troubleshooting

### Issue: Build Fails

**Problem**: Docker build fails with errors

**Solution**:
```bash
# Check Docker disk space
docker system df

# Clean up unused resources
docker system prune

# Try build again with no cache
docker-compose -f docker/docker-compose.yml build --no-cache
```

### Issue: Port 830 Already in Use

**Problem**: `Error: bind: address already in use`

**Solution**:
```bash
# Find process using port 830
sudo lsof -i :830

# Kill the process or stop other NETCONF servers
sudo kill -9 <PID>

# Or use a different port in docker-compose.yml:
# ports:
#   - "8300:830"
```

### Issue: Container Exits Immediately

**Problem**: Container starts then stops

**Solution**:
```bash
# Check container logs
docker logs mplane-server

# Check for errors in sysrepo setup
docker-compose -f docker/docker-compose.yml run --rm mplane-server setup

# Run in interactive mode for debugging
docker-compose -f docker/docker-compose.yml run --rm mplane-server shell
```

### Issue: Sysrepo Errors

**Problem**: `SR_ERR_INTERNAL` or sysrepo not initialized

**Solution**:
```bash
# Reinitialize sysrepo
docker exec mplane-server /opt/mplane/docker-scripts/init-sysrepo.sh

# Or reset everything
docker-compose -f docker/docker-compose.yml down -v
docker-compose -f docker/docker-compose.yml up
```

### Issue: Can't Connect via NETCONF

**Problem**: Connection refused on port 830

**Solution**:
```bash
# Verify server is running
docker exec mplane-server pgrep -f mplane-server-app

# Check if netopeer2-server is running
docker exec mplane-server pgrep -f netopeer2-server

# Verify port binding
docker port mplane-server 830

# Check firewall
sudo ufw status
sudo ufw allow 830/tcp
```

### Issue: Permission Denied Errors

**Problem**: Permission errors in logs

**Solution**:
```bash
# Ensure proper volume permissions
docker-compose -f docker/docker-compose.yml down
docker volume rm docker_sysrepo-data docker_server-logs
docker-compose -f docker/docker-compose.yml up
```

---

## Advanced Usage

### Run Specific Commands

```bash
# Run sysrepo setup only
docker-compose -f docker/docker-compose.yml run --rm mplane-server setup

# Run tests
docker-compose -f docker/docker-compose.yml run --rm mplane-server test

# Check version info
docker-compose -f docker/docker-compose.yml run --rm mplane-server version
```

### Access Development Container

```bash
# Start development shell (with debug profile)
docker-compose -f docker/docker-compose.yml --profile debug up -d mplane-dev

# Attach to development shell
docker attach mplane-dev
```

### Custom Configuration

Create custom configuration files:

```bash
# Create config directory
mkdir -p docker/config

# Add custom YangConfig.xml
cp mplane_server/yang-manager-server/yang-config/YangConfig.xml \
   docker/config/YangConfig.xml

# Edit as needed
vim docker/config/YangConfig.xml

# Uncomment volume mount in docker-compose.yml:
# - ./config:/opt/mplane/config:ro
```

### View Sysrepo Modules

```bash
# List installed YANG modules
docker exec mplane-server \
  /opt/mplane/deps/bin/sysrepoctl -l

# Export running configuration
docker exec mplane-server \
  /opt/mplane/deps/bin/sysrepocfg -X -d running -f json
```

### Backup and Restore

**Backup sysrepo data:**

```bash
# Create backup
docker run --rm \
  -v docker_sysrepo-data:/data \
  -v $(pwd):/backup \
  ubuntu tar czf /backup/sysrepo-backup.tar.gz /data
```

**Restore sysrepo data:**

```bash
# Restore from backup
docker run --rm \
  -v docker_sysrepo-data:/data \
  -v $(pwd):/backup \
  ubuntu tar xzf /backup/sysrepo-backup.tar.gz -C /
```

### Resource Monitoring

```bash
# Monitor resource usage
docker stats mplane-server

# View detailed info
docker inspect mplane-server
```

---

## Comparison: Docker vs Manual Build

| Aspect | Manual Build | Docker Build |
|--------|-------------|--------------|
| **First Build** | 1-2 hours | 15-30 minutes |
| **Subsequent Builds** | 15-30 min | 2-3 minutes (cached) |
| **Setup Complexity** | High (many steps) | Low (one command) |
| **Dependency Conflicts** | Frequent | Zero |
| **Isolation** | Partial | Complete |
| **Cleanup** | Manual, risky | `docker-compose down -v` |
| **Portability** | Poor | Excellent |
| **Debugging** | Easier (native) | Requires `exec` |

---

## Quick Reference

### Essential Commands

```bash
# Build
docker-compose -f docker/docker-compose.yml build

# Start (foreground)
docker-compose -f docker/docker-compose.yml up

# Start (background)
docker-compose -f docker/docker-compose.yml up -d

# Stop
docker-compose -f docker/docker-compose.yml stop

# Stop and remove
docker-compose -f docker/docker-compose.yml down

# Stop, remove, and delete volumes (clean state)
docker-compose -f docker/docker-compose.yml down -v

# View logs
docker-compose -f docker/docker-compose.yml logs -f

# Shell access
docker-compose -f docker/docker-compose.yml exec mplane-server shell

# Restart
docker-compose -f docker/docker-compose.yml restart
```

### File Locations Inside Container

```
/opt/mplane/
├── bin/
│   ├── mplane-server-app       # Main server binary
│   └── server-test-shim        # HAL simulator
├── lib/
│   └── libhalmplane.so         # HAL library
├── deps/                        # All dependencies
│   ├── bin/
│   │   ├── sysrepoctl
│   │   └── netopeer2-server
│   └── lib64/
├── yang-config/                 # YANG configuration
│   └── YangConfig.xml
├── yang-models/                 # YANG schemas
└── docker-scripts/              # Container scripts
    ├── docker-entrypoint.sh
    └── init-sysrepo.sh

/etc/sysrepo/                    # Sysrepo datastore (volume)
/var/log/
├── console.log                  # Server logs
└── app-state                    # Application state
```

---

## Next Steps

Once your server is running:

1. **Test NETCONF Connection**
   ```bash
   netopeer2-cli
   > connect --host localhost --port 830 --login root
   ```

2. **Explore YANG Models**
   ```bash
   docker exec mplane-server \
     /opt/mplane/deps/bin/sysrepoctl -l
   ```

3. **Monitor Logs**
   ```bash
   docker exec mplane-server tail -f /var/log/console.log
   ```

4. **Review O-RAN Configuration**
   ```bash
   docker exec mplane-server \
     /opt/mplane/deps/bin/sysrepocfg -X -d running -f json
   ```

---

## Additional Resources

- **Simulation Build Guide**: [SIMULATION_BUILD_AND_RUN_GUIDE.md](SIMULATION_BUILD_AND_RUN_GUIDE.md)
- **Docker Plan**: [DOCKER_SIMULATION_PLAN.md](DOCKER_SIMULATION_PLAN.md)
- **GitHub Repository**: https://github.com/Lkishor123/open-mplane
- **Docker Documentation**: https://docs.docker.com/

---

**Last Updated**: 2025-11-15
**Version**: 1.0
**Status**: Production Ready
