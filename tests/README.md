# WebServ Testing Framework

A comprehensive testing suite for the webserv HTTP server implementation.

## 📋 Table of Contents

- [Quick Start](#quick-start)
- [Test Categories](#test-categories)
- [Usage](#usage)
- [Requirements](#requirements)
- [Test Structure](#test-structure)
- [Interpreting Results](#interpreting-results)

## Quick Start

```bash
# Build the server
make

# Run all tests
make test

# Run quick validation tests only
make test-quick

# Get help on available test targets
make test-help
```

## Test Categories

### 1. Load Testing (`load/`)
Tests server availability and performance under various load conditions.

**Tools:**
- **siege_test.sh** - Tests with siege (10, 50, 100 concurrent users)
- **ab_test.sh** - Apache Bench performance testing
- **concurrent_test.sh** - Multiple simultaneous client connections

**Run:**
```bash
make test-load
# or
cd tests/load && ./siege_test.sh
```

**Requirements:** `siege`, `apache2-utils` (ab)

### 2. Memory Leak Detection (`memory/`)
Validates memory management and detects leaks using valgrind.

**Tests:**
- Startup/shutdown leak check
- Memory stability under load
- File descriptor leak detection

**Run:**
```bash
make test-memory
# or
cd tests/memory && ./valgrind_test.sh
```

**Requirements:** `valgrind`

### 3. Edge Case Testing (`edge_cases/`)
Tests server resilience against malformed requests and boundary conditions.

**Tests:**
- **malformed_requests.sh** - Invalid HTTP requests, missing headers, protocol violations
- **boundary_tests.sh** - Extreme values, large headers/URIs, numerical boundaries
- **protocol_edge_cases.sh** - HTTP protocol compliance, header variations

**Run:**
```bash
make test-edge
# or
cd tests/edge_cases && ./malformed_requests.sh
```

### 4. Static File Serving (`static/`)
Validates static content delivery and MIME type handling.

**Tests:**
- Basic HTML/CSS/JS serving
- MIME type detection
- HEAD requests
- Range requests
- Directory listing
- Large file handling

**Run:**
```bash
make test-static
# or
cd tests/static && ./serve_test.sh
```

### 5. Browser Testing (`browser/`)
Manual testing checklist and upload testing interface.

**Resources:**
- **manual_checklist.md** - Comprehensive browser testing checklist
- **upload_test.html** - Interactive file upload testing page

**Usage:**
1. Start server: `./webserv config/default.conf`
2. Copy upload_test.html to www/: `cp tests/browser/upload_test.html www/`
3. Open in browser: `http://localhost:8080/upload_test.html`
4. Follow checklist in manual_checklist.md

### 6. NGINX Comparison (`comparison/`)
Side-by-side comparison with NGINX for behavior validation.

**Tools:**
- **nginx_setup.sh** - Generates matching NGINX configuration
- **compare_results.sh** - Runs comparison tests

**Run:**
```bash
make test-nginx
# or
cd tests/comparison && ./nginx_setup.sh && ./compare_results.sh
```

**Requirements:** `nginx`

### 7. CGI Testing (`cgi_tester.cpp`, `test_cgi.sh`)
Tests CGI script execution.

**Run:**
```bash
make test-cgi
# or
cd tests && ./test_cgi.sh
```

## 💻 Usage

### Master Test Suite

Run all tests with a single command:

```bash
cd tests
./test_suite.sh
```

**Options:**
```bash
./test_suite.sh --skip-load      # Skip load testing
./test_suite.sh --skip-memory    # Skip memory tests
./test_suite.sh --skip-edge      # Skip edge case tests
./test_suite.sh --skip-nginx     # Skip NGINX comparison
./test_suite.sh --no-report      # Don't generate HTML report
./test_suite.sh --help           # Show help
```

### Individual Test Categories

```bash
# Using Makefile targets (recommended)
make test-load          # Load tests only
make test-memory        # Memory tests only
make test-edge          # Edge cases only
make test-static        # Static serving only
make test-nginx         # NGINX comparison only
make test-quick         # Quick validation (static + malformed)

# Or run scripts directly
cd tests/load && ./ab_test.sh
cd tests/edge_cases && ./boundary_tests.sh
```

### Environment Variables

Customize test behavior with environment variables:

```bash
# Change server URL/port
SERVER_HOST=192.168.1.100 SERVER_PORT=9000 ./test_suite.sh

# NGINX comparison port
NGINX_PORT=8081 make test-nginx

# Test duration
TEST_DURATION=60 ./siege_test.sh
```

## 📦 Requirements

### Required (Minimum)
- `bash` (4.0+)
- `curl`
- `nc` (netcat)
- `bc` (for calculations)

### Optional (Enhanced Testing)
- `siege` - Load testing (`sudo apt-get install siege`)
- `apache2-utils` - Apache Bench (`sudo apt-get install apache2-utils`)
- `valgrind` - Memory leak detection (`sudo apt-get install valgrind`)
- `nginx` - Comparison testing (`sudo apt-get install nginx`)

### Install All Dependencies (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y curl netcat-openbsd bc siege apache2-utils valgrind nginx
```

## 🗂️ Test Structure

```
tests/
├── test_suite.sh              # Master test orchestrator
├── test_cgi.sh               # CGI test runner
├── cgi_tester.cpp            # CGI test program
│
├── load/                     # Load & performance tests
│   ├── siege_test.sh
│   ├── ab_test.sh
│   └── concurrent_test.sh
│
├── memory/                   # Memory leak detection
│   └── valgrind_test.sh
│
├── edge_cases/              # Edge case & resilience tests
│   ├── malformed_requests.sh
│   ├── boundary_tests.sh
│   └── protocol_edge_cases.sh
│
├── browser/                 # Manual browser testing
│   ├── manual_checklist.md
│   └── upload_test.html
│
├── comparison/              # NGINX comparison
│   ├── nginx_setup.sh
│   └── compare_results.sh
│
├── static/                  # Static file serving tests
│   └── serve_test.sh
│
├── logs/                    # Test run logs (generated)
└── results/                 # Test results & reports (generated)
```

## 📈 Interpreting Results

### Exit Codes

- `0` - All tests passed
- `1` - Some tests failed
- `130` - Tests interrupted by user

### Test Output

Tests use color-coded output:
- 🟢 **Green ✓** - Test passed
- 🔴 **Red ✗** - Test failed
- 🟡 **Yellow ⚠** - Warning (test passed with issues)

### Generated Files

After running tests:

```
tests/
├── logs/
│   └── run_YYYYMMDD_HHMMSS.log      # Detailed execution log
├── results/
│   ├── summary_YYYYMMDD_HHMMSS.txt  # Text summary
│   └── report_YYYYMMDD_HHMMSS.html  # Interactive HTML report
```

Open the HTML report in a browser for a comprehensive view:
```bash
firefox tests/results/report_*.html
```

### Common Issues

#### Server Won't Start
```
Error: Failed to start server
```
**Solution:** Check if port 8080 is already in use:
```bash
lsof -i :8080
# Kill existing server if needed
pkill webserv
```

#### Missing Dependencies
```
Warning: siege not found - load tests will be limited
```
**Solution:** Install optional dependencies:
```bash
sudo apt-get install siege apache2-utils valgrind nginx
```

#### Permission Denied
```
Error: Permission denied
```
**Solution:** Make scripts executable:
```bash
chmod +x tests/**/*.sh
```

## 🎯 Testing Strategy

### Development Workflow

1. **During Development** - Run quick tests frequently:
   ```bash
   make test-quick
   ```

2. **Before Commits** - Run edge case tests:
   ```bash
   make test-edge
   ```

3. **Before Merging** - Run full test suite:
   ```bash
   make test
   ```

4. **Performance Tuning** - Focus on load tests:
   ```bash
   make test-load
   ```

5. **Memory Debugging** - Regular leak checks:
   ```bash
   make test-memory
   ```

### Continuous Integration

For CI/CD pipelines:

```bash
#!/bin/bash
# ci_test.sh

# Build
make re || exit 1

# Quick validation
make test-quick || exit 1

# Full test suite (allow some failures)
make test || echo "Some tests failed, review logs"

# Memory check (strict)
make test-memory || exit 1
```

## 🔧 Customization

### Adding New Tests

1. Create test script in appropriate category directory
2. Follow existing script structure (start_server, stop_server, trap)
3. Use color-coded output for consistency
4. Add to test_suite.sh if needed

Example template:
```bash
#!/bin/bash
set -euo pipefail

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

start_server() {
    # Server startup logic
}

stop_server() {
    # Cleanup logic
}

trap stop_server EXIT

echo "Running my test..."
start_server

# Test logic here

if [ success ]; then
    echo -e "${GREEN}✓${NC} Test passed"
    exit 0
else
    echo -e "${RED}✗${NC} Test failed"
    exit 1
fi
```

### Modifying Test Parameters

Edit variables at top of scripts:
```bash
# In siege_test.sh
TEST_DURATION="${TEST_DURATION:-30s}"  # Change default duration
NUM_CLIENTS="${NUM_CLIENTS:-100}"      # Change client count
```

## 📚 Additional Resources

- **CGI Development:** `../docs/CGI_DEVELOPMENT.md`
- **Server Configuration:** `../config/default.conf`
- **Project README:** `../README.md`

## 🤝 Contributing

When adding tests:
1. Follow existing script conventions
2. Include error handling (set -euo pipefail)
3. Clean up resources (use trap)
4. Document test purpose in comments
5. Use descriptive test names

## 📝 License

Part of the webserv project.

---

**Happy Testing! 🎉**

For questions or issues, check the logs in `tests/logs/` directory.
