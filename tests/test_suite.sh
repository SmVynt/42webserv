#!/bin/bash

# WebServ Complete Test Suite
# Usage: bash tests/test_suite.sh [options]

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$TEST_DIR/.." && pwd)"
SERVER_BIN="$PROJECT_ROOT/webserv"
CONFIG_FILE="$PROJECT_ROOT/config/default.conf"
LOG_DIR="$TEST_DIR/logs"
RESULTS_DIR="$TEST_DIR/results"

# Test options
RUN_LOAD_TESTS=true
RUN_MEMORY_TESTS=false
RUN_EDGE_TESTS=true
RUN_NGINX_COMPARISON=false
RUN_STATIC_TESTS=true
GENERATE_REPORT=true

# Tool availability flags (detected at runtime)
HAS_SIEGE=false
HAS_AB=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-load)
            RUN_LOAD_TESTS=false
            shift
            ;;
        --skip-memory)
            RUN_MEMORY_TESTS=false
            shift
            ;;
        --run-memory)
            RUN_MEMORY_TESTS=true
            shift
            ;;
        --skip-edge)
            RUN_EDGE_TESTS=false
            shift
            ;;
        --skip-nginx)
            RUN_NGINX_COMPARISON=false
            shift
            ;;
        --skip-static)
            RUN_STATIC_TESTS=false
            shift
            ;;
        --no-report)
            GENERATE_REPORT=false
            shift
            ;;
        --quick)
            RUN_LOAD_TESTS=false
            RUN_MEMORY_TESTS=false
            RUN_NGINX_COMPARISON=false
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --skip-load      Skip load testing"
            echo "  --run-memory     Run memory leak tests (skipped by default)"
            echo "  --skip-edge      Skip edge case tests"
            echo "  --skip-nginx     Skip NGINX comparison"
            echo "  --skip-static    Skip static file tests"
            echo "  --no-report      Don't generate HTML report"
            echo "  -h, --help       Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
			echo " use -h or --help to show a list of options"
            exit 1
            ;;
    esac
done

# Create necessary directories
mkdir -p "$LOG_DIR" "$RESULTS_DIR"

# Timestamp for this test run
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RUN_LOG="$LOG_DIR/run_${TIMESTAMP}.log"
SUMMARY_FILE="$RESULTS_DIR/summary_${TIMESTAMP}.txt"

# Logging function
log() {
    echo -e "$1" | tee -a "$RUN_LOG"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$RUN_LOG"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1" | tee -a "$RUN_LOG"
}

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1" | tee -a "$RUN_LOG"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1" | tee -a "$RUN_LOG"
}

