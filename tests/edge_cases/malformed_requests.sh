#!/bin/bash

# Malformed HTTP Request Testing
# Tests server resilience against invalid and malformed requests

set -euo pipefail

# Configuration
SERVER_HOST="${SERVER_HOST:-localhost}"
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
        ../webserv ../config/default.conf &>/dev/null &
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

# Send raw request and check if server survives
send_raw_request() {
    local request="$1"
    local description="$2"
    local expect_response="${3:-false}"

    echo -n "  Testing: $description... "

    # Send request and capture response
    response=$(echo -ne "$request" | nc -w 2 "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null || echo "")

    # Check if server is still alive
    if nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
        if [ "$expect_response" = "true" ]; then
            if [ -n "$response" ]; then
                echo -e "${GREEN}✓${NC} (Server survived, response received)"
            else
                echo -e "${YELLOW}⚠${NC} (Server survived, no response)"
            fi
        else
            echo -e "${GREEN}✓${NC} (Server survived)"
        fi
        return 0
    else
        echo -e "${RED}✗${NC} (Server crashed!)"
        return 1
    fi
}

echo "=========================================="
echo "Malformed HTTP Request Tests"
echo "=========================================="

start_server

# Test 1: Missing HTTP version
echo ""
echo "Test 1: Missing or invalid HTTP version"
send_raw_request "GET / \r\n\r\n" "Missing HTTP version"
send_raw_request "GET / HTTP/0.9\r\n\r\n" "HTTP/0.9"
send_raw_request "GET / HTTP/3.0\r\n\r\n" "HTTP/3.0"
send_raw_request "GET / NOTHTTP/1.1\r\n\r\n" "Invalid protocol"

# Test 2: Invalid methods
echo ""
echo "Test 2: Invalid or unsupported methods"
send_raw_request "INVALID / HTTP/1.1\r\nHost: localhost\r\n\r\n" "Invalid method"
send_raw_request "G3T / HTTP/1.1\r\nHost: localhost\r\n\r\n" "Malformed GET"
send_raw_request "TRACE / HTTP/1.1\r\nHost: localhost\r\n\r\n" "TRACE method"
send_raw_request "CONNECT localhost:8080 HTTP/1.1\r\n\r\n" "CONNECT method"

# Test 3: Missing required headers
echo ""
echo "Test 3: Missing required headers"
send_raw_request "GET / HTTP/1.1\r\n\r\n" "Missing Host header"
send_raw_request "POST / HTTP/1.1\r\nHost: localhost\r\n\r\n" "POST missing Content-Length"

# Test 4: Malformed headers
echo ""
echo "Test 4: Malformed headers"
send_raw_request "GET / HTTP/1.1\r\nNoColonHeader\r\nHost: localhost\r\n\r\n" "Header without colon"
send_raw_request "GET / HTTP/1.1\r\nHost localhost\r\n\r\n" "Host without colon"
send_raw_request "GET / HTTP/1.1\r\n: value\r\nHost: localhost\r\n\r\n" "Empty header name"
send_raw_request "GET / HTTP/1.1\r\nHost: localhost\r\nX-Custom:\r\n\r\n" "Empty header value"

# Test 5: Extremely long headers
echo ""
echo "Test 5: Extremely long values"
LONG_VALUE=$(printf 'A%.0s' {1..10000})
send_raw_request "GET / HTTP/1.1\r\nHost: localhost\r\nX-Long: $LONG_VALUE\r\n\r\n" "Very long header value"

LONG_URI=$(printf '/path%.0s' {1..1000})
send_raw_request "GET $LONG_URI HTTP/1.1\r\nHost: localhost\r\n\r\n" "Very long URI"

# Test 6: Duplicate headers
echo ""
echo "Test 6: Duplicate headers"
send_raw_request "GET / HTTP/1.1\r\nHost: localhost\r\nHost: example.com\r\n\r\n" "Duplicate Host headers"
send_raw_request "GET / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\nContent-Length: 20\r\n\r\n" "Duplicate Content-Length"

# Test 7: Invalid line endings
echo ""
echo "Test 7: Invalid line endings"
send_raw_request "GET / HTTP/1.1\nHost: localhost\n\n" "LF only (no CR)"
send_raw_request "GET / HTTP/1.1\rHost: localhost\r\r" "CR only (no LF)"

# Test 8: Empty or whitespace-only requests
echo ""
echo "Test 8: Empty/whitespace requests"
send_raw_request "\r\n\r\n" "Empty request"
send_raw_request "   \r\n\r\n" "Whitespace only"
send_raw_request "\r\n" "Single CRLF"

# Test 9: Request with null bytes
echo ""
echo "Test 9: Requests with special characters"
send_raw_request "GET /\x00path HTTP/1.1\r\nHost: localhost\r\n\r\n" "Null byte in path"
send_raw_request "GET / HTTP/1.1\r\nHost: local\x00host\r\n\r\n" "Null byte in header"

# Test 10: Incomplete requests
echo ""
echo "Test 10: Incomplete requests"
send_raw_request "GET / HTTP/1.1\r\nHost: localhost\r\n" "Missing final CRLF"
send_raw_request "GET / HTTP/1.1\r\nHost: " "Incomplete header"
send_raw_request "GET" "Partial method"

# Test 11: Multiple requests on same connection
echo ""
echo "Test 11: Pipelined/Multiple requests"
send_raw_request "GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n" "Pipelined requests" "true"

# Test 12: Case sensitivity
echo ""
echo "Test 12: Case variations"
send_raw_request "get / HTTP/1.1\r\nHost: localhost\r\n\r\n" "Lowercase method" "true"
send_raw_request "GET / http/1.1\r\nHost: localhost\r\n\r\n" "Lowercase HTTP"
send_raw_request "GET / HTTP/1.1\r\nhost: localhost\r\n\r\n" "Lowercase Host header" "true"

# Test 13: Invalid characters in request line
echo ""
echo "Test 13: Invalid characters"
send_raw_request "GET /<>| HTTP/1.1\r\nHost: localhost\r\n\r\n" "Special chars in path"
send_raw_request "GET /path HTTP/1.1\r\nHost: local host\r\n\r\n" "Space in Host value"

# Test 14: Request smuggling attempts
echo ""
echo "Test 14: Request smuggling patterns"
send_raw_request "GET / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n" "CL + TE headers"
send_raw_request "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\ntest\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n" "Smuggled GET"

# Final check
echo ""
echo "=========================================="
if nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
    echo -e "${GREEN}✓${NC} Server survived all malformed request tests!"
    echo "=========================================="
    exit 0
else
    echo -e "${RED}✗${NC} Server crashed during testing"
    echo "=========================================="
    exit 1
fi
