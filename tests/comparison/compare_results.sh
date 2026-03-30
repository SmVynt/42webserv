#!/bin/bash

# NGINX vs WebServ Comparison Script
# Compares behavior and performance between the two servers

set -euo pipefail

# Configuration
WEBSERV_PORT="${WEBSERV_PORT:-8080}"
NGINX_PORT="${NGINX_PORT:-8081}"
WEBSERV_BIN="../webserv"
WEBSERV_CONFIG="../config/default.conf"
NGINX_CONF="/tmp/nginx_test/nginx.conf"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Results directory
RESULTS_DIR="/tmp/comparison_results"
mkdir -p "$RESULTS_DIR"

# Server PIDs
WEBSERV_PID=""
NGINX_PID=""

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."

    if [ -n "$WEBSERV_PID" ]; then
        kill $WEBSERV_PID 2>/dev/null || true

        # Wait with timeout
        local timeout=3
        while kill -0 $WEBSERV_PID 2>/dev/null && [ $timeout -gt 0 ]; do
            sleep 0.1
            timeout=$((timeout - 1))
        done

        # Force kill if still running
        if kill -0 $WEBSERV_PID 2>/dev/null; then
            kill -9 $WEBSERV_PID 2>/dev/null || true
            sleep 0.2
        fi

        # Only wait if process still exists
        if kill -0 $WEBSERV_PID 2>/dev/null; then
            wait $WEBSERV_PID 2>/dev/null || true
        fi
    fi

    if [ -n "$NGINX_PID" ]; then
        nginx -s stop -c "$NGINX_CONF" 2>/dev/null || true
    fi

    # Kill any remaining processes
    pkill -f "nginx.*$NGINX_CONF" 2>/dev/null || true
}

trap cleanup EXIT

echo "=========================================="
echo "NGINX vs WebServ Comparison"
echo "=========================================="
echo ""

# Check if NGINX config exists
if [ ! -f "$NGINX_CONF" ]; then
    echo -e "${YELLOW}[INFO]${NC} NGINX configuration not found. Running setup..."
    bash nginx_setup.sh
fi

# Check dependencies
if ! command -v nginx &>/dev/null; then
    echo -e "${YELLOW}[WARNING]${NC} nginx not installed - skipping comparison"
    exit 0
fi

# Start both servers
echo "Starting servers..."

# Start WebServ
if ! nc -z localhost $WEBSERV_PORT 2>/dev/null; then
    "$WEBSERV_BIN" "$WEBSERV_CONFIG" &>/dev/null &
    WEBSERV_PID=$!
    sleep 2

    if ! nc -z localhost $WEBSERV_PORT 2>/dev/null; then
        echo -e "${RED}[ERROR]${NC} Failed to start webserv"
        exit 1
    fi
    echo -e "${GREEN}✓${NC} WebServ started on port $WEBSERV_PORT"
else
    echo -e "${YELLOW}⚠${NC} WebServ already running on port $WEBSERV_PORT"
fi

# Start NGINX
if ! nc -z localhost $NGINX_PORT 2>/dev/null; then
    nginx -c "$NGINX_CONF" &
    NGINX_PID=$!
    sleep 2

    if ! nc -z localhost $NGINX_PORT 2>/dev/null; then
        echo -e "${RED}[ERROR]${NC} Failed to start nginx"
        exit 1
    fi
    echo -e "${GREEN}✓${NC} NGINX started on port $NGINX_PORT"
else
    echo -e "${YELLOW}⚠${NC} NGINX already running on port $NGINX_PORT"
fi

echo ""

# Comparison tests
declare -a DIFFERENCES=()
declare -a SIMILARITIES=()
TOTAL_TESTS=0
PASSED_TESTS=0

# Test function
compare_request() {
    local test_name="$1"
    local path="$2"
    local method="${3:-GET}"
    local data="${4:-}"

    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo -n "Testing $test_name... "

    # Request WebServ
    if [ -n "$data" ]; then
        webserv_response=$(curl -s -i -X "$method" -d "$data" "http://localhost:$WEBSERV_PORT$path" 2>/dev/null)
    else
        webserv_response=$(curl -s -i -X "$method" "http://localhost:$WEBSERV_PORT$path" 2>/dev/null)
    fi

    # Request NGINX
    if [ -n "$data" ]; then
        nginx_response=$(curl -s -i -X "$method" -d "$data" "http://localhost:$NGINX_PORT$path" 2>/dev/null)
    else
        nginx_response=$(curl -s -i -X "$method" "http://localhost:$NGINX_PORT$path" 2>/dev/null)
    fi

    # Extract status codes
    webserv_status=$(echo "$webserv_response" | head -n 1 | grep -oP 'HTTP/\d\.\d \K\d+' || echo "000")
    nginx_status=$(echo "$nginx_response" | head -n 1 | grep -oP 'HTTP/\d\.\d \K\d+' || echo "000")

    # Save responses
    echo "$webserv_response" > "$RESULTS_DIR/${test_name// /_}_webserv.txt"
    echo "$nginx_response" > "$RESULTS_DIR/${test_name// /_}_nginx.txt"

    # Compare
    if [ "$webserv_status" = "$nginx_status" ]; then
        echo -e "${GREEN}✓${NC} (Both: $webserv_status)"
        SIMILARITIES+=("$test_name: Both returned $webserv_status")
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo -e "${YELLOW}≠${NC} (WebServ: $webserv_status, NGINX: $nginx_status)"
        DIFFERENCES+=("$test_name: WebServ=$webserv_status, NGINX=$nginx_status")
    fi
}