# Check dependencies
check_dependencies() {
    log_info "Checking dependencies..."

    local missing_deps=()

    # Required tools
    command -v curl &>/dev/null || missing_deps+=("curl")
    command -v nc &>/dev/null || missing_deps+=("netcat")

    if [ "$RUN_LOAD_TESTS" = true ]; then
        if command -v siege &>/dev/null; then
            HAS_SIEGE=true
            log_info "siege found - siege load tests enabled"
        else
            log_warning "siege not found - siege load tests will be skipped"
        fi
        if command -v ab &>/dev/null; then
            HAS_AB=true
            log_info "ab (Apache Bench) found - ab load tests enabled"
        else
            log_warning "ab (Apache Bench) not found - ab load tests will be skipped"
        fi
    fi

    if [ "$RUN_MEMORY_TESTS" = true ]; then
        command -v valgrind &>/dev/null || log_warning "valgrind not found - memory tests will be skipped"
    fi

    if [ "$RUN_NGINX_COMPARISON" = true ]; then
        command -v nginx &>/dev/null || log_warning "nginx not found - comparison tests will be skipped"
    fi

    if [ ${#missing_deps[@]} -ne 0 ]; then
        log_error "Missing required dependencies: ${missing_deps[*]}"
        exit 1
    fi

    log_success "All required dependencies available"
}

# Build server
build_server() {
    log_info "Building webserv..."
    cd "$PROJECT_ROOT"

    if make -j4 >> "$RUN_LOG" 2>&1; then
        log_success "Build successful"
    else
        log_error "Build failed. Check $RUN_LOG for details."
        exit 1
    fi
}

# Test results tracking
declare -A TEST_RESULTS
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

record_test() {
    local test_name=$1
    local status=$2
    TEST_RESULTS["$test_name"]=$status
    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    if [ "$status" = "PASS" ]; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
        log_success "✓ $test_name"
    elif [ "$status" = "SKIP" ]; then
        SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        log_warning "⊘ $test_name (skipped)"
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
        log_error "✗ $test_name"
    fi
}

# Run test category
run_test_category() {
    local category=$1
    local script=$2

    log ""
    log "${CYAN}========================================${NC}"
    log "${CYAN}Running: $category${NC}"
    log "${CYAN}========================================${NC}"

    if [ -f "$script" ]; then
        set +e
        bash "$script" 2>&1 | tee -a "$RUN_LOG"
        local status=${PIPESTATUS[0]}
        set -e
        if [ $status -eq 0 ]; then
            record_test "$category" "PASS"
        else
            record_test "$category" "FAIL"
        fi
        return 0
    else
        log_warning "Test script not found: $script"
        record_test "$category" "SKIP"
        return 0
    fi
}

# Generate HTML report
generate_html_report() {
    local report_file="$RESULTS_DIR/report_${TIMESTAMP}.html"

    cat > "$report_file" << 'EOF'
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WebServ Test Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        h1 { color: #333; border-bottom: 3px solid #4CAF50; padding-bottom: 10px; }
        .summary { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin: 20px 0; }
        .stat-box { padding: 20px; border-radius: 8px; text-align: center; }
        .stat-box h3 { margin: 0; font-size: 2em; }
        .stat-box p { margin: 5px 0 0 0; color: #666; }
        .total { background: #2196F3; color: white; }
        .passed { background: #4CAF50; color: white; }
        .failed { background: #f44336; color: white; }
        .skipped { background: #9E9E9E; color: white; }
        .pass-rate { background: #FF9800; color: white; }
        table { width: 100%; border-collapse: collapse; margin: 20px 0; }
        th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }
        th { background: #4CAF50; color: white; }
        tr:hover { background: #f5f5f5; }
        .status-pass { color: #4CAF50; font-weight: bold; }
        .status-fail { color: #f44336; font-weight: bold; }
        .status-skip { color: #FF9800; font-weight: bold; }
        .timestamp { color: #666; font-size: 0.9em; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 WebServ Test Report</h1>
        <p class="timestamp">Generated: TIMESTAMP_PLACEHOLDER</p>

        <div class="summary">
            <div class="stat-box total">
                <h3>TOTAL_TESTS_PLACEHOLDER</h3>
                <p>Total Tests</p>
            </div>
            <div class="stat-box passed">
                <h3>PASSED_TESTS_PLACEHOLDER</h3>
                <p>Passed</p>
            </div>
            <div class="stat-box failed">
                <h3>FAILED_TESTS_PLACEHOLDER</h3>
                <p>Failed</p>
            </div>
            <div class="stat-box skipped">
                <h3>SKIPPED_TESTS_PLACEHOLDER</h3>
                <p>Skipped</p>
            </div>
            <div class="stat-box pass-rate">
                <h3>PASS_RATE_PLACEHOLDER%</h3>
                <p>Pass Rate</p>
            </div>
        </div>

        <h2>Test Results</h2>
        <table>
            <thead>
                <tr>
                    <th>Test Category</th>
                    <th>Status</th>
                </tr>
            </thead>
            <tbody>
TEST_RESULTS_PLACEHOLDER
            </tbody>
        </table>

        <h2>Details</h2>
        <p>Full logs available at: <code>LOG_PATH_PLACEHOLDER</code></p>
    </div>
</body>
</html>
EOF

    # Replace placeholders
    local pass_rate=0
    local active_tests=$((TOTAL_TESTS - SKIPPED_TESTS))
    if [ $active_tests -gt 0 ]; then
        pass_rate=$((PASSED_TESTS * 100 / active_tests))
    fi

    sed -i "s|TIMESTAMP_PLACEHOLDER|$(date)|g" "$report_file"
    sed -i "s|TOTAL_TESTS_PLACEHOLDER|$TOTAL_TESTS|g" "$report_file"
    sed -i "s|PASSED_TESTS_PLACEHOLDER|$PASSED_TESTS|g" "$report_file"
    sed -i "s|FAILED_TESTS_PLACEHOLDER|$FAILED_TESTS|g" "$report_file"
    sed -i "s|SKIPPED_TESTS_PLACEHOLDER|$SKIPPED_TESTS|g" "$report_file"
    sed -i "s|PASS_RATE_PLACEHOLDER|$pass_rate|g" "$report_file"
    sed -i "s|LOG_PATH_PLACEHOLDER|$RUN_LOG|g" "$report_file"

    # Generate test results table
    local results_html=""
    for test_name in "${!TEST_RESULTS[@]}"; do
        local status="${TEST_RESULTS[$test_name]}"
        local status_class="status-${status,,}"
        results_html+="                <tr><td>$test_name</td><td class=\"$status_class\">$status</td></tr>\n"
    done

    # Use a temp file to avoid sed issues with multiline
    echo "$results_html" > "$RESULTS_DIR/tmp_results.txt"
    sed -i "/TEST_RESULTS_PLACEHOLDER/r $RESULTS_DIR/tmp_results.txt" "$report_file"
    sed -i "/TEST_RESULTS_PLACEHOLDER/d" "$report_file"
    rm "$RESULTS_DIR/tmp_results.txt"

    log_success "HTML report generated: $report_file"
}

# Main test execution
main() {
    log "${MAGENTA}╔════════════════════════════════════════╗${NC}"
    log "${MAGENTA}║   WebServ Complete Test Suite v0.1    ║${NC}"
    log "${MAGENTA}╚════════════════════════════════════════╝${NC}"
    log ""
    log "Test run started at: $(date)"
    log "Log file: $RUN_LOG"
    log ""

    # Pre-flight checks
    check_dependencies
    build_server
    cd "$TEST_DIR"

    # Default to IPv4 localhost to avoid IPv6-only resolution issues.
    : "${SERVER_HOST:=127.0.0.1}"
    : "${SERVER_PORT:=8080}"
    : "${SERVER_URL:=http://${SERVER_HOST}:${SERVER_PORT}}"
    : "${SERVER_BIN:=$PROJECT_ROOT/webserv}"
    : "${CONFIG_FILE:=$PROJECT_ROOT/config/default.conf}"
    export SERVER_HOST SERVER_PORT SERVER_URL SERVER_BIN CONFIG_FILE PROJECT_ROOT

    # Run test categories
    if [ "$RUN_STATIC_TESTS" = true ]; then
        run_test_category "Static File Serving" "$TEST_DIR/static/serve_test.sh"
    fi

    if [ "$RUN_EDGE_TESTS" = true ]; then
        run_test_category "Malformed Requests" "$TEST_DIR/edge_cases/malformed_requests.sh"
        run_test_category "Boundary Tests" "$TEST_DIR/edge_cases/boundary_tests.sh"
        run_test_category "Protocol Edge Cases" "$TEST_DIR/edge_cases/protocol_edge_cases.sh"
    fi

    if [ "$RUN_LOAD_TESTS" = true ]; then
        if [ "$HAS_SIEGE" = true ]; then
            run_test_category "Siege Load Test" "$TEST_DIR/load/siege_test.sh"
        else
            log_warning "Skipping Siege Load Test (siege not installed)"
            record_test "Siege Load Test" "SKIP"
        fi
        if [ "$HAS_AB" = true ]; then
            run_test_category "Apache Bench Test" "$TEST_DIR/load/ab_test.sh"
        else
            log_warning "Skipping Apache Bench Test (ab not installed)"
            record_test "Apache Bench Test" "SKIP"
        fi
        run_test_category "Concurrent Clients" "$TEST_DIR/load/concurrent_test.sh"
    fi

    if [ "$RUN_MEMORY_TESTS" = true ]; then
        run_test_category "Memory Leak Detection" "$TEST_DIR/memory/valgrind_test.sh"
    fi

    if [ "$RUN_NGINX_COMPARISON" = true ]; then
        run_test_category "NGINX Comparison" "$TEST_DIR/comparison/compare_results.sh"
    fi

    # Summary
    log ""
    log "${CYAN}========================================${NC}"
    log "${CYAN}Test Summary${NC}"
    log "${CYAN}========================================${NC}"
    log "Total Tests:  $TOTAL_TESTS"
    log "${GREEN}Passed:       $PASSED_TESTS${NC}"
    log "${RED}Failed:       $FAILED_TESTS${NC}"
    log "${YELLOW}Skipped:      $SKIPPED_TESTS${NC}"

    local active_tests=$((TOTAL_TESTS - SKIPPED_TESTS))
    if [ $active_tests -gt 0 ]; then
        local pass_rate=$((PASSED_TESTS * 100 / active_tests))
        log "Pass Rate:    ${pass_rate}% (of non-skipped tests)"
    fi

    # Write summary to file
    {
        echo "WebServ Test Suite Summary"
        echo "=========================="
        echo "Timestamp: $(date)"
        echo "Total Tests: $TOTAL_TESTS"
        echo "Passed: $PASSED_TESTS"
        echo "Failed: $FAILED_TESTS"
        echo "Skipped: $SKIPPED_TESTS"
        echo ""
        echo "Detailed Results:"
        for test_name in "${!TEST_RESULTS[@]}"; do
            echo "  - $test_name: ${TEST_RESULTS[$test_name]}"
        done
    } > "$SUMMARY_FILE"

    # Generate HTML report
    if [ "$GENERATE_REPORT" = true ]; then
        generate_html_report
    fi

    log ""
    log_info "Summary saved to: $SUMMARY_FILE"
    log_info "Full log saved to: $RUN_LOG"

    # Exit with appropriate code
    if [ $FAILED_TESTS -eq 0 ]; then
        log ""
        log "${GREEN}╔════════════════════════════════════════╗${NC}"
        log "${GREEN}║     All Tests Passed!                  ║${NC}"
        log "${GREEN}╚════════════════════════════════════════╝${NC}"
        exit 0
    else
        log ""
        log "${RED}╔════════════════════════════════════════╗${NC}"
        log "${RED}║     Some Tests Failed!                 ║${NC}"
        log "${RED}╚════════════════════════════════════════╝${NC}"
        exit 1
    fi
}

# Trap to handle interrupts
trap 'log_error "Test suite interrupted"; exit 130' INT TERM

# Run main
main
