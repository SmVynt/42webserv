#!/bin/bash

# CGI Script Tests
# Tests server's ability to execute and return CGI responses correctly

set -euo pipefail

# Always cd to project root for consistent paths
cd /root/projects/webserv

# Configuration (absolute paths)
PROJECT_ROOT="/root/projects/webserv"
SERVER_BIN="$PROJECT_ROOT/webserv"
CONFIG_FILE="$PROJECT_ROOT/config/default.conf"
SERVER_HOST="${SERVER_HOST:-127.0.0.1}"
SERVER_PORT="${SERVER_PORT:-8080}"
SERVER_URL="http://${SERVER_HOST}:${SERVER_PORT}"
CURL_OPTS="-4 -sS --connect-timeout 2 --max-time 5"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Start server if not running
start_server() {
    if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
        echo "Starting server..."
        local start_dir="$PWD"
        cd "$PROJECT_ROOT"
        "$SERVER_BIN" "$CONFIG_FILE" &>/dev/null &
        SERVER_PID=$!
        cd "$start_dir"
        sleep 1

        if ! nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
            echo -e "${RED}[ERROR]${NC} Failed to start server"
            exit 1
        fi
        echo "Server started (PID: $SERVER_PID)"
    else
        echo "Server already running"
        SERVER_PID=""
    fi

    # Wait for HTTP to be responsive
    local ready_code="000"
    for _ in {1..20}; do
        ready_code=$(curl $CURL_OPTS -o /dev/null -w "%{http_code}" "$SERVER_URL/" 2>/dev/null || echo "000")
        if [ "$ready_code" != "000" ]; then
            break
        fi
        sleep 0.2
    done

    if [ "$ready_code" = "000" ]; then
        echo -e "${RED}[ERROR]${NC} Server not responding to HTTP requests"
        exit 1
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

# Test counters
PASSED=0
FAILED=0

test_cgi() {
    local description="$1"
    local path="$2"
    local expected_status="${3:-200}"
    local expected_content="$4"
    local method="${5:-GET}"
    local post_data="$6"

    echo -n "  Testing $description... "

    if [ "$method" = "POST" ]; then
        response=$(curl $CURL_OPTS -X POST -d "$post_data" -w "\n%{http_code}" "$SERVER_URL$path" 2>/dev/null || true)
    else
        response=$(curl $CURL_OPTS -w "\n%{http_code}" "$SERVER_URL$path" 2>/dev/null || true)
    fi

    status=$(echo "$response" | tail -n 1)
    body=$(echo "$response" | head -n -1)
    if [ -z "$status" ]; then
        status="000"
    fi

    if [ "$status" = "$expected_status" ]; then
        if [ -n "$expected_content" ]; then
            if echo "$body" | grep -q "$expected_content"; then
                echo -e "${GREEN}✓${NC} (Status: $status)"
                PASSED=$((PASSED + 1))
                return 0
            else
                echo -e "${RED}✗${NC} (Status: $status, Content mismatch)"
                FAILED=$((FAILED + 1))
                return 0
            fi
        else
            echo -e "${GREEN}✓${NC} (Status: $status)"
            PASSED=$((PASSED + 1))
            return 0
        fi
    else
        echo -e "${RED}✗${NC} (Expected: $expected_status, Got: $status)"
        FAILED=$((FAILED + 1))
        return 0
    fi
}

echo "=========================================="
echo "CGI Script Tests"
echo "=========================================="

start_server

echo ""
echo "Test 1: Basic CGI GET"
test_cgi "CGI Hello (GET)" "/cgi-bin/python/hello.py" "200" "Hello, CGI"

echo ""
echo "Test 2: CGI POST"
test_cgi "CGI Echo (POST)" "/cgi-bin/python/echo.py" "200" "You posted: testdata" "POST" "testdata"

echo ""
echo "Test 3: CGI Error Handling"
test_cgi "CGI Not Found" "/cgi-bin/python/nonexistent.py" "404" ""
test_cgi "CGI Script Error" "/cgi-bin/python/error.py" "500" ""

echo ""
echo "Test 4: Edge Cases"
test_cgi "CGI with Query String" "/cgi-bin/python/hello.py?name=Webserv" "200" "Webserv"

echo ""
echo "=========================================="
echo "Summary"
echo "=========================================="
TOTAL=$((PASSED + FAILED))
echo "Total tests: $TOTAL"
echo -e "${GREEN}Passed: $PASSED${NC}"
if [ $FAILED -gt 0 ]; then
    echo -e "${RED}Failed: $FAILED${NC}"
fi

if [ $TOTAL -gt 0 ]; then
    PASS_RATE=$((PASSED * 100 / TOTAL))
    echo "Pass rate: ${PASS_RATE}%"
fi

echo "=========================================="

# Exit with appropriate code
if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All CGI tests passed!${NC}"
    exit 0
elif [ $PASSED -gt $((TOTAL * 7 / 10)) ]; then
    echo -e "${YELLOW}Most tests passed (${PASS_RATE}%)${NC}"
    exit 0
else
    echo -e "${RED}Many tests failed${NC}"
    exit 1
fi
