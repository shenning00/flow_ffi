#!/bin/bash
# Format all code in the project (C++ and Dart)
# This script applies formatting changes

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=================================="
echo "Formatting Flow FFI Project"
echo "=================================="
echo ""

# C++ Formatting
echo "1. Formatting C++ files..."
cd "$PROJECT_ROOT"

CPP_FILES=$(find src include test -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) \
  ! -path "*/build/*" \
  ! -path "*/.dart_tool/*" \
  ! -path "*/CMakeFiles/*" 2>/dev/null || true)

if [ -z "$CPP_FILES" ]; then
  echo "   No C++ files found to format"
else
  echo "   Found $(echo "$CPP_FILES" | wc -l) C++ files"

  if command -v clang-format &> /dev/null; then
    for file in $CPP_FILES; do
      echo "   Formatting: $file"
      clang-format -i "$file"
    done
    echo "   ✅ C++ formatting complete"
  else
    echo "   ⚠️  clang-format not found. Skipping C++ formatting."
    echo "   Install with: sudo apt-get install clang-format (Ubuntu/Debian)"
    echo "                 brew install clang-format (macOS)"
  fi
fi

echo ""

# Dart Formatting
echo "2. Formatting Dart files..."
cd "$PROJECT_ROOT/dart_package"

if command -v dart &> /dev/null; then
  dart format .
  echo "   ✅ Dart formatting complete"
else
  echo "   ⚠️  dart command not found. Skipping Dart formatting."
  echo "   Install from: https://dart.dev/get-dart"
fi

echo ""
echo "=================================="
echo "✅ Formatting complete!"
echo "=================================="
echo ""
echo "Next steps:"
echo "  1. Review changes: git diff"
echo "  2. Run checks: ./scripts/check_format.sh"
echo "  3. Commit changes: git add . && git commit -m 'Apply code formatting'"
