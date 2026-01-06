#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BUILD_TYPE="Debug"
TARGET_ARCH="x86_64"
TARGET="run"
JOBS="12"

MODE=""

DOCKER=0
DOCKER_IMAGE="coolpotos"
DOCKER_PLATFORM="linux/amd64"
DOCKER_BUILD=1

usage() {
  cat <<'EOF'
Usage:
  ./build.sh -docker [docker options] [build options]
  ./build.sh -direct [build options]

Docker options:
  --no-docker-build           Skip docker build step (reuse existing image).
  --docker-image <name>       Docker image name (default: coolpotos).
  --docker-platform <plat>    Docker platform (default: linux/amd64).

Build options:
  --arch <arch>               Target arch (default: x86_64).
  --build-type <type>         Debug | Release (default: Debug).
  --target <target>           CMake target (default: run).
  -j, --jobs <n>              Build parallel jobs (default: 12).
  -h, --help                  Show this help.
EOF
}

usage_docker() {
  cat <<'EOF'
Usage:
  ./build.sh -docker [docker options] [build options]

Docker options:
  --no-docker-build           Skip docker build step (reuse existing image).
  --docker-image <name>       Docker image name (default: coolpotos).
  --docker-platform <plat>    Docker platform (default: linux/amd64).

Build options:
  --arch <arch>               Target arch (default: x86_64).
  --build-type <type>         Debug | Release (default: Debug).
  --target <target>           CMake target (default: run).
  -j, --jobs <n>              Build parallel jobs (default: 12).
EOF
}

usage_direct() {
  cat <<'EOF'
Usage:
  ./build.sh -direct [build options]

Build options:
  --arch <arch>               Target arch (default: x86_64).
  --build-type <type>         Debug | Release (default: Debug).
  --target <target>           CMake target (default: run).
  -j, --jobs <n>              Build parallel jobs (default: 12).
EOF
}

require_value() {
  if [[ $# -lt 2 || -z "${2:-}" ]]; then
    echo "Option $1 requires a value." >&2
    exit 1
  fi
}

if [[ $# -eq 0 ]]; then
  usage
  exit 1
fi

case "$1" in
  -docker)
    MODE="docker"
    shift
    ;;
  -direct)
    MODE="direct"
    shift
    ;;
  -h|--help)
    usage
    exit 0
    ;;
  *)
    echo "First argument must be -docker or -direct." >&2
    usage
    exit 1
    ;;
esac

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-docker-build)
      if [[ "$MODE" != "docker" ]]; then
        echo "Option $1 is only valid with -docker." >&2
        usage_direct
        exit 1
      fi
      DOCKER_BUILD=0
      shift
      ;;
    --docker-image)
      if [[ "$MODE" != "docker" ]]; then
        echo "Option $1 is only valid with -docker." >&2
        usage_direct
        exit 1
      fi
      require_value "$1" "${2:-}"
      DOCKER_IMAGE="$2"
      shift 2
      ;;
    --docker-platform)
      if [[ "$MODE" != "docker" ]]; then
        echo "Option $1 is only valid with -docker." >&2
        usage_direct
        exit 1
      fi
      require_value "$1" "${2:-}"
      DOCKER_PLATFORM="$2"
      shift 2
      ;;
    --arch)
      require_value "$1" "${2:-}"
      TARGET_ARCH="$2"
      shift 2
      ;;
    --build-type)
      require_value "$1" "${2:-}"
      BUILD_TYPE="$2"
      shift 2
      ;;
    --target)
      require_value "$1" "${2:-}"
      TARGET="$2"
      shift 2
      ;;
    -j|--jobs)
      require_value "$1" "${2:-}"
      JOBS="$2"
      shift 2
      ;;
    -h|--help)
      if [[ "$MODE" == "docker" ]]; then
        usage_docker
      else
        usage_direct
      fi
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      if [[ "$MODE" == "docker" ]]; then
        usage_docker
      else
        usage_direct
      fi
      exit 1
      ;;
  esac
done

if [[ "$MODE" == "docker" ]]; then
  DOCKER=1
fi

if [[ "$DOCKER" -eq 1 ]]; then
  if [[ "$DOCKER_BUILD" -eq 1 ]]; then
    docker build --platform "$DOCKER_PLATFORM" -t "$DOCKER_IMAGE" "$ROOT_DIR"
  fi

  docker run --rm -it \
    --platform "$DOCKER_PLATFORM" \
    -v "$ROOT_DIR":/workspace \
    -w /workspace \
    -e BUILD_TYPE="$BUILD_TYPE" \
    -e TARGET_ARCH="$TARGET_ARCH" \
    -e TARGET="$TARGET" \
    -e JOBS="$JOBS" \
    "$DOCKER_IMAGE" \
    bash -lc 'set -e; \
      if ! python3 -c "import importlib.util, sys; sys.exit(0 if importlib.util.find_spec(\"cryptography\") else 1)"; then \
        if python3 -m venv /tmp/cpos-venv >/dev/null 2>&1; then \
          /tmp/cpos-venv/bin/pip install -q cryptography; \
          export PATH="/tmp/cpos-venv/bin:$PATH"; \
        else \
          python3 -m pip install -q --break-system-packages cryptography; \
        fi; \
      fi; \
      cmake -S . -B build/ -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DTARGET_ARCH="$TARGET_ARCH"; \
      cmake --build build/ --target "$TARGET" -j "${JOBS:-$(nproc)}"; \
      cp build/compile_commands.json ./compile_commands.json'
else
  cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build/" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DTARGET_ARCH="$TARGET_ARCH"

  cmake --build "$ROOT_DIR/build/" --target "$TARGET" -j "$JOBS"

  cp "$ROOT_DIR/build/compile_commands.json" "$ROOT_DIR/compile_commands.json"
fi
