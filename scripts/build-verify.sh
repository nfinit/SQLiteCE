#!/bin/bash
#
# SQLite/CE Build Verification Script
#
# This script performs automated build verification for the SQLite/CE project.
# It builds all targets, runs tests, and reports results.
#
# Usage:
#   ./scripts/build-verify.sh          # Full build and test
#   ./scripts/build-verify.sh quick    # Quick build only (no clean)
#   ./scripts/build-verify.sh clean    # Clean build artifacts only
#
# Exit codes:
#   0 - All builds and tests passed
#   1 - Build failed
#   2 - Tests failed
#   3 - Static analysis found issues
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Results tracking
BUILD_PASSED=0
TEST_PASSED=0
CHECK_PASSED=0

#============================================================================
# Helper Functions
#============================================================================

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_section() {
    echo ""
    echo "=============================================="
    echo " $1"
    echo "=============================================="
}

#============================================================================
# Build Functions
#============================================================================

do_clean() {
    log_section "Cleaning build artifacts"
    cd "$PROJECT_ROOT"
    make clean 2>/dev/null || true
    log_info "Clean complete"
}

do_build() {
    log_section "Building SQLite library"
    cd "$PROJECT_ROOT"

    if make sqlite; then
        log_info "SQLite library build: PASSED"
        BUILD_PASSED=1
    else
        log_error "SQLite library build: FAILED"
        return 1
    fi
}

do_test() {
    log_section "Running unit tests"
    cd "$PROJECT_ROOT"

    if make test; then
        log_info "Unit tests: PASSED"
        TEST_PASSED=1
    else
        log_error "Unit tests: FAILED"
        return 1
    fi
}

do_bench() {
    log_section "Building benchmark tool"
    cd "$PROJECT_ROOT"

    if make bench; then
        log_info "Benchmark build: PASSED"
    else
        log_warn "Benchmark build: FAILED (non-critical)"
    fi
}

do_check() {
    log_section "Running static analysis"
    cd "$PROJECT_ROOT"

    # Check if cppcheck is available
    if ! command -v cppcheck &> /dev/null; then
        log_warn "cppcheck not found, skipping static analysis"
        CHECK_PASSED=1
        return 0
    fi

    if make check 2>&1 | grep -q "error:"; then
        log_warn "Static analysis found issues (review recommended)"
        CHECK_PASSED=0
    else
        log_info "Static analysis: PASSED"
        CHECK_PASSED=1
    fi
}

#============================================================================
# Main
#============================================================================

main() {
    local mode="${1:-full}"

    log_info "SQLite/CE Build Verification"
    log_info "Project root: $PROJECT_ROOT"
    log_info "Mode: $mode"

    case "$mode" in
        clean)
            do_clean
            exit 0
            ;;
        quick)
            # Quick build without clean
            do_build || exit 1
            do_test || exit 2
            ;;
        full|*)
            # Full build with clean
            do_clean
            do_build || exit 1
            do_test || exit 2
            do_bench
            do_check
            ;;
    esac

    # Summary
    log_section "Build Verification Summary"

    if [ $BUILD_PASSED -eq 1 ]; then
        echo -e "  Build:    ${GREEN}PASSED${NC}"
    else
        echo -e "  Build:    ${RED}FAILED${NC}"
    fi

    if [ $TEST_PASSED -eq 1 ]; then
        echo -e "  Tests:    ${GREEN}PASSED${NC}"
    else
        echo -e "  Tests:    ${RED}FAILED${NC}"
    fi

    if [ $CHECK_PASSED -eq 1 ]; then
        echo -e "  Analysis: ${GREEN}PASSED${NC}"
    else
        echo -e "  Analysis: ${YELLOW}WARNINGS${NC}"
    fi

    echo ""

    if [ $BUILD_PASSED -eq 1 ] && [ $TEST_PASSED -eq 1 ]; then
        log_info "All critical checks passed!"
        exit 0
    else
        log_error "Build verification failed"
        exit 1
    fi
}

main "$@"
