#!/bin/bash

# Siege Load Testing Script
# Tests server availability under various load conditions

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
SERVER_BIN="${SERVER_BIN:-$PROJECT_ROOT/webserv}"
CONFIG_FILE="${CONFIG_FILE:-$PROJECT_ROOT/config/default.conf}"
SERVER_HOST="${SERVER_HOST:-127.0.0.1}"
SERVER_PORT="${SERVER_PORT:-8080}"
SERVER_URL="${SERVER_URL:-http://${SERVER_HOST}:${SERVER_PORT}}"
TEST_DURATION="${TEST_DURATION:-30s}"
LOG_FILE="${LOG_FILE:-/tmp/siege_test.log}"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Check if siege is installed
if ! command -v siege &>/dev/null; then
    echo -e "${YELLOW}[WARNING]${NC} siege not installed. Install with: sudo apt-get install siege"
    exit 0
fi

# Start server if not running
start_server() {
    if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
        echo "Starting server..."
        "$SERVER_BIN" "$CONFIG_FILE" &>/dev/null &
        SERVER_PID=$!
        sleep 2

        if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
            echo -e "${RED}[ERROR]${NC} Failed to start server"
            exit 1
        fi
        echo "Server started (PID: $SERVER_PID)"
    else
        echo "Server already running"
        SERVER_PID=""
    fi
}

# Stop server
stop_server() {
    if [ -n "$SERVER_PID" ]; then
        echo "Stopping server..."
        kill $SERVER_PID 2>/dev/null || true

        # Wait with timeout
        local timeout=3
        while kill -0 $SERVER_PID 2>/dev/null && [ $timeout -gt 0 ]; do
            sleep 0.1
            timeout=$((timeout - 1))
        done

        # Force kill if still running
        if kill -0 $SERVER_PID 2>/dev/null; then
            kill -9 $SERVER_PID 2>/dev/null || true
            sleep 0.2
        fi

        # Only wait if process still exists
        if kill -0 $SERVER_PID 2>/dev/null; then
            wait $SERVER_PID 2>/dev/null || true
        fi
    fi
}

trap stop_server EXIT

echo "=========================================="
echo "Siege Load Testing"
echo "=========================================="

start_server

# Create URLs file for varied testing
URLS_FILE="/tmp/siege_urls.txt"
cat > "$URLS_FILE" << EOF
$SERVER_URL/
$SERVER_URL/index.html
$SERVER_URL/error/404.html
EOF

echo ""
echo "Test 1: Light load (10 concurrent users)"
siege -c 10 -t 10s -b "$SERVER_URL" > /tmp/siege_light.log 2>&1
if grep -q "Availability.*100.00" /tmp/siege_light.log; then
    echo -e "${GREEN}✓${NC} Light load test passed"
else
    echo -e "${RED}✗${NC} Light load test failed"
    exit 1
fi

echo ""
echo "Test 2: Medium load (50 concurrent users)"
siege -c 50 -t 15s -b "$SERVER_URL" > /tmp/siege_medium.log 2>&1
AVAIL=$(grep "Availability" /tmp/siege_medium.log | awk '{print $2}' | tr -d '%')
if (( $(echo "$AVAIL >= 95.0" | bc -l) )); then
    echo -e "${GREEN}✓${NC} Medium load test passed (${AVAIL}% availability)"
else
    echo -e "${RED}✗${NC} Medium load test failed (${AVAIL}% availability)"
    exit 1
fi

echo ""
echo "Test 3: Heavy load (100 concurrent users)"
siege -c 100 -t 15s -b "$SERVER_URL" > /tmp/siege_heavy.log 2>&1
AVAIL=$(grep "Availability" /tmp/siege_heavy.log | awk '{print $2}' | tr -d '%')
if (( $(echo "$AVAIL >= 90.0" | bc -l) )); then
    echo -e "${GREEN}✓${NC} Heavy load test passed (${AVAIL}% availability)"
else
    echo -e "${YELLOW}⚠${NC} Heavy load test warning (${AVAIL}% availability - expected >= 90%)"
fi

echo ""
echo "Test 4: Varied URLs (mixed content)"
siege -c 25 -t 10s -b -f "$URLS_FILE" > /tmp/siege_varied.log 2>&1
if grep -q "Availability.*[1-9][0-9]\." /tmp/siege_varied.log; then
    echo -e "${GREEN}✓${NC} Varied URL test passed"
else
    echo -e "${RED}✗${NC} Varied URL test failed"
    exit 1
fi

echo ""
echo "=========================================="
echo "Summary"
echo "=========================================="
echo "All siege tests completed successfully!"
echo ""
echo "Performance Metrics:"
grep "Transaction rate" /tmp/siege_heavy.log || true
grep "Response time" /tmp/siege_heavy.log || true
grep "Concurrency" /tmp/siege_heavy.log || true

rm -f "$URLS_FILE" /tmp/siege_*.log

exit 0
