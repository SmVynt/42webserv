#!/bin/bash

# Apache Bench (ab) Load Testing Script
# Tests server performance metrics under load

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
SERVER_BIN="${SERVER_BIN:-$PROJECT_ROOT/webserv}"
CONFIG_FILE="${CONFIG_FILE:-$PROJECT_ROOT/config/default.conf}"
SERVER_HOST="${SERVER_HOST:-127.0.0.1}"
SERVER_PORT="${SERVER_PORT:-8080}"
SERVER_URL="${SERVER_URL:-http://${SERVER_HOST}:${SERVER_PORT}/}"
LOG_FILE="${LOG_FILE:-/tmp/ab_test.log}"

case "$SERVER_URL" in
	*/) ;;
	*) SERVER_URL="${SERVER_URL}/" ;;
esac

cd $PROJECT_ROOT

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Check if ab is installed
if ! command -v ab &>/dev/null; then
    echo -e "${YELLOW}[WARNING]${NC} ab (Apache Bench) not installed. Install with: sudo apt-get install apache2-utils"
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
echo "Apache Bench Load Testing"
echo "=========================================="

start_server

# Test 1: Basic performance
echo ""
echo "Test 1: Basic performance (1000 requests, 10 concurrent)"
ab -n 1000 -c 10 -q "$SERVER_URL" > /tmp/ab_basic.log 2>&1
FAILED=$(grep "Failed requests:" /tmp/ab_basic.log | awk '{print $3}')
if [ "$FAILED" -eq 0 ]; then
    echo -e "${GREEN}✓${NC} Basic performance test passed (0 failed requests)"
else
    echo -e "${RED}✗${NC} Basic performance test failed ($FAILED failed requests)"
    exit 1
fi

# Test 2: Moderate concurrency
echo ""
echo "Test 2: Moderate concurrency (5000 requests, 50 concurrent)"
ab -n 5000 -c 50 -q "$SERVER_URL" > /tmp/ab_moderate.log 2>&1
FAILED=$(grep "Failed requests:" /tmp/ab_moderate.log | awk '{print $3}')
if [ "$FAILED" -lt 50 ]; then  # Allow up to 1% failure
    echo -e "${GREEN}✓${NC} Moderate concurrency test passed ($FAILED failed requests)"
else
    echo -e "${RED}✗${NC} Moderate concurrency test failed ($FAILED failed requests)"
    exit 1
fi

# Test 3: High concurrency
echo ""
echo "Test 3: High concurrency (10000 requests, 100 concurrent)"
ab -n 10000 -c 100 -q "$SERVER_URL" > /tmp/ab_high.log 2>&1
FAILED=$(grep "Failed requests:" /tmp/ab_high.log | awk '{print $3}')
if [ "$FAILED" -lt 500 ]; then  # Allow up to 5% failure under high load
    echo -e "${GREEN}✓${NC} High concurrency test passed ($FAILED failed requests)"
else
    echo -e "${YELLOW}⚠${NC} High concurrency test warning ($FAILED failed requests - expected < 500)"
fi

# Test 4: Keep-Alive connections
echo ""
echo "Test 4: Keep-Alive connections (2000 requests, 20 concurrent)"
ab -n 2000 -c 20 -k -q "$SERVER_URL" > /tmp/ab_keepalive.log 2>&1
FAILED=$(grep "Failed requests:" /tmp/ab_keepalive.log | awk '{print $3}')
if [ "$FAILED" -eq 0 ]; then
    echo -e "${GREEN}✓${NC} Keep-Alive test passed"
else
    echo -e "${YELLOW}⚠${NC} Keep-Alive test warning ($FAILED failed requests)"
fi

# Test 5: POST requests
echo ""
echo "Test 5: POST requests (500 requests, 10 concurrent)"
echo "test=data" > /tmp/post_data.txt
ab -n 500 -c 10 -p /tmp/post_data.txt -T "application/x-www-form-urlencoded" -q "$SERVER_URL" > /tmp/ab_post.log 2>&1
FAILED=$(grep "Failed requests:" /tmp/ab_post.log | awk '{print $3}')
if [ "$FAILED" -lt 25 ]; then  # Allow up to 5% failure
    echo -e "${GREEN}✓${NC} POST requests test passed ($FAILED failed requests)"
else
    echo -e "${YELLOW}⚠${NC} POST requests test warning ($FAILED failed requests)"
fi
rm -f /tmp/post_data.txt

echo ""
echo "=========================================="
echo "Performance Summary"
echo "=========================================="

# Extract key metrics from high concurrency test
echo "High Concurrency Test Metrics:"
grep "Requests per second:" /tmp/ab_high.log | sed 's/^/  /'
grep "Time per request:" /tmp/ab_high.log | head -1 | sed 's/^/  /'
grep "Transfer rate:" /tmp/ab_high.log | sed 's/^/  /'

echo ""
echo "Response Time Distribution (High Concurrency):"
grep -A 7 "Percentage of the requests served" /tmp/ab_high.log | sed 's/^/  /'

# Cleanup
rm -f /tmp/ab_*.log

exit 0
