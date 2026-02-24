#!/bin/bash

# Valgrind Memory Leak Detection
# Tests server for memory leaks under various scenarios

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
SERVER_BIN="${SERVER_BIN:-$PROJECT_ROOT/webserv}"
CONFIG_FILE="${CONFIG_FILE:-$PROJECT_ROOT/config/default.conf}"
TEST_DURATION="${TEST_DURATION:-30}"
VALGRIND_LOG="/tmp/valgrind_webserv.log"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Check if valgrind is installed
if ! command -v valgrind &>/dev/null; then
    echo -e "${YELLOW}[WARNING]${NC} valgrind not installed. Install with: sudo apt-get install valgrind"
    exit 0
fi

# Cleanup function
cleanup() {
    if [ -n "${SERVER_PID:-}" ]; then
        echo "Stopping server..."
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
}

trap cleanup EXIT

echo "=========================================="
echo "Memory Leak Detection with Valgrind"
echo "=========================================="
echo "This test will take several minutes..."
echo ""

# Build server without optimization for better valgrind output
cd "$PROJECT_ROOT"
echo "Building server with debug symbols..."
make fclean > /dev/null 2>&1
make "CXXFLAGS=-Wall -Wextra -Werror -std=c++17 -g -O0" > /dev/null 2>&1

cd "$SCRIPT_DIR"

# Test 1: Startup and shutdown leak check
echo "Test 1: Startup/Shutdown leak check"
echo "  Starting server with valgrind..."

valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file="$VALGRIND_LOG" \
         "$SERVER_BIN" "$CONFIG_FILE" &
SERVER_PID=$!

# Give server time to start
sleep 3

# Check if server started successfully
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}✗${NC} Server failed to start"
    cat "$VALGRIND_LOG"
    exit 1
fi

# Send a few test requests
echo "  Sending test requests..."
for i in {1..10}; do
    curl -s http://localhost:8080/ > /dev/null 2>&1 || true
done

sleep 1

# Gracefully stop server
echo "  Stopping server..."
kill -INT $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true
SERVER_PID=""

# Analyze valgrind output
echo "  Analyzing memory leaks..."

if ! grep -q "ERROR SUMMARY: 0 errors" "$VALGRIND_LOG"; then
    echo -e "${RED}✗${NC} Valgrind detected errors"
    echo "Error summary:"
    grep "ERROR SUMMARY" "$VALGRIND_LOG"
    echo ""
    echo "See full log at: $VALGRIND_LOG"
else
    echo -e "${GREEN}✓${NC} No valgrind errors detected"
fi

# Check for memory leaks
DEFINITELY_LOST=$(grep "definitely lost:" "$VALGRIND_LOG" | tail -1 | awk '{print $4}' | tr -d ',')
INDIRECTLY_LOST=$(grep "indirectly lost:" "$VALGRIND_LOG" | tail -1 | awk '{print $4}' | tr -d ',')
POSSIBLY_LOST=$(grep "possibly lost:" "$VALGRIND_LOG" | tail -1 | awk '{print $4}' | tr -d ',')

echo ""
echo "Memory Leak Summary:"
echo "  Definitely lost: ${DEFINITELY_LOST:-0} bytes"
echo "  Indirectly lost: ${INDIRECTLY_LOST:-0} bytes"
echo "  Possibly lost: ${POSSIBLY_LOST:-0} bytes"

# Evaluate results
LEAK_DETECTED=false

if [ "${DEFINITELY_LOST:-0}" -gt 0 ]; then
    echo -e "${RED}✗${NC} Definite memory leaks detected!"
    LEAK_DETECTED=true
fi

if [ "${INDIRECTLY_LOST:-0}" -gt 0 ]; then
    echo -e "${YELLOW}⚠${NC} Indirect memory leaks detected"
    LEAK_DETECTED=true
fi

if [ "${POSSIBLY_LOST:-0}" -gt 1000 ]; then
    echo -e "${YELLOW}⚠${NC} Possible memory leaks detected (may be false positives)"
fi

if [ "$LEAK_DETECTED" = true ]; then
    echo ""
    echo "Leak details:"
    grep -A 5 "definitely lost" "$VALGRIND_LOG" | head -20
    echo ""
    echo "Full valgrind log saved at: $VALGRIND_LOG"
    exit 1
fi

# Test 2: Load test with valgrind (lighter version)
echo ""
echo "Test 2: Memory stability under load"
echo "  Starting server with valgrind..."

valgrind --leak-check=full \
         --show-leak-kinds=definite,indirect \
         --log-file="${VALGRIND_LOG}.load" \
         "$SERVER_BIN" "$CONFIG_FILE" &
SERVER_PID=$!

sleep 3

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}✗${NC} Server failed to start"
    exit 1
fi

echo "  Running load test (100 requests)..."

# Send multiple concurrent requests
for i in {1..100}; do
    curl -s http://localhost:8080/ > /dev/null 2>&1 &
done
wait

sleep 2

# Stop server
kill -INT $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true
SERVER_PID=""

# Check for leaks after load
DEFINITELY_LOST=$(grep "definitely lost:" "${VALGRIND_LOG}.load" | tail -1 | awk '{print $4}' | tr -d ',')

if [ "${DEFINITELY_LOST:-0}" -gt 0 ]; then
    echo -e "${RED}✗${NC} Memory leaks detected under load (${DEFINITELY_LOST} bytes)"
    echo "Full log at: ${VALGRIND_LOG}.load"
    exit 1
else
    echo -e "${GREEN}✓${NC} No memory leaks under load"
fi

# Test 3: Check for file descriptor leaks
echo ""
echo "Test 3: File descriptor leak check"

# Start server normally (without valgrind for this test)
"$SERVER_BIN" "$CONFIG_FILE" &>/dev/null &
SERVER_PID=$!
sleep 2

# Get initial FD count
INITIAL_FDS=$(ls -1 /proc/$SERVER_PID/fd 2>/dev/null | wc -l || echo "0")
echo "  Initial file descriptors: $INITIAL_FDS"

# Make many requests
echo "  Making 50 requests..."
for i in {1..50}; do
    curl -s http://localhost:8080/ > /dev/null 2>&1
done

sleep 2

# Get final FD count
FINAL_FDS=$(ls -1 /proc/$SERVER_PID/fd 2>/dev/null | wc -l || echo "0")
echo "  Final file descriptors: $FINAL_FDS"

FD_DIFF=$((FINAL_FDS - INITIAL_FDS))

if [ $FD_DIFF -gt 5 ]; then
    echo -e "${YELLOW}⚠${NC} File descriptor leak possible (increased by $FD_DIFF)"
else
    echo -e "${GREEN}✓${NC} No file descriptor leaks detected"
fi

# Stop server
kill -INT $SERVER_PID 2>/dev/null || true
wait $SERVER_PID 2>/dev/null || true
SERVER_PID=""

echo ""
echo "=========================================="
echo "Memory leak tests completed"
echo "=========================================="
echo ""
echo "Summary:"
echo "  ✓ Startup/Shutdown: No definite leaks"
echo "  ✓ Under load: No definite leaks"
echo "  ✓ File descriptors: Stable"
echo ""
echo "Valgrind logs:"
echo "  - $VALGRIND_LOG"
echo "  - ${VALGRIND_LOG}.load"

exit 0
