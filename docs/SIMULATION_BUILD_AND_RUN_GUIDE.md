# M-Plane Simulator - Build and Run Guide

This guide provides step-by-step instructions for building and running the O-RAN M-Plane simulator from scratch.

## Table of Contents
- [Prerequisites](#prerequisites)
- [Getting the Source Code](#getting-the-source-code)
- [Building Server Dependencies](#building-server-dependencies)
- [Pre-Configuration Setup](#pre-configuration-setup)
- [Building Client Dependencies (Optional)](#building-client-dependencies-optional)
- [Building the Simulation](#building-the-simulation)
- [One-Time Sysrepo Setup](#one-time-sysrepo-setup)
- [Running the Server](#running-the-server)
- [Stopping the Server](#stopping-the-server)

---

## Prerequisites

### System Requirements
- Ubuntu 20.04 or later
- At least 8GB RAM
- At least 20GB free disk space
- sudo access for system configuration

### Install System Dependencies

```bash
# Update package list
sudo apt update

# Install build essentials
sudo apt install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libssl-dev \
    libpcre2-dev \
    zlib1g-dev \
    wget \
    curl

```

---

## Getting the Source Code

Clone the repository from GitHub:

```bash
cd ~/
mkdir -p mplane_dev
cd mplane_dev

# Clone the repository
git clone https://github.com/Lkishor123/open-mplane.git
cd open-mplane

# Checkout the Release1.0 branch
git checkout Release1.0
```

---

## Building Server Dependencies

The M-Plane server requires several dependencies including libyang, sysrepo, libnetconf2, netopeer2, libssh, and tinyxml2.

### Step 1: Download Dependencies

```bash
cd ~/mplane_dev/open-mplane/mplane_server/utils

# Download all server dependencies
./get_deps_server.sh --no-fwdproxy
```

This script will download:
- **libssh** (v0.9.2) - SSH library for secure connections
- **libyang** (v1.0.225) - YANG data modeling language library
- **libnetconf2** (v1.1.43) - NETCONF protocol library
- **sysrepo** (specific commit) - YANG-based datastore
- **tinyxml2** (v10.0.0) - XML parsing library
- **netopeer2** (v1.1.27) - NETCONF server

All dependencies are downloaded to `mplane_server/deps/`

**Note:** Use `--no-fwdproxy` to disable forward proxy. Remove this flag if you need to use a forward proxy.

**Expected time:** 5-10 minutes depending on internet speed

### Step 2: Build Dependencies

```bash
cd ~/mplane_dev/open-mplane/mplane_server/utils

# Build all server dependencies
./build_deps_server.sh
```

This script will:
1. Build libssh with SSH support
2. Build libyang with C++ bindings
3. Build libnetconf2 with SSH and TLS support
4. Build sysrepo with C++ bindings
5. Build netopeer2 NETCONF server
6. Install all libraries to `mplane_server/deps/install/`

**Expected output:**
```
Building libssh...
Building libyang...
Building libnetconf2...
Building sysrepo...
Building netopeer2...
Dependencies built and installed to .../mplane_server/deps/install
```

**Expected time:** 15-30 minutes depending on your system

**Important:** The build creates a symlink from `lib64` to `lib` to handle platform differences in library installation paths.

### Step 3: Apply Netopeer2 Patches (Required)

After building dependencies, apply required patches to fix build errors and NACM bugs in netopeer2:

```bash
cd ~/mplane_dev/open-mplane/mplane_server/deps/netopeer2

# Apply the netopeer2 fixes patch
git apply ../../utils/netopeer2-fixes.patch
```

**What this patch fixes:**
- **Build errors:** Multiple definition issues for `some_msg` and `np2_stderr_log` variables
- **NACM bug:** Incorrect username retrieval in NETCONF NMDA get-data operation (issue #617)
  - Changes `sr_session_get_user(session)` to `np_get_nc_sess_user(session)` in `src/netconf_nmda.c`

**Verification:**
```bash
# Check if patches were applied successfully
git diff HEAD | grep -E "(extern char some_msg|np_get_nc_sess_user)"
```

**Expected output:**
```
+extern char some_msg[4096];
+    ncac_check_data_read_filter(&data_get, np_get_nc_sess_user(session));
```

**Note:** If you get "error: patch failed" or "already applied", the patches may already be applied. You can verify by checking the files manually or running the build - it should succeed if patches are present.

---

## Pre-Configuration Setup

After building server dependencies, you need to set up the required directory structure, copy YANG modules, and configure the system before building the simulation.

### Step 1: Set Up libyang Module Directory

Create directory structure and copy libyang YANG modules:

```bash
cd ~/mplane_dev/open-mplane/mplane_server/deps

# Create libyang modules directory
mkdir -p install/share/yang/modules/libyang

# Copy libyang YANG modules
find libyang/models -name "*.yang" -type f -exec cp {} install/share/yang/modules/libyang/ \; 2>/dev/null
find libyang/src/extensions -name "*.yang" -type f -exec cp {} install/share/yang/modules/libyang/ \; 2>/dev/null

# Verify modules were copied
echo "Copied $(ls install/share/yang/modules/libyang/*.yang 2>/dev/null | wc -l) libyang modules"
```

**Expected output:**
```
Copied 6 libyang modules
```

**Verification:**
```bash
ls -la install/share/yang/modules/libyang/
```

You should see modules like:
- `ietf-datastores@2018-02-14.yang`
- `ietf-inet-types@2013-07-15.yang`
- `ietf-yang-library@2019-01-04.yang`
- `ietf-yang-metadata@2016-08-05.yang`
- `ietf-yang-types@2013-07-15.yang`
- `yang@2017-02-20.yang`

### Step 2: Create libyang System Symlinks

Create symbolic links for libyang extensions and user types:

```bash
# Create libyang directory in system library path
sudo mkdir -p /lib/libyang1

# Create symlinks to extensions and user_types
sudo ln -sf ~/mplane_dev/open-mplane/mplane_server/deps/install/lib/libyang1/extensions /lib/libyang1/extensions
sudo ln -sf ~/mplane_dev/open-mplane/mplane_server/deps/install/lib/libyang1/user_types /lib/libyang1/user_types
```

**Verification:**
```bash
ls -la /lib/libyang1/
```

**Expected output:**
```
lrwxrwxrwx 1 root root extensions -> /home/.../mplane_server/deps/install/lib/libyang1/extensions
lrwxrwxrwx 1 root root user_types -> /home/.../mplane_server/deps/install/lib/libyang1/user_types
```

### Step 3: Create Sysrepo Directories

Set up the sysrepo repository structure:

```bash
# Create sysrepo directories
sudo mkdir -p /etc/sysrepo/yang /etc/sysrepo/data /etc/sysrepo/data/notif /etc/sysrepo/data/yang

# Set ownership and permissions
sudo chown -R $USER:$USER /etc/sysrepo
sudo chmod -R 777 /etc/sysrepo
```

**Note:** The 777 permissions are for development convenience. For production, use more restrictive permissions.

### Step 4: Create M-Plane Modules Directory

```bash
# Create YANG modules directory for M-Plane server
sudo mkdir -p /usr/share/mplane-server/modules

# Set ownership and permissions
sudo chown -R $USER:$USER /usr/share/mplane-server/modules
sudo chmod -R 775 /usr/share/mplane-server/modules
```

### Step 5: Create Log Files

```bash
# Create log files
sudo touch /var/log/console.log /var/log/app-state

# Set permissions (make them world-writable for development)
sudo chmod 666 /var/log/console.log /var/log/app-state
```

### Step 6: Copy O-RAN YANG Modules

Copy all O-RAN YANG modules to the modules directory:

```bash
cd ~/mplane_dev/open-mplane

# Copy all YANG modules
find mplane_server/yang-models -name "*.yang" -type f -exec cp {} /usr/share/mplane-server/modules/ \;

# Set proper permissions
chmod 644 /usr/share/mplane-server/modules/*.yang

# Verify modules were copied
echo "Copied $(ls /usr/share/mplane-server/modules/*.yang 2>/dev/null | wc -l) O-RAN YANG modules"
```

**Expected output:**
```
Copied 52 O-RAN YANG modules
```

### Step 7: Verify Sysrepo Installation

Set up environment variables and verify sysrepo is working:

```bash
# Set environment variables
export INSTALL_DIR="$PWD/mplane_server/deps/install"
export LD_LIBRARY_PATH="${INSTALL_DIR}/lib64:${INSTALL_DIR}/lib"
export YANG_MODPATH="${INSTALL_DIR}/share/yang/modules/libyang"
export PATH="${INSTALL_DIR}/bin:$PATH"
export SYSREPO_REPOSITORY_PATH="/etc/sysrepo"

# List installed sysrepo modules
"${INSTALL_DIR}/bin/sysrepoctl" -l
```

**Expected output:**
```
Sysrepo repository: /etc/sysrepo

Module Name                | Revision   | Flags | Owner       | Permissions | Submodules | Features
---------------------------------------------------------------------------------------------------
ietf-datastores            | 2018-02-14 | I     | fahim:fahim | 666         |            |
ietf-inet-types            | 2013-07-15 | i     |             |             |            |
ietf-netconf               | 2011-06-01 | I     | fahim:fahim | 666         |            |
ietf-netconf-notifications | 2012-02-06 | I     | fahim:fahim | 666         |            |
ietf-netconf-with-defaults | 2011-06-01 | I     | fahim:fahim | 666         |            |
ietf-origin                | 2018-02-14 | I     | fahim:fahim | 666         |            |
ietf-yang-library          | 2019-01-04 | I     | fahim:fahim | 666         |            |
ietf-yang-metadata         | 2016-08-05 | i     |             |             |            |
ietf-yang-types            | 2013-07-15 | i     |             |             |            |
sysrepo-monitoring         | 2020-04-17 | I     | fahim:fahim | 600         |            |
yang                       | 2017-02-20 | I     | fahim:fahim | 666         |            |
```

You should see the basic NETCONF and sysrepo modules installed.

---

## Building Client Dependencies (Optional)

If you plan to use the M-Plane client for testing, build the client dependencies before building the simulation.

### Step 1: Install Client Prerequisites

```bash
sudo apt install cmake build-essential libssl-dev zlib1g-dev libpcre3-dev
```

### Step 2: Download Client Dependencies

```bash
cd ~/mplane_dev/open-mplane/mplane_client/utils/

# Download client dependencies
./get_deps.sh --no-fwdproxy --dir ../
```

### Step 3: Build Client Dependencies

```bash
# Build client dependencies (without netopeer2 as it's handled by server)
./build_deps.sh --no-netopeer2 --dir ../
```

### Step 4: Build M-Plane Client

```bash
# Build the client with 2 parallel jobs
./build_mpclient.sh --parallel 2
```

**Expected output:**
```
Building mplane_client...
[Build progress...]
Client binary: .../mplane_client/build/mpc_client
```

**Expected time:** 15-25 minutes for first build

**Note:** The `build_sim.sh` script in the next section will automatically build the client if dependencies are not found, so this step is optional if you want to build everything at once.

---

## Building the Simulation

Build the complete simulation including server, HAL, and optionally client components:

```bash
cd ~/mplane_dev/open-mplane

# Build the simulation
./tools/sim/build_sim.sh
```

This script will:
1. Build `halmplane` (x86 HAL library) - Hardware abstraction layer for x86 platforms
2. Build `server-test-shim` (HAL simulator) - Simulates hardware interactions
3. Build `mplane-server-app` (M-Plane server) - Main O-RAN M-Plane server application
4. Build `mpc_client` (M-Plane client) - If client dependencies were built in the previous step

**Expected output:**
```
[sim] Building halmplane (x86)
[sim] Building server-test-shim
[sim] Building mplane_server with HAL_TEST
[sim] Building mplane_client
[sim] Done. Binaries:
 - Shim: .../mplane_server/utils/test_shim/build/server-test-shim
 - Server: .../build/server-sim/mplane-server-app
 - Client: .../mplane_client/build/mpc_client
```

**Note:** If client dependencies were not built, the script will automatically fetch and build them.

**Expected time:** 10-20 minutes for first build (server dependencies must be built first)

---

## One-Time Sysrepo Setup

After completing the pre-configuration setup, run the M-Plane server setup script to configure remaining YANG modules and NETCONF settings.

### Run Setup Script

Run the M-Plane server setup script to configure sysrepo, install YANG modules, and set up NETCONF:

```bash
cd ~/mplane_dev/open-mplane

# Run setup (one-time operation)
./tools/setup_mplane_server.sh --setup-only --force
```

This script will:
1. **Phase 0:** Verify all required tools are available
2. **Phase 1:** Locate netopeer2 scripts
3. **Phase 3:** Set up environment variables
4. **Phase 4:** Install YANG modules to sysrepo
5. **Phase 5:** Generate SSH host keys for NETCONF
6. **Phase 6:** Configure NETCONF server endpoint (port 830)
7. **Phase 7:** Configure O-RAN user management
8. **Phase 8:** Clean up any stale sysrepo shared memory

**Expected output:**
```
=== M-Plane Server Setup ===

[INFO] Phase 0: Pre-flight Checks
[✓] Found sysrepoctl: .../bin/sysrepoctl
[✓] Found openssl
[✓] YANG modules directory: /usr/share/mplane-server/modules
[✓] Sysrepo repository: /etc/sysrepo

[INFO] Phase 1: Script Discovery
[✓] Found netopeer2 scripts: .../netopeer2/build
...

[✓] Setup completed successfully!
```

**What this does:**
- Installs core NETCONF and sysrepo YANG modules
- Configures SSH authentication for NETCONF
- Sets up O-RAN-specific modules and users
- Prepares the system for the M-Plane server

**Important Notes:**
- Run this script **only once** or when you need to reset the configuration
- Use `--force` flag to reinstall/reconfigure everything
- The script is idempotent (safe to run multiple times with `--force`)

---

## Running the Server

Start the M-Plane server with isolated dependencies:

```bash
cd ~/mplane_dev/open-mplane

# Start the server
./tools/run_server_only.sh
```

The script will:
1. Clean up stale sockets and shared memory
2. Start the HAL test shim (hardware simulator)
3. Start the M-Plane server with sudo privileges (for port 830 binding)

**Expected output:**
```
[Server] Starting M-Plane Server with isolated dependencies
[Server] Cleaning sysrepo shared memory
[Server] Starting HAL test shim...
[Server] Shim PID: 12345
[Server] Starting mplane-server-app with elevated privileges...
[Server] Server PID: 12346

========================================
Server started successfully!
========================================
Shim PID: 12345
Server PID: 12346

To stop the server:
  kill 12346
  kill 12345

Or use: tools/stop_server.sh
========================================
```

**The server runs in the foreground.** Open a new terminal for monitoring logs or other commands.

### Verify Server is Running

Check server logs in a separate terminal:

```bash
# Follow server logs
tail -f /var/log/console.log

# Or check for errors
grep -i error /var/log/console.log
```

**Expected log output (healthy server):**
```
[info] RRH call radio initialisation
[info] RRH create services monitor
[info] RRH create services
[info] Application being created
[info] Load file .../YangConfig.xml successful
[info] Initialising YANG Manager
[info] Installing ietf-interfaces module ...
[info] Installed: .../ietf-interfaces.yang
```

---

## Stopping the Server

```bash
cd ~/mplane_dev/open-mplane
./tools/stop_server.sh
```

Or manually:
```bash
# Kill server processes
kill $(cat /tmp/mplane-server-app.pid)
kill $(cat /tmp/server-test-shim.pid)

# Clean up PID files
rm -f /tmp/mplane-server-app.pid /tmp/server-test-shim.pid

# Clean up sysrepo shared memory (optional)
sudo rm -rf /dev/shm/sr_* /dev/shm/srsub_*
```

---

## Quick Reference

### Build Commands
```bash
# Step 1: Get and build server dependencies
cd ~/mplane_dev/open-mplane/mplane_server/utils
./get_deps_server.sh --no-fwdproxy
./build_deps_server.sh

# Step 2: Apply netopeer2 patches (REQUIRED)
cd ~/mplane_dev/open-mplane/mplane_server/deps/netopeer2
git apply ../../utils/netopeer2-fixes.patch

# Step 3: Pre-configuration setup
cd ~/mplane_dev/open-mplane/mplane_server/deps
mkdir -p install/share/yang/modules/libyang
find libyang/models -name "*.yang" -type f -exec cp {} install/share/yang/modules/libyang/ \; 2>/dev/null
find libyang/src/extensions -name "*.yang" -type f -exec cp {} install/share/yang/modules/libyang/ \; 2>/dev/null

sudo mkdir -p /lib/libyang1
sudo ln -sf ~/mplane_dev/open-mplane/mplane_server/deps/install/lib/libyang1/extensions /lib/libyang1/extensions
sudo ln -sf ~/mplane_dev/open-mplane/mplane_server/deps/install/lib/libyang1/user_types /lib/libyang1/user_types

sudo mkdir -p /etc/sysrepo/yang /etc/sysrepo/data /etc/sysrepo/data/notif /etc/sysrepo/data/yang
sudo chown -R $USER:$USER /etc/sysrepo
sudo chmod -R 777 /etc/sysrepo

sudo mkdir -p /usr/share/mplane-server/modules
sudo chown -R $USER:$USER /usr/share/mplane-server/modules
sudo chmod -R 775 /usr/share/mplane-server/modules

sudo touch /var/log/console.log /var/log/app-state
sudo chmod 666 /var/log/console.log /var/log/app-state

cd ~/mplane_dev/open-mplane
find mplane_server/yang-models -name "*.yang" -type f -exec cp {} /usr/share/mplane-server/modules/ \;
chmod 644 /usr/share/mplane-server/modules/*.yang

# Step 4 (Optional): Build client dependencies
sudo apt install cmake build-essential libssl-dev zlib1g-dev libpcre3-dev
cd ~/mplane_dev/open-mplane/mplane_client/utils/
./get_deps.sh --no-fwdproxy --dir ../
./build_deps.sh --no-netopeer2 --dir ../
./build_mpclient.sh --parallel 2

# Step 5: Build the simulation
cd ~/mplane_dev/open-mplane
./tools/sim/build_sim.sh
```

### Setup Commands
```bash
# One-time setup: Install remaining YANG modules and configure NETCONF
./tools/setup_mplane_server.sh --setup-only --force
```

### Run Commands
```bash
./tools/run_server_only.sh            # Start server
./tools/stop_server.sh                # Stop server
```

### Verify Commands
```bash
# Check server is running
ps aux | grep mplane-server-app

# Check HAL shim is running
ps aux | grep server-test-shim

# Check NETCONF port 830 is listening
sudo netstat -tlnp | grep :830

# View server logs
tail -f /var/log/console.log

# Check for errors in logs
grep -i error /var/log/console.log

# List installed YANG modules
export LD_LIBRARY_PATH=~/mplane_dev/open-mplane/mplane_server/deps/install/lib64
~/mplane_dev/open-mplane/mplane_server/deps/install/bin/sysrepoctl -l
```

---

## Next Steps

Once the simulator is running successfully:

1. **Test alarm injection:**
   ```bash
   ./tools/sim/inject_alarm.sh 1001 Major false "Test alarm"
   ```

2. **Connect with NETCONF client:**
   ```bash
   # Install netopeer2-cli if not available
   sudo apt install netopeer2

   # Connect to server
   netopeer2-cli
   > connect --host localhost --port 830 --login root
   > get-config --source running
   ```

3. **Explore O-RAN APIs:**
   - Review YANG models in `/usr/share/mplane-server/modules/`
   - Test configuration changes via NETCONF
   - Monitor server logs for debugging

---

## Additional Resources

- **GitHub Repository:** https://github.com/Lkishor123/open-mplane/tree/Release1.0
- **Detailed Build Analysis:** `docs/SIMULATOR_BUILD_ANALYSIS.md`
- **Quick Start Guide:** `docs/SIMULATOR_QUICKSTART.md`
- **Alarm Testing:** `docs/ALARM_TESTING_SHARED_MEMORY_FIX.md`

---

**Last Updated:** 2025-11-19
**Version:** Release1.0
