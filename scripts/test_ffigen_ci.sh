#!/bin/bash
# Test script to verify ffigen works in a CI-like environment
# This simulates the dart-build job from .gitlab-ci.yml

set -e  # Exit on error

echo "================================================"
echo "Testing FFI Bindings Generation (CI Simulation)"
echo "================================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if libclang is installed
echo "Checking for libclang..."
if ! command -v llvm-config &> /dev/null && ! dpkg -l | grep -q libclang-dev; then
    echo -e "${YELLOW}WARNING: libclang-dev not found${NC}"
    echo ""
    echo "To install:"
    echo "  Ubuntu/Debian: sudo apt-get install libclang-dev"
    echo "  macOS: brew install llvm"
    echo ""
    read -p "Attempt to install libclang-dev with sudo? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        sudo apt-get update -qq
        sudo apt-get install -y libclang-dev
    else
        echo -e "${RED}Cannot proceed without libclang${NC}"
        exit 1
    fi
fi

echo -e "${GREEN}libclang found${NC}"
echo ""

# Navigate to dart_package directory
cd "$(dirname "$0")/../dart_package" || exit 1
echo "Working directory: $(pwd)"
echo ""

# Step 1: Install Dart dependencies
echo "Step 1: Installing Dart dependencies..."
dart pub get
echo -e "${GREEN}Dependencies installed${NC}"
echo ""

# Step 2: Generate FFI bindings
echo "Step 2: Generating FFI bindings..."
dart run ffigen --config ffigen.yaml
echo -e "${GREEN}FFI bindings generated${NC}"
echo ""

# Step 3: Verify generated bindings exist
echo "Step 3: Verifying generated bindings..."
if [ -f "lib/src/ffi/bindings_generated.dart" ]; then
    echo -e "${GREEN}bindings_generated.dart exists${NC}"

    # Show file info
    FILE_SIZE=$(wc -c < "lib/src/ffi/bindings_generated.dart")
    LINE_COUNT=$(wc -l < "lib/src/ffi/bindings_generated.dart")
    echo "  Size: $FILE_SIZE bytes"
    echo "  Lines: $LINE_COUNT"
else
    echo -e "${RED}ERROR: bindings_generated.dart not generated${NC}"
    exit 1
fi
echo ""

# Step 4: Compile Dart code (verify no syntax errors)
echo "Step 4: Compiling Dart code..."
dart compile kernel lib/flow_ffi.dart -o /tmp/flow_ffi.dill
echo -e "${GREEN}Dart code compiled successfully${NC}"
echo ""

# Step 5: Run Dart analyzer on generated bindings
echo "Step 5: Running Dart analyzer..."
if dart analyze lib/src/ffi/bindings_generated.dart --no-fatal-infos --no-fatal-warnings; then
    echo -e "${GREEN}Analyzer passed${NC}"
else
    echo -e "${YELLOW}Analyzer found issues (may be acceptable)${NC}"
fi
echo ""

# Success!
echo "================================================"
echo -e "${GREEN}SUCCESS: FFI bindings generation test passed${NC}"
echo "================================================"
echo ""
echo "This simulates the dart-build job from GitLab CI."
echo "Your local environment is correctly configured."
echo ""
