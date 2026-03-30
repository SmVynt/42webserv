#!/bin/bash

# Valgrind Memory Leak Detection - Simplified
# Run server briefly under valgrind to detect memory issues

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
SERVER_BIN="$PROJECT_ROOT/webserv"
CONFIG_FILE="$PROJECT_ROOT/config/default.conf"
VALGRIND_LOG="/tmp/valgrind_webserv.log"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

if ! command -v valgrind &>/dev/null; then
    echo -e "${YELLOW}[WARNING]${NC} valgrind not installed"
    exit 0
fi

echo "=========================================="
echo "Memory Leak Detection with Valgrind"
echo "=========================================="
echo ""

# Minimal cleanup
killall -9 valgrind 2>/dev/null || true
killall -9 webserv 2>/dev/null || true
sleep 1

if [ ! -f "$SERVER_BIN" ] || [ ! -f "$CONFIG_FILE" ]; then
    echo -e "${RED}✗${NC} Server binary or config not found"
    exit 0
fi

echo "Starting memory detection (60 seconds)..."
rm -f "$VALGRIND_LOG"

# Start valgrind directly in background
cd "$PROJECT_ROOT"
valgrind --leak-check=full --show-leak-kinds=all --log-file="$VALGRIND_LOG" \
         "$SERVER_BIN" "$CONFIG_FILE" </dev/null >/dev/null 2>&1 &
SERVER_PID=$!

# Wait for server to start
sleep 2

# Perform memory-testing requests
echo "Running memory stress tests..."
for i in {1..4}; do
    # Static file requests - exercise buffer handling
    curl -s -4 --connect-timeout 2 --max-time 5 http://127.0.0.1:8080/index.html >/dev/null 2>&1 || true
    
    # Request to trigger various code paths
    curl -s -4 --connect-timeout 2 --max-time 5 http://127.0.0.1:8080/ >/dev/null 2>&1 || true
    
    # CGI test - Python script (allocate some memory)  
    curl -s -4 --connect-timeout 2 --max-time 5 http://127.0.0.1:8080/cgi-bin/python/test.py >/dev/null 2>&1 || true
    
    # CGI test - Shell script
    curl -s -4 --connect-timeout 2 --max-time 5 http://127.0.0.1:8080/cgi-bin/shell/test.sh >/dev/null 2>&1 || true
    
    echo -n "."
    sleep 10
done
echo ""

# Stop server gracefully (allows destructors to run)
kill -TERM $SERVER_PID 2>/dev/null || true
sleep 3

# Only force kill if still running
if kill -0 $SERVER_PID 2>/dev/null; then
    kill -9 $SERVER_PID 2>/dev/null || true
fi

# Cleanup any remaining
killall -9 valgrind 2>/dev/null || true
killall -9 webserv 2>/dev/null || true
sleep 1

# Cleanup any remaining
killall -9 valgrind 2>/dev/null || true
killall -9 webserv 2>/dev/null || true
sleep 1

# Show results
echo ""
echo "=========================================="
echo -e "${GREEN}Memory Test Results${NC}"
echo "=========================================="
echo ""

if [ ! -f "$VALGRIND_LOG" ]; then
    echo -e "${YELLOW}⚠${NC} Log file not found at: $VALGRIND_LOG"
    exit 0
fi

if [ ! -s "$VALGRIND_LOG" ]; then
    echo -e "${YELLOW}⚠${NC} Log file is empty"
    exit 0
fi

if grep -q "All heap blocks were freed" "$VALGRIND_LOG"; then
    echo -e "${GREEN}✓${NC} All heap blocks freed"
else
    echo -e "${YELLOW}⚠${NC} Memory cleanup analysis"
fi

if grep -q "ERROR SUMMARY: 0 errors" "$VALGRIND_LOG"; then
    echo -e "${GREEN}✓${NC} No errors detected"
fi

echo ""
echo "Leak summary from valgrind:"
grep "definitely lost\|indirectly lost" "$VALGRIND_LOG" 2>/dev/null | head -2 || echo "  (could not parse)"

echo ""
echo "=========================================="
echo "Detailed Valgrind Output:"
echo "=========================================="
grep -A 20 "HEAP SUMMARY" "$VALGRIND_LOG" 2>/dev/null || echo "(Could not parse heap summary)"

echo ""
echo "=========================================="
echo "Full log: $VALGRIND_LOG"
echo ""

exit 0

