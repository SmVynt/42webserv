#!/bin/bash

# WebServ Testing Quick Start
# Run this script to get started with testing immediately

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
MAGENTA='\033[0;35m'
NC='\033[0m'

clear

echo -e "${MAGENTA}"
cat << "EOF"
╔══════════════════════════════════════════════════════════╗
║                                                          ║
║         WebServ Testing Framework - Quick Start          ║
║                                                          ║
╚══════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

echo ""
echo -e "${BLUE}What would you like to do?${NC}"
echo ""
echo "  1) Run complete test suite (all tests)"
echo "  2) Run quick validation tests (5-10 minutes)"
echo "  3) Run load tests only (performance/stress)"
echo "  4) Run memory leak detection"
echo "  5) Run edge case tests (malformed requests)"
echo "  6) Run static file serving tests"
echo "  7) Compare with NGINX"
echo "  8) Open browser test page"
echo "  9) Install missing test dependencies"
echo "  x) Stop running webserv server"
echo "  0) Exit"
echo ""
read -p "Enter your choice: " choice

case $choice in
    1)
        echo ""
        echo -e "${GREEN}Running complete test suite...${NC}"
        echo "This will take 10-30 minutes depending on your system."
        echo ""
        read -p "Continue? [y/N]: " confirm
        if [[ $confirm =~ ^[Yy]$ ]]; then
            cd tests
            ./test_suite.sh
        fi
        ;;

    2)
        echo ""
        echo -e "${GREEN}Running quick validation tests...${NC}"
        cd tests
        ./test_suite.sh --skip-memory --skip-nginx
        ;;

    3)
        echo ""
        echo -e "${GREEN}Running load tests...${NC}"
        cd tests/load
        ./siege_test.sh && ./ab_test.sh && ./concurrent_test.sh
        ;;

    4)
        echo ""
        echo -e "${GREEN}Running memory leak detection...${NC}"
        echo "This will take several minutes..."
        cd tests/memory
        ./valgrind_test.sh
        ;;

    5)
        echo ""
        echo -e "${GREEN}Running edge case tests...${NC}"
        cd tests/edge_cases
        ./malformed_requests.sh && ./boundary_tests.sh && ./protocol_edge_cases.sh
        ;;

    6)
        echo ""
        echo -e "${GREEN}Running static file serving tests...${NC}"
        cd tests/static
        ./serve_test.sh
        ;;

    7)
        echo ""
        echo -e "${GREEN}Setting up NGINX comparison...${NC}"
        cd tests/comparison
        ./nginx_setup.sh
        echo ""
        echo -e "${YELLOW}NGINX configuration created.${NC}"
        echo ""
        read -p "Run comparison now? [y/N]: " confirm
        if [[ $confirm =~ ^[Yy]$ ]]; then
            ./compare_results.sh
        fi
        ;;

    8)
        echo ""
        echo -e "${GREEN}Preparing browser test page...${NC}"

        # Check if server is running
        if nc -z localhost 8080 2>/dev/null; then
            echo -e "${GREEN}✓${NC} Server is running"
        else
            echo -e "${YELLOW}Starting server...${NC}"
            ./webserv config/default.conf &
            SERVER_PID=$!
            sleep 2
            echo -e "${GREEN}✓${NC} Server started (PID: $SERVER_PID)"
        fi

        # Copy test page if not in www
        if [ ! -f www/upload_test.html ]; then
            cp tests/browser/upload_test.html www/
            echo -e "${GREEN}✓${NC} Upload test page copied to www/"
        fi

        echo ""
        echo -e "${GREEN}Ready for browser testing!${NC}"
        echo ""
        echo "Open in your browser:"
        echo -e "  ${BLUE}http://localhost:8080/upload_test.html ${NC}"
        echo ""
        echo "Manual testing checklist at:"
        echo "  tests/browser/manual_checklist.md"
        echo ""
        ;;

    9)
        echo ""
        echo -e "${YELLOW}Checking and installing dependencies...${NC}"
        echo ""

        # Check which are missing
        missing=()

        command -v curl &>/dev/null || missing+=("curl")
        command -v nc &>/dev/null || missing+=("netcat-openbsd")
        command -v bc &>/dev/null || missing+=("bc")
        command -v siege &>/dev/null || missing+=("siege")
        command -v ab &>/dev/null || missing+=("apache2-utils")
        command -v valgrind &>/dev/null || missing+=("valgrind")
        command -v nginx &>/dev/null || missing+=("nginx")

        if [ ${#missing[@]} -eq 0 ]; then
            echo -e "${GREEN}✓ All dependencies installed!${NC}"
        else
            echo "Missing packages: ${missing[*]}"
            echo ""
            echo "Install command:"
            echo "  sudo apt-get install -y ${missing[*]}"
            echo ""
            read -p "Install now? [y/N]: " confirm
            if [[ $confirm =~ ^[Yy]$ ]]; then
                sudo apt-get update
                sudo apt-get install -y "${missing[@]}"
                echo ""
                echo -e "${GREEN}✓ Dependencies installed!${NC}"
            fi
        fi
        ;;

    x)
        echo ""
        echo -e "${YELLOW}Stopping running webserv server...${NC}"
        echo ""

        # Find running webserv processes
        WEBSERV_PIDS=$(pgrep -x webserv 2>/dev/null || true)

        if [ -z "$WEBSERV_PIDS" ]; then
            echo -e "${GREEN}✓${NC} No running webserv processes found"
        else
            echo "Found webserv process(es): $WEBSERV_PIDS"
            kill $WEBSERV_PIDS 2>/dev/null || true
            sleep 1

            # Check if still running and force kill if needed
            STILL_RUNNING=$(pgrep -x webserv 2>/dev/null || true)
            if [ -n "$STILL_RUNNING" ]; then
                echo "Force killing: $STILL_RUNNING"
                kill -9 $STILL_RUNNING 2>/dev/null || true
            fi

            echo -e "${GREEN}✓${NC} webserv stopped"
        fi
        ;;

    0)
        echo "Exiting..."
        exit 0
        ;;

    *)
        echo "Invalid choice"
        exit 1
        ;;
esac

echo ""
echo -e "${GREEN}═══════════════════════════════════════════════════${NC}"
echo ""
