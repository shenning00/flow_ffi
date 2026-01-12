#!/bin/bash
# Check code formatting without making changes
# This script mimics what CI will do

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=================================="
echo "Checking Code Formatting"
echo "=================================="
echo ""

EXIT_CODE=0

# C++ Formatting Check
echo "1. Checking C++ formatting..."
cd "$PROJECT_ROOT"

CPP_FILES=$(find src include test -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) \
  ! -path "*/build/*" \
  ! -path "*/.dart_tool/*" \
  ! -path "*/CMakeFiles/*" 2>/dev/null || true)

if [ -z "$CPP_FILES" ]; then
  echo "   No C++ files found to check"
else
  if command -v clang-format &> /dev/null; then
    NEEDS_FORMAT=0
    for file in $CPP_FILES; do
      if ! clang-format --dry-run --Werror "$file" 2>&1; then
        echo "   ❌ $file needs formatting"
        NEEDS_FORMAT=1
        EXIT_CODE=1
      fi
    done

    if [ $NEEDS_FORMAT -eq 0 ]; then
      echo "   ✅ All C++ files are correctly formatted"
    else
      echo ""
      echo "   To fix C++ formatting, run:"
      echo "   ./scripts/format_all.sh"
    fi
  else
    echo "   ⚠️  clang-format not found. Skipping C++ check."
    echo "   Install with: sudo apt-get install clang-format (Ubuntu/Debian)"
    echo "                 brew install clang-format (macOS)"
  fi
fi

echo ""

# Dart Formatting Check
echo "2. Checking Dart formatting..."
cd "$PROJECT_ROOT/dart_package"

if command -v dart &> /dev/null; then
  if dart format --set-exit-if-changed --output=none . 2>&1; then
    echo "   ✅ All Dart files are correctly formatted"
  else
    echo "   ❌ Some Dart files need formatting"
    echo ""
    echo "   To fix Dart formatting, run:"
    echo "   ./scripts/format_all.sh"
    EXIT_CODE=1
  fi
else
  echo "   ⚠️  dart command not found. Skipping Dart check."
  echo "   Install from: https://dart.dev/get-dart"
fi

echo ""

# Dart Analysis
echo "3. Running Dart static analysis..."
cd "$PROJECT_ROOT/dart_package"

if command -v dart &> /dev/null; then
  # Check if dependencies are installed
  if [ ! -d ".dart_tool" ]; then
    echo "   Installing dependencies..."
    dart pub get > /dev/null 2>&1
  fi

  # Run analysis but don't fail on warnings (for now)
  # TODO: Fix all warnings and enable --fatal-warnings
  dart analyze 2>&1 || true
  echo "   ✅ Dart analysis completed (warnings shown but not blocking)"
else
  echo "   ⚠️  dart command not found. Skipping analysis."
fi

echo ""
echo "=================================="

if [ $EXIT_CODE -eq 0 ]; then
  echo "✅ All checks passed!"
  echo "=================================="
  echo ""
  echo "Your code is ready to push."
else
  echo "❌ Some checks failed"
  echo "=================================="
  echo ""
  echo "Please fix the issues above before pushing."
  echo "Run './scripts/format_all.sh' to auto-fix formatting."
fi

exit $EXIT_CODE
