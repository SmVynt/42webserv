#!/bin/bash

# Static File Serving Tests
# Tests server's ability to serve static content correctly

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
SERVER_BIN="${SERVER_BIN:-$PROJECT_ROOT/webserv}"
CONFIG_FILE="${CONFIG_FILE:-$PROJECT_ROOT/config/default.conf}"
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

    # Wait for HTTP to be responsive (socket open does not guarantee readiness).
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

# Test function
test_static_file() {
    local description="$1"
    local path="$2"
    local expected_status="${3:-200}"
    local check_content_type="${4:-}"

    echo -n "  Testing $description... "

    response=$(curl $CURL_OPTS -w "\n%{http_code}\n%{content_type}" "$SERVER_URL$path" 2>/dev/null || true)

    # Extract status code and content type
    status=$(echo "$response" | tail -n 2 | head -n 1)
    content_type=$(echo "$response" | tail -n 1)
    if [ -z "$status" ]; then
        status="000"
    fi

    # Check status code
    if [ "$status" = "$expected_status" ]; then
        # Check content type if specified
        if [ -n "$check_content_type" ]; then
            if echo "$content_type" | grep -q "$check_content_type"; then
                echo -e "${GREEN}✓${NC} (Status: $status, Type: $content_type)"
                PASSED=$((PASSED + 1))
                return 0
            else
                echo -e "${RED}✗${NC} (Status: $status, Wrong type: $content_type, expected: $check_content_type)"
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

# Test file content
test_file_content() {
    local description="$1"
    local path="$2"
    local expected_content="$3"

    echo -n "  Testing $description... "

    content=$(curl $CURL_OPTS "$SERVER_URL$path" 2>/dev/null || true)

    if echo "$content" | grep -q "$expected_content"; then
        echo -e "${GREEN}✓${NC}"
        PASSED=$((PASSED + 1))
        return 0
    else
        echo -e "${RED}✗${NC} (Content mismatch)"
        FAILED=$((FAILED + 1))
        return 0
    fi
}

# Test HEAD request
test_head_request() {
    local description="$1"
    local path="$2"

    echo -n "  Testing $description... "

    # HEAD should return headers but no body
    response=$(curl $CURL_OPTS -I "$SERVER_URL$path" 2>/dev/null || true)
    status=$(echo "$response" | head -n 1 | grep -oP 'HTTP/\d\.\d \K\d+' || echo "000")

    # Check if Content-Length is present
    content_length=$(echo "$response" | grep -i "Content-Length:" | awk '{print $2}' | tr -d '\r' || true)

    if [ "$status" = "200" ] && [ -n "$content_length" ]; then
        echo -e "${GREEN}✓${NC} (Status: $status, Content-Length: $content_length)"
        PASSED=$((PASSED + 1))
        return 0
    else
        echo -e "${RED}✗${NC} (Status: $status)"
        FAILED=$((FAILED + 1))
        return 0
    fi
}

echo "=========================================="
echo "Static File Serving Tests"
echo "=========================================="

start_server

# Test 1: Basic file serving
echo ""
echo "Test 1: Basic HTML files"
test_static_file "Homepage" "/" "200" "text/html"
test_static_file "Index.html" "/index.html" "200" "text/html"
test_static_file "404 error page" "/error/404.html" "200" "text/html"
test_static_file "500 error page" "/error/500.html" "200" "text/html"

# Test 2: MIME type detection
echo ""
echo "Test 2: MIME Type Detection"
test_static_file "HTML file" "/index.html" "200" "text/html"

# Create test files if they don't exist
TEST_DIR="$PROJECT_ROOT/www/test_files"
mkdir -p "$TEST_DIR"

# Create CSS file
if [ ! -f "$TEST_DIR/style.css" ]; then
    echo "body { margin: 0; }" > "$TEST_DIR/style.css"
fi
test_static_file "CSS file" "/test_files/style.css" "200" "text/css"

# Create JS file
if [ ! -f "$TEST_DIR/script.js" ]; then
    echo "console.log('test');" > "$TEST_DIR/script.js"
fi
test_static_file "JavaScript file" "/test_files/script.js" "200" "javascript"

# Create JSON file
if [ ! -f "$TEST_DIR/data.json" ]; then
    echo '{"test": "data"}' > "$TEST_DIR/data.json"
fi
test_static_file "JSON file" "/test_files/data.json" "200" "application/json"

# Create plain text file
if [ ! -f "$TEST_DIR/readme.txt" ]; then
    echo "Test readme file" > "$TEST_DIR/readme.txt"
fi
test_static_file "Text file" "/test_files/readme.txt" "200" "text/plain"

# Test 3: Non-existent files
echo ""
echo "Test 3: Non-existent files (404)"
test_static_file "Non-existent file" "/nonexistent.html" "404"
test_static_file "Non-existent in subdir" "/error/nonexistent.html" "404"
test_static_file "Completely invalid path" "/this/path/does/not/exist/file.html" "404"

# Test 4: Directory listing
echo ""
echo "Test 4: Directory access"
test_static_file "Uploads directory" "/uploads/" "200"
test_static_file "Root directory" "/" "200"

# Test 5: HEAD requests
echo ""
echo "Test 5: HEAD Requests"
test_head_request "HEAD for homepage" "/"
test_head_request "HEAD for index.html" "/index.html"

# Test 6: File content verification
echo ""
echo "Test 6: Content Verification"
test_file_content "Homepage contains HTML" "/" "<html"
test_file_content "404 page has error message" "/error/404.html" "404"

# Test 7: Case sensitivity
echo ""
echo "Test 7: Path Case Sensitivity"
test_static_file "Lowercase path" "/index.html" "200"
test_static_file "Uppercase extension (should fail)" "/index.HTML" "404"

# Test 8: Special characters in paths
echo ""
echo "Test 8: Special Characters"
test_static_file "Path with %20 (space)" "/test%20file" "404"
test_static_file "Path with encoded slash" "/test%2Ffile" "200"

# Test 9: Range requests (if supported)
echo ""
echo "Test 9: Range Requests"
echo -n "  Testing partial content request... "
response=$(curl $CURL_OPTS -H "Range: bytes=0-10" -w "\n%{http_code}" "$SERVER_URL/index.html" 2>/dev/null || true)
status=$(echo "$response" | tail -n 1)

if [ "$status" = "206" ]; then
    echo -e "${GREEN}✓${NC} (Partial content supported: 206)"
    PASSED=$((PASSED + 1))
elif [ "$status" = "200" ]; then
    echo -e "${YELLOW}⚠${NC} (Full content returned: 200 - ranges not supported)"
    PASSED=$((PASSED + 1))
else
    echo -e "${RED}✗${NC} (Unexpected status: $status)"
    FAILED=$((FAILED + 1))
fi

# Test 10: Large file handling
echo ""
echo "Test 10: Large File Handling"

# Create a 1MB test file
if [ ! -f "$TEST_DIR/large.bin" ]; then
    dd if=/dev/urandom of="$TEST_DIR/large.bin" bs=1M count=1 2>/dev/null
fi

echo -n "  Testing 1MB file download... "
start_time=$(date +%s.%N)
response=$(curl $CURL_OPTS -w "%{http_code}" -o /dev/null "$SERVER_URL/test_files/large.bin" 2>/dev/null || true)
end_time=$(date +%s.%N)
duration=$(echo "$end_time - $start_time" | bc)

if [ "$response" = "200" ]; then
    echo -e "${GREEN}✓${NC} (Downloaded in ${duration}s)"
    PASSED=$((PASSED + 1))
else
    echo -e "${RED}✗${NC} (Status: $response)"
    FAILED=$((FAILED + 1))
fi

# Test 11: Multiple simultaneous requests
echo ""
echo "Test 11: Concurrent Static File Requests"

echo -n "  Testing 10 concurrent requests... "
temp_results=$(mktemp)
pids=()

for i in {1..10}; do
    (curl $CURL_OPTS -w "%{http_code}\n" -o /dev/null "$SERVER_URL/index.html" 2>/dev/null >> "$temp_results" || true) &
    pids+=($!)
done

timeout_sec=10
end_time=$((SECONDS + timeout_sec))
while :; do
    alive=0
    for pid in "${pids[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            alive=1
            break
        fi
    done
    if [ $alive -eq 0 ]; then
        break
    fi
    if [ $SECONDS -ge $end_time ]; then
        echo -e "${YELLOW}⚠${NC} (Timed out waiting for concurrent requests)"
        for pid in "${pids[@]}"; do
            kill "$pid" 2>/dev/null || true
        done
        wait 2>/dev/null || true
        break
    fi
    sleep 0.1
done

# Count successful requests
success_count=$(grep -c "200" "$temp_results" || echo "0")
rm "$temp_results"

if [ "$success_count" -eq 10 ]; then
    echo -e "${GREEN}✓${NC} (All 10 successful)"
    PASSED=$((PASSED + 1))
else
    echo -e "${YELLOW}⚠${NC} (Only $success_count/10 successful)"
    FAILED=$((FAILED + 1))
fi

# Test 12: Connection persistence
echo ""
echo "Test 12: Connection Persistence"

echo -n "  Testing keep-alive... "
response=$(curl $CURL_OPTS -H "Connection: keep-alive" -w "\n%{http_code}" "$SERVER_URL/" 2>/dev/null || true)
status=$(echo "$response" | tail -n 1)

if [ "$status" = "200" ]; then
    echo -e "${GREEN}✓${NC} (Keep-alive handled)"
    PASSED=$((PASSED + 1))
else
    echo -e "${RED}✗${NC} (Status: $status)"
    FAILED=$((FAILED + 1))
fi

# Test 13: Empty file
echo ""
echo "Test 13: Edge Cases"

if [ ! -f "$TEST_DIR/empty.txt" ]; then
    touch "$TEST_DIR/empty.txt"
fi
test_static_file "Empty file" "/test_files/empty.txt" "200"

# Test 14: File permissions (if applicable)
echo ""
echo "Test 14: Server Behavior"

echo -n "  Testing server stability after many requests... "
for i in {1..100}; do
    curl $CURL_OPTS "$SERVER_URL/" > /dev/null 2>&1 || true
done

if nc -z "$SERVER_HOST" "$SERVER_PORT" 2>/dev/null; then
    echo -e "${GREEN}✓${NC} (Server stable)"
    PASSED=$((PASSED + 1))
else
    echo -e "${RED}✗${NC} (Server crashed)"
    FAILED=$((FAILED + 1))
fi

# Cleanup test files
rm -rf "$TEST_DIR"

# Summary
echo ""
echo "=========================================="
echo "Summary"
echo "=========================================="
TOTAL=$((PASSED + FAILED))
echo "Total tests: $TOTAL"
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${RED}Failed: $FAILED${NC}"

if [ $TOTAL -gt 0 ]; then
    PASS_RATE=$((PASSED * 100 / TOTAL))
    echo "Pass rate: ${PASS_RATE}%"
fi

echo "=========================================="

# Exit with appropriate code
if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All static file serving tests passed!${NC}"
    exit 0
elif [ $PASSED -gt $((TOTAL * 7 / 10)) ]; then
    echo -e "${YELLOW}Most tests passed (${PASS_RATE}%)${NC}"
    exit 0
else
    echo -e "${RED}Many tests failed${NC}"
    exit 1
fi
