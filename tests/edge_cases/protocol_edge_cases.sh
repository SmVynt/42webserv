#!/bin/bash

# Protocol Edge Cases Testing
# Tests HTTP protocol compliance and edge cases

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
SERVER_BIN="${SERVER_BIN:-$PROJECT_ROOT/webserv}"
CONFIG_FILE="${CONFIG_FILE:-$PROJECT_ROOT/config/default.conf}"
SERVER_HOST="${SERVER_HOST:-127.0.0.1}"
SERVER_PORT="${SERVER_PORT:-8080}"

#echo "$PROJECT_ROOT"
cd $PROJECT_ROOT

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
}

trap stop_server EXIT

# Test helper function
test_protocol() {
    local request="$1"
    local description="$2"
    local expect_code="${3:-any}"

    echo -n "  Testing $description... "

    response=$(echo -ne "$request" | nc -w 1 "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null || echo "")

    # Check if server is still alive
    if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
        echo -e "${RED}✗${NC} (Server crashed)"
        return 1
    fi

    # Extract status code if response exists
    if [ -n "$response" ]; then
        status_code=$(echo "$response" | head -n 1 | grep -oP 'HTTP/\d\.\d \K\d+' || echo "000")

        if [ "$expect_code" = "any" ]; then
            echo -e "${GREEN}✓${NC} ($status_code)"
        elif [ "$status_code" = "$expect_code" ]; then
            echo -e "${GREEN}✓${NC} ($status_code)"
        else
            echo -e "${YELLOW}⚠${NC} (Expected $expect_code, got $status_code)"
        fi
    else
        echo -e "${YELLOW}⚠${NC} (No response)"
    fi

    return 0
}

echo "=========================================="
echo "HTTP Protocol Edge Cases"
echo "=========================================="

start_server

# Test 1: HTTP version variations
echo ""
echo "Test 1: HTTP version handling"
test_protocol "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n" "HTTP/1.0 request"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" "HTTP/1.1 request" "200"
test_protocol "GET / HTTP/2.0\r\nHost: localhost\r\n\r\n" "HTTP/2.0 request (unsupported)"
test_protocol "GET / HTTP/1.2\r\nHost: localhost\r\n\r\n" "HTTP/1.2 request (invalid)"

# Test 2: Connection header variations
echo ""
echo "Test 2: Connection header handling"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" "Connection: close"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n" "Connection: keep-alive"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: upgrade\r\n\r\n" "Connection: upgrade"
test_protocol "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n" "HTTP/1.0 (close by default)"

# Test 3: Transfer-Encoding
echo ""
echo "Test 3: Transfer-Encoding"
test_protocol "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n0\r\n\r\n" "Chunked encoding"
test_protocol "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: gzip\r\n\r\n" "Unsupported encoding"
test_protocol "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n" "Empty chunked body"

# Test 4: Content-Length edge cases
echo ""
echo "Test 4: Content-Length handling"
test_protocol "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello" "Correct Content-Length"
test_protocol "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello world" "Body longer than CL"
test_protocol "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n" "Zero Content-Length"

# Test 5: Host header variations
echo ""
echo "Test 5: Host header variations"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" "Standard Host"
test_protocol "GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n" "Host with port"
test_protocol "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n" "Host as IP"
test_protocol "GET / HTTP/1.1\r\nHost: 127.0.0.1:8080\r\n\r\n" "IP with port"
test_protocol "GET / HTTP/1.1\r\nHost: \r\n\r\n" "Empty Host value"

# Test 6: Expect header
echo ""
echo "Test 6: Expect header handling"
test_protocol "POST / HTTP/1.1\r\nHost: localhost\r\nExpect: 100-continue\r\nContent-Length: 5\r\n\r\nhello" "Expect: 100-continue" "200"
test_protocol "POST / HTTP/1.1\r\nHost: localhost\r\nExpect: something-else\r\nContent-Length: 5\r\n\r\nhello" "Invalid Expect value" "417"

# Test 7: Accept headers
echo ""
echo "Test 7: Content negotiation headers"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nAccept: */*\r\n\r\n" "Accept: */*"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nAccept: text/html\r\n\r\n" "Accept: text/html"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nAccept: application/json\r\n\r\n" "Accept: application/json"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nAccept-Encoding: gzip\r\n\r\n" "Accept-Encoding"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nAccept-Language: en-US\r\n\r\n" "Accept-Language"

# Test 8: Authorization (basic patterns)
echo ""
echo "Test 8: Authorization header"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nAuthorization: Basic dXNlcjpwYXNz\r\n\r\n" "Basic auth header"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nAuthorization: Bearer token123\r\n\r\n" "Bearer token"

# Test 9: Range requests
echo ""
echo "Test 9: Range requests"
test_protocol "GET /index.html HTTP/1.1\r\nHost: localhost\r\nRange: bytes=0-99\r\n\r\n" "Range: bytes=0-99"
test_protocol "GET /index.html HTTP/1.1\r\nHost: localhost\r\nRange: bytes=0-\r\n\r\n" "Range: bytes=0- (open)"
test_protocol "GET /index.html HTTP/1.1\r\nHost: localhost\r\nRange: bytes=-100\r\n\r\n" "Range: last 100 bytes"

# Test 10: Cache-Control
echo ""
echo "Test 10: Cache-Control headers"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nCache-Control: no-cache\r\n\r\n" "Cache-Control: no-cache"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nCache-Control: max-age=3600\r\n\r\n" "Cache-Control: max-age"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nIf-None-Match: \"etag123\"\r\n\r\n" "If-None-Match"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nIf-Modified-Since: Wed, 21 Oct 2015 07:28:00 GMT\r\n\r\n" "If-Modified-Since"

# Test 11: Redirect handling
echo ""
echo "Test 11: Redirection scenarios"
test_protocol "GET /redirect HTTP/1.1\r\nHost: localhost\r\n\r\n" "Request to redirect path"
test_protocol "GET /moved HTTP/1.1\r\nHost: localhost\r\n\r\n" "Moved resource"

# Test 12: Method completeness
echo ""
echo "Test 12: All standard methods"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n" "GET method"
test_protocol "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n" "POST method"
test_protocol "DELETE /test HTTP/1.1\r\nHost: localhost\r\n\r\n" "DELETE method"
test_protocol "HEAD / HTTP/1.1\r\nHost: localhost\r\n\r\n" "HEAD method"
test_protocol "PUT /test HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n" "PUT method"
test_protocol "OPTIONS / HTTP/1.1\r\nHost: localhost\r\n\r\n" "OPTIONS method"

# Test 13: Special URI characters
echo ""
echo "Test 13: URI encoding and special characters"
test_protocol "GET /%20 HTTP/1.1\r\nHost: localhost\r\n\r\n" "Encoded space"
test_protocol "GET /?query=value HTTP/1.1\r\nHost: localhost\r\n\r\n" "Query string"
test_protocol "GET /?key=value&key2=value2 HTTP/1.1\r\nHost: localhost\r\n\r\n" "Multiple params"
test_protocol "GET /#fragment HTTP/1.1\r\nHost: localhost\r\n\r\n" "Fragment identifier"
test_protocol "GET /%2F%2F HTTP/1.1\r\nHost: localhost\r\n\r\n" "Encoded slashes"

# Test 14: Cookie handling
echo ""
echo "Test 14: Cookie headers"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nCookie: session=abc123\r\n\r\n" "Single cookie"
test_protocol "GET / HTTP/1.1\r\nHost: localhost\r\nCookie: session=abc123; user=john\r\n\r\n" "Multiple cookies"

# Test 15: Multipart form data
echo ""
echo "Test 15: Multipart content type"
test_protocol "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Type: multipart/form-data; boundary=----Boundary\r\nContent-Length: 10\r\n\r\n------test" "Multipart form data"

# Final check
echo ""
echo "=========================================="
if nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
    echo -e "${GREEN}✓${NC} Server survived all protocol edge case tests!"
    echo "=========================================="
    exit 0
else
    echo -e "${RED}✗${NC} Server crashed during testing"
    echo "=========================================="
    exit 1
fi
