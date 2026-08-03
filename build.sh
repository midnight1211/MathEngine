#!/usr/bin/env bash
# build.sh — MathEngine cross-platform build script
# Replaces build.ps1 for Linux/macOS; Windows users can still use build.ps1.
#
# Usage:
#   ./build.sh                # build C++ + Java, launch desktop app
#   ./build.sh --skip-cpp     # skip C++ recompile (faster Java-only iteration)
#   ./build.sh --server       # start the Spring Boot REST server
#   ./build.sh --server-only  # start only the server, no desktop app
#   ./build.sh --clean        # clean all build artifacts
#   ./build.sh --help

set -euo pipefail

# ── Config ────────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
LIB_DIR="$BUILD_DIR/lib"
LOG_DIR="$BUILD_DIR/logs"
SERVER_DIR="$SCRIPT_DIR/server"

SKIP_CPP=false
START_SERVER=false
SERVER_ONLY=false
CLEAN=false

# ── Argument parsing ──────────────────────────────────────────────────────────
for arg in "$@"; do
  case "$arg" in
    --skip-cpp)     SKIP_CPP=true ;;
    --server)       START_SERVER=true ;;
    --server-only)  SERVER_ONLY=true; START_SERVER=true; SKIP_CPP=true ;;
    --clean)        CLEAN=true ;;
    --help|-h)
      head -15 "$0" | grep "^#" | sed 's/^# \?//'
      exit 0 ;;
    *) echo "Unknown argument: $arg  (try --help)"; exit 1 ;;
  esac
done

# ── Colours ───────────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; RESET='\033[0m'
info()  { echo -e "${CYAN}[build]${RESET} $*"; }
ok()    { echo -e "${GREEN}[build]${RESET} $*"; }
warn()  { echo -e "${YELLOW}[build]${RESET} $*"; }
die()   { echo -e "${RED}[build] ERROR:${RESET} $*" >&2; exit 1; }

# ── Clean ─────────────────────────────────────────────────────────────────────
if $CLEAN; then
  info "Cleaning build artifacts…"
  rm -rf "$BUILD_DIR"
  (cd "$SCRIPT_DIR" && mvn -q clean 2>/dev/null || true)
  (cd "$SERVER_DIR"  && mvn -q clean 2>/dev/null || true)
  ok "Clean done."
  exit 0
fi

mkdir -p "$LIB_DIR" "$LOG_DIR"

# ── Detect platform ───────────────────────────────────────────────────────────
OS="$(uname -s)"
case "$OS" in
  Linux*)   PLATFORM="linux"  ; LIB_EXT="so"   ;;
  Darwin*)  PLATFORM="macos"  ; LIB_EXT="dylib" ;;
  MINGW*|CYGWIN*|MSYS*)
            PLATFORM="windows"; LIB_EXT="dll"   ;;
  *)        die "Unsupported OS: $OS" ;;
esac
info "Platform: $PLATFORM ($OS)"

# ── Check prerequisites ───────────────────────────────────────────────────────
require() {
  command -v "$1" &>/dev/null || die "$1 not found on PATH. Install it and retry."
}
require cmake
require java
require mvn

if [ -z "${JAVA_HOME:-}" ]; then
  # Try to find JAVA_HOME automatically
  if command -v java &>/dev/null; then
    JAVA_HOME="$(java -XshowSettings:all -version 2>&1 \
      | grep "java.home" | awk '{print $NF}')"
    export JAVA_HOME
  fi
fi
[ -n "${JAVA_HOME:-}" ] || die "JAVA_HOME is not set. Set it to your JDK 21 install."
info "JAVA_HOME: $JAVA_HOME"

# Detect C++ compiler
if command -v g++ &>/dev/null; then
  CXX_CMD="g++"
elif command -v clang++ &>/dev/null; then
  CXX_CMD="clang++"
else
  die "No C++ compiler found (g++ or clang++ required)."
fi
info "C++ compiler: $CXX_CMD ($(${CXX_CMD} --version | head -1))"

# ── Step 1: Build C++ shared library ─────────────────────────────────────────
if ! $SKIP_CPP; then
  info "Configuring C++ build (CMake)…"
  cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR/cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_LIBRARY_OUTPUT_DIRECTORY="$LIB_DIR" \
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$LIB_DIR" \
    2>&1 | tee "$LOG_DIR/cmake_configure.log"

  info "Compiling C++ engine…"
  CPU_COUNT=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
  cmake --build "$BUILD_DIR/cmake" --config Release -j "$CPU_COUNT" \
    2>&1 | tee "$LOG_DIR/cmake_build.log"

  LIB_FILE="$LIB_DIR/libmathengine.$LIB_EXT"
  [ "$PLATFORM" = "windows" ] && LIB_FILE="$LIB_DIR/mathengine.dll"
  [ -f "$LIB_FILE" ] && ok "C++ engine built: $LIB_FILE" \
                      || die "Expected library not found: $LIB_FILE"
else
  warn "Skipping C++ build (--skip-cpp)."
fi

# ── Step 2: Start Spring Boot server (optional) ───────────────────────────────
if $START_SERVER; then
  info "Starting Spring Boot server…"
  mvn -f "$SERVER_DIR/pom.xml" spring-boot:run \
    -Dspring-boot.run.jvmArguments="-Djava.library.path=$LIB_DIR" \
    > "$LOG_DIR/server.log" 2>&1 &
  SERVER_PID=$!
  echo "$SERVER_PID" > "$BUILD_DIR/server.pid"
  # Wait for the server to be ready (up to 30 s)
  for i in $(seq 1 30); do
    curl -sf http://localhost:8080/api/engine/status &>/dev/null && break
    sleep 1
  done
  curl -sf http://localhost:8080/api/engine/status &>/dev/null \
    && ok "Server ready at http://localhost:8080 (PID $SERVER_PID)" \
    || warn "Server may not be ready yet — check $LOG_DIR/server.log"
fi

# ── Step 3: Launch JavaFX desktop app ─────────────────────────────────────────
if ! $SERVER_ONLY; then
  info "Launching JavaFX desktop application…"
  mvn -f "$SCRIPT_DIR/pom.xml" javafx:run \
    -Djava.library.path="$LIB_DIR" \
    2>&1 | tee "$LOG_DIR/javafx.log"
fi

ok "Done."
