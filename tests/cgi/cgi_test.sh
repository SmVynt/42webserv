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

./cgi_test

# Cleanup
rm -f cgi_test

echo -e "\n${GREEN}=== All CGI tests completed ===${NC}"
