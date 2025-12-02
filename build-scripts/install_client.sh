#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: install_client.sh [-b build_dir] [-p prefix] [-g generator]

Options:
  -b  Build directory to use (default: build/client-qt)
  -p  Install prefix (default: dist/client)
  -g  CMake generator (default: Ninja if available, otherwise CMake default)
USAGE
}

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/client-qt"
PREFIX="${ROOT_DIR}/dist/client"
GENERATOR=""

while getopts "hb:p:g:" opt; do
  case "$opt" in
    h)
      usage
      exit 0
      ;;
    b)
      BUILD_DIR="$OPTARG"
      ;;
    p)
      PREFIX="$OPTARG"
      ;;
    g)
      GENERATOR="$OPTARG"
      ;;
    *)
      usage
      exit 1
      ;;
  esac
done

if [[ -z "$GENERATOR" ]] && command -v ninja >/dev/null 2>&1; then
  GENERATOR="Ninja"
fi

CONFIGURE_ARGS=("-S" "${ROOT_DIR}/client-qt" "-B" "$BUILD_DIR")
if [[ -n "$GENERATOR" ]]; then
  CONFIGURE_ARGS+=("-G${GENERATOR}")
fi

cmake "${CONFIGURE_ARGS[@]}"
cmake --build "$BUILD_DIR"
cmake --install "$BUILD_DIR" --prefix "$PREFIX"

echo "✅ sm_client installed to: $PREFIX"
