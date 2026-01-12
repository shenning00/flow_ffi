#!/bin/bash
# Git pre-commit hook to check code formatting
# Install this hook by running: ./scripts/install_hooks.sh

set -e

echo "Running pre-commit checks..."

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

EXIT_CODE=0

# Get list of staged files
STAGED_CPP_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(cpp|h|hpp)$' | grep -vE '(build/|\.dart_tool/|CMakeFiles/)' || true)
STAGED_DART_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.dart$' || true)

# Check C++ files
if [ -n "$STAGED_CPP_FILES" ]; then
  echo "Checking C++ formatting..."
  if command -v clang-format &> /dev/null; then
    for file in $STAGED_CPP_FILES; do
      if [ -f "$file" ]; then
        if ! clang-format --dry-run --Werror "$file" 2>&1; then
          echo "❌ $file needs formatting"
          EXIT_CODE=1
        fi
      fi
    done
  else
    echo "⚠️  clang-format not installed, skipping C++ check"
  fi
fi

# Check Dart files
if [ -n "$STAGED_DART_FILES" ]; then
  echo "Checking Dart formatting..."
  if command -v dart &> /dev/null; then
    cd "$PROJECT_ROOT/dart_package"
    for file in $STAGED_DART_FILES; do
      # Convert to relative path from dart_package
      rel_file=$(echo "$file" | sed 's|dart_package/||')
      if [ -f "$rel_file" ]; then
        if ! dart format --set-exit-if-changed --output=none "$rel_file" 2>&1; then
          echo "❌ $file needs formatting"
          EXIT_CODE=1
        fi
      fi
    done
  else
    echo "⚠️  dart not installed, skipping Dart check"
  fi
fi

if [ $EXIT_CODE -ne 0 ]; then
  echo ""
  echo "❌ Pre-commit check failed!"
  echo "Fix formatting with: ./scripts/format_all.sh"
  echo ""
  echo "Or skip this check with: git commit --no-verify"
  exit 1
fi

echo "✅ Pre-commit checks passed"
exit 0
