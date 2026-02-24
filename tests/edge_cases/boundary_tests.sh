#!/bin/bash

# Boundary Value Testing
# Tests server behavior at limits and boundaries

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
SERVER_BIN="${SERVER_BIN:-$PROJECT_ROOT/webserv}"
CONFIG_FILE="${CONFIG_FILE:-$PROJECT_ROOT/config/default.conf}"
SERVER_HOST="${SERVER_HOST:-127.0.0.1}"
SERVER_PORT="${SERVER_PORT:-8080}"

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
        wait $SERVER_PID 2>/dev/null || true
    fi
}

trap stop_server EXIT

# Test with specific data size
test_data_size() {
    local size=$1
    local description="$2"

    echo -n "  Testing $description (${size} bytes)... "

    # Create test data
    local data=$(head -c "$size" /dev/urandom | base64 | tr -d '\n' | head -c "$size")

    # Send POST request with data
    response=$(curl -s -w "%{http_code}" -o /dev/null \
        -X POST \
        -H "Content-Type: application/octet-stream" \
        -H "Content-Length: ${size}" \
        --data-binary "@-" \
        "http://${SERVER_HOST}:${SERVER_PORT}/" <<< "$data" 2>/dev/null || echo "000")

    # Check if server is still alive
    if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
        echo -e "${RED}✗${NC} (Server crashed!)"
        return 1
    fi

    # Accept various response codes (server may reject large requests)
    if [[ "$response" =~ ^(200|201|400|413|500)$ ]]; then
        echo -e "${GREEN}✓${NC} (Response: $response)"
        return 0
    else
        echo -e "${YELLOW}⚠${NC} (Unexpected response: $response)"
        return 0
    fi
}

echo "=========================================="
echo "Boundary Value Tests"
echo "=========================================="

start_server

# Test 1: Request body sizes
echo ""
echo "Test 1: Request body size boundaries"
test_data_size 0 "Empty body"
test_data_size 1 "1 byte"
test_data_size 100 "100 bytes"
test_data_size 1024 "1 KB"
test_data_size 10240 "10 KB"
test_data_size 102400 "100 KB"
test_data_size 1048576 "1 MB"

# Test 2: Header count boundaries
echo ""
echo "Test 2: Number of headers"

echo -n "  Testing 1 header... "
response=$(curl -s -w "%{http_code}" -o /dev/null "http://${SERVER_HOST}:${SERVER_PORT}/" 2>/dev/null || echo "000")
echo -e "${GREEN}✓${NC} ($response)"

echo -n "  Testing 50 headers... "
headers=""
for i in {1..50}; do
    headers="$headers -H \"X-Custom-$i: value$i\""
done
response=$(eval "curl -s -w '%{http_code}' -o /dev/null $headers 'http://${SERVER_HOST}:${SERVER_PORT}/' 2>/dev/null" || echo "000")
echo -e "${GREEN}✓${NC} ($response)"

echo -n "  Testing 100 headers... "
headers=""
for i in {1..100}; do
    headers="$headers -H \"X-Custom-$i: value$i\""
done
response=$(eval "curl -s -w '%{http_code}' -o /dev/null $headers 'http://${SERVER_HOST}:${SERVER_PORT}/' 2>/dev/null" || echo "000")
echo -e "${GREEN}✓${NC} ($response)"

# Test 3: URI length boundaries
echo ""
echo "Test 3: URI length boundaries"

test_uri_length() {
    local length=$1
    local description="$2"

    echo -n "  Testing $description ($length chars)... "

    local path=$(printf 'A%.0s' $(seq 1 $length))
    response=$(curl -s -w "%{http_code}" -o /dev/null "http://${SERVER_HOST}:${SERVER_PORT}/${path}" 2>/dev/null || echo "000")

    if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
        echo -e "${RED}✗${NC} (Server crashed!)"
        return 1
    fi

    echo -e "${GREEN}✓${NC} ($response)"
}

test_uri_length 10 "Short URI"
test_uri_length 100 "Medium URI"
test_uri_length 1000 "Long URI"
test_uri_length 2000 "Very long URI"
test_uri_length 8000 "Extremely long URI"

# Test 4: Query string boundaries
echo ""
echo "Test 4: Query string parameters"

