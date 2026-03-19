#!/bin/bash

# CGI Script Tests
# Tests server's ability to execute and return CGI responses correctly

set -uo pipefail

# Configuration (absolute paths)
# PROJECT_ROOT="/root/projects/webserv"
# SERVER_BIN="$PROJECT_ROOT/webserv"
# CONFIG_FILE="$PROJECT_ROOT/config/default.conf"
# SERVER_HOST="${SERVER_HOST:-127.0.0.1}"
# SERVER_PORT="${SERVER_PORT:-8080}"
# SERVER_URL="http://${SERVER_HOST}:${SERVER_PORT}"
# CURL_OPTS="-4 -sS --connect-timeout 2 --max-time 5"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
SERVER_BIN="${SERVER_BIN:-$PROJECT_ROOT/webserv}"
CONFIG_FILE="${CONFIG_FILE:-$PROJECT_ROOT/config/default.conf}"
SERVER_HOST="${SERVER_HOST:-127.0.0.1}"
SERVER_PORT="${SERVER_PORT:-8080}"
SERVER_URL="http://${SERVER_HOST}:${SERVER_PORT}"
CURL_OPTS="-4 -sS --connect-timeout 2 --max-time 5"

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
        local start_dir="$PWD"
        cd "$PROJECT_ROOT"
        "$SERVER_BIN" "$CONFIG_FILE" &>server.log &
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
    local post_data="${6:-}"

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
				echo -e "Expected content: $expected_content"
				echo -e "Received content: $body"
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

test_multiple_workers() {
	local description="$1"
	local path="$2"
	local post_size="${3:-1000000}"  # Default 1 MB
	local workers="${4:-10}"
	local repeats="${5:-3}"

	echo -n "  Testing $description with $workers workers..."
	echo ""

    local tmpdir=$(mktemp -d)
    local pids=()
    for i in $(seq 1 $workers); do
        (
            echo "    Worker $i: Sending POST request with $post_size bytes..."
            dd if=/dev/urandom bs=1 count="$post_size" 2>/dev/null | \
            curl $CURL_OPTS -X POST --data-binary @- -w "\n%{http_code} %{size_download}" "$SERVER_URL$path" > "$tmpdir/worker_$i.out" 2>/dev/null
        ) &
        pids+=("$!")
		sleep 0.05  # Stagger worker start times slightly
    done

    wait "${pids[@]}"

    # Print return codes and sizes
    for i in $(seq 1 $workers); do
        code=$(tail -n 1 "$tmpdir/worker_$i.out" | awk '{print $1}')
        sz=$(tail -n 1 "$tmpdir/worker_$i.out" | awk '{print $2}')
        echo "    Worker $i: HTTP $code, Size $sz"
    done

    # Check results
    local all_same=1
    local ref_size=$(tail -n 1 "$tmpdir/worker_1.out" | awk '{print $2}')
    for i in $(seq 2 $workers); do
        sz=$(tail -n 1 "$tmpdir/worker_$i.out" | awk '{print $2}')
        if [ "$sz" != "$ref_size" ]; then
            all_same=0
            break
        fi
    done

    rm -rf "$tmpdir"

    if [ $all_same -eq 1 ]; then
        echo -e "${GREEN}✓${NC} (All responses same size: $ref_size)"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}✗${NC} (Response sizes differ)"
        FAILED=$((FAILED + 1))
    fi
}

echo "=========================================="
echo "CGI Script Tests"
echo "=========================================="

start_server

echo ""
echo "Test 1: Basic CGI GET"
test_cgi "CGI Hello (GET)" "/cgi-bin/python/hello.py" "200" "Hello, CGI"

# echo ""
# echo "Test 2: CGI POST"
# test_cgi "CGI Echo (POST)" "/cgi-bin/python/echo.py" "200" "You posted: testdata" "POST" "testdata"

# echo ""
# echo "Test 3: CGI Error Handling"
# test_cgi "CGI Not Found" "/cgi-bin/python/nonexistent.py" "404" ""
# test_cgi "CGI Script Error" "/cgi-bin/python/error.py" "500" ""

# echo ""
# echo "Test 4: Edge Cases"
# test_cgi "CGI with Query String" "/cgi-bin/python/hello.py?name=Webserv" "200" "Webserv"

# echo ""

# echo ""
# echo "Test 5: Multiple Workers"
# test_multiple_workers "CGI Echo with 10 Workers" "/cgi-bin/python/echo.py"
# test_multiple_workers "CGI Echo with 20 Workers (Small POST)" "/cgi-bin/python/echo.py" 10000 20
test_multiple_workers "CGI Echo with 20 Workers (Large POST)" "/cgi-bin/python/echo.py" 5000000 20
# test_multiple_workers "CGI Echo with 20 Workers (Very Large POST)" "/cgi-bin/python/echo.py" 10000000 20
# test_multiple_workers "CGI Youpi" "/directory/youpi.bla" 10000000 20

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
