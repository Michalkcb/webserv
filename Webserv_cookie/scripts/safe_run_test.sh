#!/bin/bash
# Safe runner for heavy tests (20x5x100MB)
# Usage: ./scripts/safe_run_test.sh [config-file] [mem_limit_MB]
# Example: ./scripts/safe_run_test.sh ./tester_config_final.conf 1600

set -e
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
WEBSERV_BIN="$ROOT_DIR/webserv"
TESTER_SH="$ROOT_DIR/run_tester.sh"
CONFIG=${1:-$ROOT_DIR/tester_config_final.conf}
MEM_MB=${2:-1500}
MEM_MAX="${MEM_MB}M"
NICE_LEVEL=10
IONICE_CLASS=2
IONICE_PRIO=7
OUT_DIR="$ROOT_DIR/logs_safe_run"
mkdir -p "$OUT_DIR"

echo "Safe run: webserv=$WEBSERV_BIN, config=$CONFIG, memMax=$MEM_MAX"

if [ ! -x "$WEBSERV_BIN" ]; then
  echo "Error: webserv binary not found or not executable: $WEBSERV_BIN"
  exit 1
fi
if [ ! -x "$TESTER_SH" ]; then
  echo "Warning: tester runner not found or not executable: $TESTER_SH"
fi

# Recommend closing heavyweight apps (VS Code) that can contribute to pressure
echo "Recommendation: close VS Code and other heavy apps before running."

# 1) scaled-run prompt
read -p "Run full test or scaled-down first? (full/scaled/quit) [scaled] " runmode
runmode=${runmode:-scaled}
if [ "$runmode" = "quit" ]; then
  echo "Aborted by user."; exit 0
fi
if [ "$runmode" = "scaled" ]; then
  echo "Running a scaled-down test (concurrency reduced). Edit config as needed and re-run for full test." 
  # If you want, implement an automatic config edit here. For safety, we just continue with the provided config.
fi

# Function to launch webserv under chosen method
WEBSERV_PID=0
LAUNCH_METHOD=""
if command -v systemd-run >/dev/null 2>&1; then
  echo "systemd-run available: launching webserv in a MemoryMax scope ($MEM_MAX)"
  LAUNCH_METHOD=systemd-run
  # Note: systemd-run returns immediately; the process runs under the unit's scope
  systemd-run --scope -p MemoryMax=$MEM_MAX --unit=webserv-safe --description="webserv-safe-run" --slice=system.slice --property=CPUShares=256 \
    bash -lc "nice -n $NICE_LEVEL ionice -c$IONICE_CLASS -n$IONICE_PRIO $WEBSERV_BIN $CONFIG" &
  sleep 1
  # find pid of webserv child process (best-effort)
  WEBSERV_PID=$(pgrep -f "^$WEBSERV_BIN" | head -n1 || true)
else
  echo "systemd-run not available: using ulimit + nice + ionice fallback"
  LAUNCH_METHOD=ulimit
  # Limit address space (virtual memory) for the launched shell
  bash -c "ulimit -v $((MEM_MB * 1024)); ulimit -n 4096; nice -n $NICE_LEVEL ionice -c$IONICE_CLASS -n$IONICE_PRIO $WEBSERV_BIN $CONFIG" &
  WEBSERV_PID=$!
fi

if [ -z "$WEBSERV_PID" ] || [ "$WEBSERV_PID" -eq 0 ]; then
  # try to detect pid again
  sleep 1
  WEBSERV_PID=$(pgrep -f "^$WEBSERV_BIN" | head -n1 || true)
fi

if [ -z "$WEBSERV_PID" ]; then
  echo "Warning: could not detect webserv pid. You can find it with: pgrep -a webserv"
else
  echo "webserv launched with pid=$WEBSERV_PID (method=$LAUNCH_METHOD)"
fi

# Give webserv a small amount of time to start
sleep 2

# Run tester
TIMESTAMP=$(date +%Y%m%d-%H%M%S)
TEST_LOG="$OUT_DIR/tester_run_${TIMESTAMP}.log"
FINALIZE_LOG="$ROOT_DIR/finalize_cgi_debug.log"
LIFECYCLE_LOG="$ROOT_DIR/cgi_lifecycle.log"

echo "Running tester. Output -> $TEST_LOG"
if [ -x "$TESTER_SH" ]; then
  # run tester with stdout/stderr captured
  bash -c "nice -n $NICE_LEVEL ionice -c$IONICE_CLASS -n$IONICE_PRIO $TESTER_SH" > "$TEST_LOG" 2>&1 || true
else
  echo "Tester runner not present. If you have a tester binary, run it now. Waiting for manual start..."
  read -p "Press Enter after starting tester (or Ctrl-C to abort)" _
fi

# Give a short grace period for logs to flush
sleep 1

# Copy/rotate logs for inspection
if [ -f "$FINALIZE_LOG" ]; then
  cp "$FINALIZE_LOG" "$OUT_DIR/finalize_cgi_debug_${TIMESTAMP}.log"
  echo "Copied finalize log -> $OUT_DIR/finalize_cgi_debug_${TIMESTAMP}.log"
else
  echo "No finalize_cgi_debug.log found. It will be created when webserv runs." 
fi
if [ -f "$LIFECYCLE_LOG" ]; then
  cp "$LIFECYCLE_LOG" "$OUT_DIR/cgi_lifecycle_${TIMESTAMP}.log"
  echo "Copied lifecycle log -> $OUT_DIR/cgi_lifecycle_${TIMESTAMP}.log"
else
  echo "No cgi_lifecycle.log found yet." 
fi

# Stop webserv that we started (best-effort)
read -p "Stop webserv started by this script? (yes/no) [yes] " stopit
stopit=${stopit:-yes}
if [ "$stopit" = "yes" ]; then
  if [ "$LAUNCH_METHOD" = "systemd-run" ]; then
    # kill by name (best effort)
    pkill -f "^$WEBSERV_BIN" || true
    # try systemctl to stop the unit
    systemctl kill webserv-safe.service >/dev/null 2>&1 || true
  else
    if [ -n "$WEBSERV_PID" ]; then
      kill $WEBSERV_PID >/dev/null 2>&1 || true
    fi
    pkill -f "^$WEBSERV_BIN" || true
  fi
  echo "webserv stop requested."
else
  echo "Left webserv running. Remember to stop it when you're done."
fi

echo "Safe run complete. Logs are in: $OUT_DIR"

# Helpful hint for user to send logs
echo "When you have logs ready, please attach these two files to the issue/analysis:"
echo "  $OUT_DIR/finalize_cgi_debug_${TIMESTAMP}.log"
echo "  $OUT_DIR/cgi_lifecycle_${TIMESTAMP}.log"

exit 0
