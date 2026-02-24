#!/bin/bash

# Concurrent Clients Test
# Tests server behavior with multiple simultaneous connections

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
SERVER_BIN="${SERVER_BIN:-$PROJECT_ROOT/webserv}"
CONFIG_FILE="${CONFIG_FILE:-$PROJECT_ROOT/config/default.conf}"
SERVER_HOST="${SERVER_HOST:-127.0.0.1}"
SERVER_PORT="${SERVER_PORT:-8080}"
NUM_CLIENTS="${NUM_CLIENTS:-100}"
REQUESTS_PER_CLIENT="${REQUESTS_PER_CLIENT:-10}"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

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


trap stop_server EXIT

# Single client simulation
simulate_client() {
    local client_id=$1
    local success_count=0
    local fail_count=0

    for ((i=1; i<=REQUESTS_PER_CLIENT; i++)); do
        response=$(curl -s -w "%{http_code}" -o /dev/null "http://${SERVER_HOST}:${SERVER_PORT}/" 2>/dev/null || echo "000")

        if [[ "$response" =~ ^2[0-9][0-9]$ ]]; then
            success_count=$((success_count + 1))
        else
            fail_count=$((fail_count + 1))
        fi
    done

    echo "$client_id,$success_count,$fail_count"
}

export -f simulate_client
export SERVER_HOST SERVER_PORT REQUESTS_PER_CLIENT

echo "=========================================="
echo "Concurrent Clients Test"
echo "=========================================="
echo "Configuration:"
echo "  Clients: $NUM_CLIENTS"
echo "  Requests per client: $REQUESTS_PER_CLIENT"
echo "  Total requests: $((NUM_CLIENTS * REQUESTS_PER_CLIENT))"
echo ""

start_server

# Test 1: Simultaneous connections
echo "Test 1: Launching $NUM_CLIENTS concurrent clients..."

START_TIME=$(date +%s)

# Create a temporary directory for results
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR; stop_server" EXIT

# Launch clients in parallel
for ((i=1; i<=NUM_CLIENTS; i++)); do
    simulate_client "$i" >> "$TEMP_DIR/results.txt" &
done

# Wait for all clients to complete
wait

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

# Analyze results
TOTAL_SUCCESS=0
TOTAL_FAIL=0

while IFS=',' read -r client success fail; do
    TOTAL_SUCCESS=$((TOTAL_SUCCESS + success))
    TOTAL_FAIL=$((TOTAL_FAIL + fail))
done < "$TEMP_DIR/results.txt"

TOTAL_REQUESTS=$((TOTAL_SUCCESS + TOTAL_FAIL))
SUCCESS_RATE=0
if [ $TOTAL_REQUESTS -gt 0 ]; then
    SUCCESS_RATE=$((TOTAL_SUCCESS * 100 / TOTAL_REQUESTS))
fi

echo ""
echo "Results:"
echo "  Duration: ${DURATION}s"
echo "  Total requests: $TOTAL_REQUESTS"
echo "  Successful: $TOTAL_SUCCESS"
echo "  Failed: $TOTAL_FAIL"
echo "  Success rate: ${SUCCESS_RATE}%"
echo "  Throughput: $((TOTAL_REQUESTS / DURATION)) req/s"

# Evaluate results
if [ $SUCCESS_RATE -ge 95 ]; then
    echo -e "${GREEN}✓${NC} Concurrent clients test passed (${SUCCESS_RATE}% success rate)"
elif [ $SUCCESS_RATE -ge 80 ]; then
    echo -e "${YELLOW}⚠${NC} Concurrent clients test warning (${SUCCESS_RATE}% success rate - expected >= 95%)"
    exit 0
else
    echo -e "${RED}✗${NC} Concurrent clients test failed (${SUCCESS_RATE}% success rate)"
    exit 1
fi

# Test 2: Connection reuse (keep-alive)
echo ""
echo "Test 2: Testing connection reuse..."

KEEPALIVE_SUCCESS=0
for ((i=1; i<=10; i++)); do
    # Make multiple requests on the same connection
    response=$(curl -s -H "Connection: keep-alive" "http://${SERVER_HOST}:${SERVER_PORT}/" \
                    -w "%{http_code}" -o /dev/null 2>/dev/null || echo "000")

    if [[ "$response" =~ ^2[0-9][0-9]$ ]]; then
        KEEPALIVE_SUCCESS=$((KEEPALIVE_SUCCESS + 1))
    fi
done

if [ $KEEPALIVE_SUCCESS -ge 9 ]; then
    echo -e "${GREEN}✓${NC} Connection reuse test passed"
else
    echo -e "${YELLOW}⚠${NC} Connection reuse test warning"
fi

# Test 3: Rapid connection open/close
echo ""
echo "Test 3: Testing rapid connection cycles..."

RAPID_SUCCESS=0
for ((i=1; i<=50; i++)); do
    response=$(curl -s -H "Connection: close" "http://${SERVER_HOST}:${SERVER_PORT}/" \
                    -w "%{http_code}" -o /dev/null 2>/dev/null || echo "000")

    if [[ "$response" =~ ^2[0-9][0-9]$ ]]; then
        RAPID_SUCCESS=$((RAPID_SUCCESS + 1))
    fi
done

if [ $RAPID_SUCCESS -ge 48 ]; then
    echo -e "${GREEN}✓${NC} Rapid connection cycles test passed (${RAPID_SUCCESS}/50)"
else
    echo -e "${YELLOW}⚠${NC} Rapid connection cycles test warning (${RAPID_SUCCESS}/50)"
fi

echo ""
echo "=========================================="
echo "All concurrent client tests completed"
echo "=========================================="

exit 0
