#!/bin/bash

# CGI Test Suite - Run this to test all CGI scripts independently
# Usage: bash tests/test_cgi.sh

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}=== Building CGI Tester ===${NC}"
c++ -std=c++17 -Wall -Wextra -Werror -I includes tests/cgi_tester.cpp srcs/cgi/CGI.cpp -o cgi_test

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to compile CGI tester${NC}"
    exit 1
fi

echo -e "${GREEN}CGI Tester built successfully${NC}\n"

# Advanced Tests
echo -e "\n${YELLOW}=== Advanced CGI Tests ===${NC}\n"

# Test 9: Advanced test - comprehensive environment variables
if [ -f cgi-bin/advanced_test.py ]; then
    chmod +x cgi-bin/advanced_test.py
    echo -e "${YELLOW}Test 9: Advanced CGI - comprehensive features${NC}"
    ./cgi_test cgi-bin/advanced_test.py "format=json&user=alice&id=42"
fi

# Test 10: Error test - stderr output
if [ -f cgi-bin/error_test.py ]; then
    chmod +x cgi-bin/error_test.py
    echo -e "\n${YELLOW}Test 10: Error handling - stderr output${NC}"
    ./cgi_test cgi-bin/error_test.py "stderr"
fi

# Test 11: Error test - script crash
if [ -f cgi-bin/error_test.py ]; then
    echo -e "\n${YELLOW}Test 11: Error handling - script crash${NC}"
    echo -e "${YELLOW}Expected: Non-zero exit code${NC}"
    ./cgi_test cgi-bin/error_test.py "crash"
fi

# Test 12: Error test - missing CGI header
if [ -f cgi-bin/error_test.py ]; then
    echo -e "\n${YELLOW}Test 12: Error handling - missing CGI header${NC}"
    ./cgi_test cgi-bin/error_test.py "no_header"
fi

# Test 13: Stress test - small output (100 lines)
if [ -f cgi-bin/stress.py ]; then
    chmod +x cgi-bin/stress.py
    echo -e "\n${YELLOW}Test 13: Stress test - 100 lines of output${NC}"
    ./cgi_test cgi-bin/stress.py "100"
fi

# Test 14: Stress test - large output (1000 lines)
if [ -f cgi-bin/stress.py ]; then
    echo -e "\n${YELLOW}Test 14: Stress test - 1000 lines of output${NC}"
    ./cgi_test cgi-bin/stress.py "1000"
fi

# Test 15: Upload handler - POST data
if [ -f cgi-bin/upload.py ]; then
    chmod +x cgi-bin/upload.py
    echo -e "\n${YELLOW}Test 15: Upload handler - POST data${NC}"
    ./cgi_test cgi-bin/upload.py "" "filename=test.txt&data=Hello+World"
fi

# Cleanup
rm -f cgi_test

echo -e "\n${GREEN}=== All CGI tests completed ===${NC}"