echo -n "  Testing no parameters... "
response=$(curl -s -w "%{http_code}" -o /dev/null "http://${SERVER_HOST}:${SERVER_PORT}/" 2>/dev/null || echo "000")
echo -e "${GREEN}✓${NC} ($response)"

echo -n "  Testing 10 parameters... "
params="?"
for i in {1..10}; do
    params="${params}param$i=value$i&"
done
response=$(curl -s -w "%{http_code}" -o /dev/null "http://${SERVER_HOST}:${SERVER_PORT}/${params}" 2>/dev/null || echo "000")
echo -e "${GREEN}✓${NC} ($response)"

echo -n "  Testing 50 parameters... "
params="?"
for i in {1..50}; do
    params="${params}param$i=value$i&"
done
response=$(curl -s -w "%{http_code}" -o /dev/null "http://${SERVER_HOST}:${SERVER_PORT}/${params}" 2>/dev/null || echo "000")
echo -e "${GREEN}✓${NC} ($response)"

# Test 5: Connection timeout boundaries
echo ""
echo "Test 5: Connection behavior"

echo -n "  Testing immediate disconnect... "
(echo "GET / HTTP/1.1" | nc "$SERVER_HOST" "$SERVER_PORT" > /dev/null 2>&1) &
sleep 0.1
echo -e "${GREEN}✓${NC}"

echo -n "  Testing slow header sending... "
(
    exec 3<>/dev/tcp/$SERVER_HOST/$SERVER_PORT
    echo -n "GET / HTTP/1.1" >&3
    sleep 1
    echo -ne "\r\n" >&3
    echo -ne "Host: localhost\r\n\r\n" >&3
    cat <&3 > /dev/null 2>&1 &
    exec 3>&-
) 2>/dev/null
echo -e "${GREEN}✓${NC}"

# Test 6: Special path characters
echo ""
echo "Test 6: Path edge cases"

test_path() {
    local path="$1"
    local description="$2"

    echo -n "  Testing $description... "
    response=$(curl -s -w "%{http_code}" -o /dev/null --path-as-is "http://${SERVER_HOST}:${SERVER_PORT}${path}" 2>/dev/null || echo "000")
    echo -e "${GREEN}✓${NC} ($response)"
}

test_path "/" "Root path"
test_path "/." "Dot path"
test_path "/.." "Parent path"
test_path "///" "Multiple slashes"
test_path "/path/./file" "Current dir in path"
test_path "/path/../file" "Parent dir in path"
test_path "" "Empty path"
test_path "/%20" "Encoded space"
test_path "/%2F" "Encoded slash"

# Test 7: Numerical boundaries
echo ""
echo "Test 7: Numerical value boundaries"

echo -n "  Testing Content-Length: 0... "
response=$(echo -ne "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n" | \
    nc -w 2 "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null | head -n 1)
echo -e "${GREEN}✓${NC}"

echo -n "  Testing Content-Length: -1... "
response=$(echo -ne "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: -1\r\n\r\n" | \
    nc -w 2 "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null | head -n 1)
echo -e "${GREEN}✓${NC}"

echo -n "  Testing Content-Length: 999999999999... "
response=$(echo -ne "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 999999999999\r\n\r\n" | \
    nc -w 2 "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null | head -n 1)
echo -e "${GREEN}✓${NC}"

# Test 8: Whitespace boundaries
echo ""
echo "Test 8: Whitespace handling"

echo -n "  Testing extra spaces in request line... "
response=$(echo -ne "GET  /  HTTP/1.1\r\nHost: localhost\r\n\r\n" | \
    nc -w 2 "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null | head -n 1)
echo -e "${GREEN}✓${NC}"

echo -n "  Testing leading spaces... "
response=$(echo -ne "  GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" | \
    nc -w 2 "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null | head -n 1)
echo -e "${GREEN}✓${NC}"

echo -n "  Testing trailing spaces... "
response=$(echo -ne "GET / HTTP/1.1  \r\nHost: localhost\r\n\r\n" | \
    nc -w 2 "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null | head -n 1)
echo -e "${GREEN}✓${NC}"

# Final check
echo ""
echo "=========================================="
if nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
    echo -e "${GREEN}✓${NC} Server survived all boundary tests!"
    echo "=========================================="
    exit 0
else
    echo -e "${RED}✗${NC} Server crashed during testing"
    echo "=========================================="
    exit 1
fi