# Run comparison tests
echo "${BLUE}Behavior Comparison${NC}"
echo "-------------------"

compare_request "Homepage" "/"
compare_request "Existing file" "/index.html"
compare_request "Non-existent file" "/nonexistent.html"
compare_request "Directory" "/uploads/"
compare_request "Error page" "/error/404.html"
compare_request "POST request" "/" "POST" "test=data"
compare_request "Empty path" ""
compare_request "Trailing slash" "/uploads"
compare_request "Query string" "/?param=value"

echo ""
echo "${BLUE}Header Comparison${NC}"
echo "-----------------"

# Compare specific headers
echo "Checking response headers..."

webserv_headers=$(curl -s -I "http://localhost:$WEBSERV_PORT/" 2>/dev/null)
nginx_headers=$(curl -s -I "http://localhost:$NGINX_PORT/" 2>/dev/null)

echo "$webserv_headers" > "$RESULTS_DIR/headers_webserv.txt"
echo "$nginx_headers" > "$RESULTS_DIR/headers_nginx.txt"

# Check for common headers
for header in "Content-Type" "Content-Length" "Connection" "Server"; do
    webserv_val=$(echo "$webserv_headers" | grep -i "^$header:" || echo "Not present")
    nginx_val=$(echo "$nginx_headers" | grep -i "^$header:" || echo "Not present")

    echo "  $header:"
    echo "    WebServ: $webserv_val"
    echo "    NGINX:   $nginx_val"
done

echo ""
echo "${BLUE}Performance Comparison${NC}"
echo "---------------------"

# Simple performance test with curl
echo "Running quick performance test (100 requests each)..."

# WebServ timing
webserv_start=$(date +%s.%N)
for i in {1..100}; do
    curl -s -o /dev/null "http://localhost:$WEBSERV_PORT/" 2>/dev/null
done
webserv_end=$(date +%s.%N)
webserv_time=$(echo "$webserv_end - $webserv_start" | bc)
webserv_rps=$(echo "100 / $webserv_time" | bc)

# NGINX timing
nginx_start=$(date +%s.%N)
for i in {1..100}; do
    curl -s -o /dev/null "http://localhost:$NGINX_PORT/" 2>/dev/null
done
nginx_end=$(date +%s.%N)
nginx_time=$(echo "$nginx_end - $nginx_start" | bc)
nginx_rps=$(echo "100 / $nginx_time" | bc)

echo "WebServ: ${webserv_time}s (${webserv_rps} req/s)"
echo "NGINX:   ${nginx_time}s (${nginx_rps} req/s)"

# Calculate ratio
if (( $(echo "$nginx_time > 0" | bc -l) )); then
    ratio=$(echo "scale=2; $webserv_time / $nginx_time" | bc)
    echo "Ratio: ${ratio}x (webserv/nginx)"
fi

# Advanced performance comparison with ab (if available)
if command -v ab &>/dev/null; then
    echo ""
    echo "Running Apache Bench comparison (1000 requests, 10 concurrent)..."

    echo -n "  WebServ... "
    ab -n 1000 -c 10 -q "http://localhost:$WEBSERV_PORT/" > "$RESULTS_DIR/ab_webserv.txt" 2>&1
    webserv_ab_rps=$(grep "Requests per second:" "$RESULTS_DIR/ab_webserv.txt" | awk '{print $4}')
    echo "${webserv_ab_rps} req/s"

    echo -n "  NGINX... "
    ab -n 1000 -c 10 -q "http://localhost:$NGINX_PORT/" > "$RESULTS_DIR/ab_nginx.txt" 2>&1
    nginx_ab_rps=$(grep "Requests per second:" "$RESULTS_DIR/ab_nginx.txt" | awk '{print $4}')
    echo "${nginx_ab_rps} req/s"
fi

# Summary
echo ""
echo "=========================================="
echo "Summary"
echo "=========================================="
echo ""
echo "Behavior Tests: $PASSED_TESTS/$TOTAL_TESTS passed"
echo ""

if [ ${#SIMILARITIES[@]} -gt 0 ]; then
    echo "${GREEN}Similarities (${#SIMILARITIES[@]}):${NC}"
    for sim in "${SIMILARITIES[@]}"; do
        echo "  ✓ $sim"
    done
    echo ""
fi

if [ ${#DIFFERENCES[@]} -gt 0 ]; then
    echo "${YELLOW}Differences (${#DIFFERENCES[@]}):${NC}"
    for diff in "${DIFFERENCES[@]}"; do
        echo "  ≠ $diff"
    done
    echo ""
fi

echo "Detailed results saved to: $RESULTS_DIR"
echo ""
echo "Files:"
ls -lh "$RESULTS_DIR" | tail -n +2 | awk '{print "  " $9 " (" $5 ")"}'

echo ""
echo "=========================================="
echo "Comparison complete!"
echo "=========================================="

# Exit with success if most tests passed
if [ $PASSED_TESTS -ge $((TOTAL_TESTS * 7 / 10)) ]; then
    exit 0
else
    exit 1
fi
