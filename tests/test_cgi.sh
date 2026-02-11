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

# Test 1: Simple Python CGI (GET)
echo -e "${YELLOW}Test 1: Python CGI - GET request${NC}"
./cgi_test cgi-bin/test.py

# Test 2: Python CGI with Query String
echo -e "\n${YELLOW}Test 2: Python CGI - GET with query string${NC}"
./cgi_test cgi-bin/test.py "name=Alice&age=25"

# Test 3: Python CGI with POST data
echo -e "\n${YELLOW}Test 3: Python CGI - POST request${NC}"
./cgi_test cgi-bin/test.py "" "username=testuser&password=secret123"

# Test 4: PHP CGI (if available)
if [ -f cgi-bin/test.php ] && command -v php-cgi &> /dev/null; then
    echo -e "\n${YELLOW}Test 4: PHP CGI - GET request${NC}"
    ./cgi_test cgi-bin/test.php

    echo -e "\n${YELLOW}Test 4b: PHP CGI - GET with query string${NC}"
    ./cgi_test cgi-bin/test.php "name=Bob&city=Paris"

    echo -e "\n${YELLOW}Test 4c: PHP CGI - POST request${NC}"
    ./cgi_test cgi-bin/test.php "" "email=test@example.com&message=Hello"
else
    echo -e "\n${YELLOW}Test 4: PHP CGI - SKIPPED (php-cgi not installed or test.php missing)${NC}"
fi

# Test 5: Executable script with shebang
if [ -f cgi-bin/test.sh ]; then
    chmod +x cgi-bin/test.sh
    echo -e "\n${YELLOW}Test 5: Shell script CGI${NC}"
    ./cgi_test cgi-bin/test.sh
fi

# Test 6: Timeout test - slow script with short timeout (should timeout)
if [ -f cgi-bin/slow.py ]; then
    echo -e "\n${YELLOW}Test 6: Timeout test - slow.py (5 seconds sleep, 2 second timeout)${NC}"
    echo -e "${YELLOW}Expected: Should timeout with 504 status${NC}"
    ./cgi_test cgi-bin/slow.py "5" "" 2
fi

# Test 7: Timeout test - slow script with sufficient timeout (should complete)
if [ -f cgi-bin/slow.py ]; then
    echo -e "\n${YELLOW}Test 7: Timeout test - slow.py (2 seconds sleep, 5 second timeout)${NC}"
    echo -e "${YELLOW}Expected: Should complete successfully${NC}"
    ./cgi_test cgi-bin/slow.py "2" "" 5
fi

# Test 8: Timeout test - infinite loop (should timeout)
if [ -f cgi-bin/infinite.py ]; then
    echo -e "\n${YELLOW}Test 8: Timeout test - infinite.py (infinite loop, 3 second timeout)${NC}"
    echo -e "${YELLOW}Expected: Should timeout with 504 status${NC}"
    ./cgi_test cgi-bin/infinite.py "" "" 3
fi

# Cleanup
rm -f cgi_test

echo -e "\n${GREEN}=== All CGI tests completed ===${NC}"
