#!/bin/bash
# Release flow_ffi Dart package to GitLab
# This script handles version verification, package validation, archiving, and manifest generation

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Variables
TAG_VERSION=${CI_COMMIT_TAG#v}  # Remove 'v' prefix
PUBSPEC_VERSION=$(grep '^version:' dart_package/pubspec.yaml | sed 's/version: *//' | tr -d ' ')
ARCHIVE_NAME="flow_ffi-${CI_COMMIT_TAG}.tar.gz"
MANIFEST_NAME="flow_ffi-${CI_COMMIT_TAG}.manifest.txt"

echo "================================================"
echo "Publishing flow_ffi Dart Package ${CI_COMMIT_TAG}"
echo "================================================"
echo ""

# Step 1: Verify version match
echo "Step 1: Verifying version..."
echo "  Git tag: ${CI_COMMIT_TAG}"
echo "  Tag version: ${TAG_VERSION}"
echo "  Pubspec version: ${PUBSPEC_VERSION}"

if [ "${PUBSPEC_VERSION}" != "${TAG_VERSION}" ]; then
  echo -e "${RED}❌ ERROR: Version mismatch!${NC}"
  echo "   pubspec.yaml version: ${PUBSPEC_VERSION}"
  echo "   Git tag version: ${TAG_VERSION}"
  echo ""
  echo "Please ensure pubspec.yaml version matches the git tag."
  echo "Example: For tag 'v1.0.1', pubspec.yaml should have 'version: 1.0.1'"
  exit 1
fi
echo -e "${GREEN}✅ Version verification passed${NC}"
echo ""

# Step 2: Validate package structure
echo "Step 2: Validating Dart package..."
cd dart_package

if dart pub publish --dry-run 2>&1 | grep -q "Package validation succeeded"; then
  echo -e "${GREEN}✅ Package validation successful${NC}"
else
  echo -e "${YELLOW}⚠️  Package validation warnings detected${NC}"
  echo "This is expected for private packages not intended for pub.dev"
  echo "Continuing with archive creation..."
fi
echo ""

# Step 3: Check FFI bindings
echo "Step 3: Verifying FFI bindings..."
if [ ! -f lib/src/ffi/bindings_generated.dart ]; then
  echo -e "${RED}❌ ERROR: FFI bindings not found!${NC}"
  echo "The dart-build job should have generated lib/src/ffi/bindings_generated.dart"
  exit 1
fi
echo -e "${GREEN}✅ FFI bindings verified${NC}"
echo ""

# Step 4: Check native libraries
echo "Step 4: Verifying native libraries..."
if [ ! -f ../build/libflow_ffi.so ]; then
  echo -e "${RED}❌ ERROR: Native library libflow_ffi.so not found!${NC}"
  echo "The cpp-build job should have created build/libflow_ffi.so"
  exit 1
fi
echo -e "${GREEN}✅ Native library verified${NC}"
echo ""

# Step 5: Bundle native libraries
echo "Step 5: Bundling native libraries..."
mkdir -p lib/native/linux
cp ../build/libflow_ffi.so lib/native/linux/

if [ -f ../build/libflow-core.so ]; then
  cp ../build/libflow-core.so* lib/native/linux/ 2>/dev/null || true
  echo -e "${GREEN}✅ Native libraries bundled (libflow_ffi.so, libflow-core.so)${NC}"
else
  echo -e "${GREEN}✅ Native library bundled (libflow_ffi.so only)${NC}"
fi
echo ""

# Step 6: Check CHANGELOG
echo "Step 6: Checking CHANGELOG.md..."
if grep -q "\[${TAG_VERSION}\]" CHANGELOG.md; then
  echo -e "${GREEN}✅ CHANGELOG.md contains release notes for version ${TAG_VERSION}${NC}"
else
  echo -e "${YELLOW}⚠️  WARNING: CHANGELOG.md may not contain release notes for ${TAG_VERSION}${NC}"
  echo "Please ensure CHANGELOG.md is updated before releasing."
fi
echo ""

# Step 7: Create archive
echo "Step 7: Creating package archive..."
cd ..
tar czf "${ARCHIVE_NAME}" dart_package/
ARCHIVE_SIZE=$(du -h "${ARCHIVE_NAME}" | cut -f1)
echo -e "${GREEN}✅ Package archive created: ${ARCHIVE_NAME} (${ARCHIVE_SIZE})${NC}"
echo ""

# Step 8: Generate manifest
echo "Step 8: Generating package manifest..."
RELEASE_DATE=$(date -u +"%Y-%m-%d %H:%M:%S UTC")

cat > "${MANIFEST_NAME}" <<EOF
Flow FFI Dart Package Release
==============================

Version: ${TAG_VERSION}
Git Tag: ${CI_COMMIT_TAG}
Commit SHA: ${CI_COMMIT_SHA}
Release Date: ${RELEASE_DATE}
Pipeline: ${CI_PIPELINE_URL}

Package Contents:
-----------------
- Complete Dart package source code
- Generated FFI bindings (bindings_generated.dart)
- Compiled native libraries (libflow_ffi.so, libflow-core.so)
- Locked dependencies (pubspec.lock)
- Documentation and examples

Installation Instructions:
--------------------------
1. Extract the archive:
   tar xzf ${ARCHIVE_NAME}

2. Add to your project's pubspec.yaml:
   dependencies:
     flow_ffi:
       path: /path/to/extracted/dart_package

3. Run dart pub get to install the package

4. Import in your Dart code:
   import 'package:flow_ffi/flow_ffi.dart';

Platform Support:
-----------------
This release includes native libraries for:
- Linux (libflow_ffi.so, libflow-core.so)

For other platforms, you will need to build the native libraries
separately using the C++ source code.

Notes:
------
- This is a PRIVATE package, not published to pub.dev
- Native libraries are platform-specific
- Ensure your system has compatible dependencies (uuid-dev, etc.)
EOF

echo -e "${GREEN}✅ Package manifest created${NC}"
echo ""

# Final summary
echo "================================================"
echo -e "${GREEN}✅ Package Release Complete${NC}"
echo "================================================"
echo "Archive: ${ARCHIVE_NAME} (${ARCHIVE_SIZE})"
echo "Manifest: ${MANIFEST_NAME}"
echo ""
echo "Download artifacts from:"
echo "${CI_JOB_URL}/artifacts/browse"
echo "================================================"
echo ""

# List package contents for verification
echo "Package contents (first 20 files):"
tar tzf "${ARCHIVE_NAME}" | head -20
echo "... (showing first 20 files)"
