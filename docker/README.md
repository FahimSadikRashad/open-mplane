# O-RAN M-Plane Server - Docker

Containerized O-RAN M-Plane simulation with complete dependency isolation.

## Quick Start

```bash
# From project root (open-mplane/)
cd open-mplane

# Build
docker-compose -f docker/docker-compose.yml build

# Run
docker-compose -f docker/docker-compose.yml up

# Server is now running on port 830!
```

## What's Included

- **Complete M-Plane simulation** (halmplane, test-shim, server)
- **All dependencies** (libyang, sysrepo, netopeer2, etc.)
- **Automatic sysrepo setup** (YANG modules, SSH keys, NETCONF)
- **Isolated environment** (no host system pollution)
- **Persistent volumes** (sysrepo data, logs)

## Directory Structure

```
docker/
├── Dockerfile              # Multi-stage production build
├── docker-compose.yml      # Orchestration configuration
├── .dockerignore           # Build context exclusions
│
├── scripts/
│   ├── docker-entrypoint.sh  # Container startup script
│   └── init-sysrepo.sh       # Sysrepo initialization
│
├── config/                 # (Optional) Custom configurations
└── volumes/                # (Gitignored) Volume mount points
```

## Common Commands

```bash
# Build
docker-compose -f docker/docker-compose.yml build

# Start (foreground)
docker-compose -f docker/docker-compose.yml up

# Start (background)
docker-compose -f docker/docker-compose.yml up -d

# Stop
docker-compose -f docker/docker-compose.yml stop

# Clean everything (including volumes)
docker-compose -f docker/docker-compose.yml down -v

# View logs
docker-compose -f docker/docker-compose.yml logs -f

# Shell access
docker-compose -f docker/docker-compose.yml exec mplane-server shell
```

## Documentation

📖 **Complete Guide**: [docs/DOCKER_BUILD_AND_RUN_GUIDE.md](../docs/DOCKER_BUILD_AND_RUN_GUIDE.md)

📋 **Implementation Plan**: [docs/DOCKER_SIMULATION_PLAN.md](../docs/DOCKER_SIMULATION_PLAN.md)

🔧 **Manual Build**: [docs/SIMULATION_BUILD_AND_RUN_GUIDE.md](../docs/SIMULATION_BUILD_AND_RUN_GUIDE.md)

## Features

✅ **Complete Isolation** - Sysrepo, dependencies fully isolated
✅ **Fast Builds** - Multi-stage with layer caching
✅ **Easy Cleanup** - One command to reset everything
✅ **Production Ready** - Minimal runtime image (~500MB)
✅ **Development Friendly** - Debug profile included

## Troubleshooting

See [DOCKER_BUILD_AND_RUN_GUIDE.md](../docs/DOCKER_BUILD_AND_RUN_GUIDE.md#troubleshooting) for detailed troubleshooting steps.

**Quick fixes:**

- **Port 830 in use**: `sudo lsof -i :830` and kill the process
- **Container exits**: `docker logs mplane-server` to check errors
- **Sysrepo errors**: `docker-compose down -v && docker-compose up`

## Requirements

- Docker 20.10+
- Docker Compose 1.29+
- 4GB RAM (8GB recommended for build)
- 10GB free disk space

## Support

Issues? Questions? See the [Troubleshooting section](../docs/DOCKER_BUILD_AND_RUN_GUIDE.md#troubleshooting) or check the logs:

```bash
docker-compose -f docker/docker-compose.yml logs -f
```

---

**Version**: 1.0
**Status**: Production Ready
**Last Updated**: 2025-11-15
