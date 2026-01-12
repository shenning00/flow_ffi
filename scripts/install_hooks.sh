#!/bin/bash
# Install Git hooks for the project

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
HOOKS_DIR="$PROJECT_ROOT/.git/hooks"

echo "Installing Git hooks..."

# Check if .git directory exists
if [ ! -d "$PROJECT_ROOT/.git" ]; then
  echo "❌ Error: Not a git repository"
  exit 1
fi

# Create hooks directory if it doesn't exist
mkdir -p "$HOOKS_DIR"

# Install pre-commit hook
HOOK_SOURCE="$SCRIPT_DIR/pre-commit-hook.sh"
HOOK_TARGET="$HOOKS_DIR/pre-commit"

if [ -f "$HOOK_TARGET" ]; then
  echo "⚠️  Pre-commit hook already exists"
  read -p "Overwrite? (y/n) " -n 1 -r
  echo
  if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Skipping pre-commit hook installation"
    exit 0
  fi
fi

# Copy and make executable
cp "$HOOK_SOURCE" "$HOOK_TARGET"
chmod +x "$HOOK_TARGET"

echo "✅ Pre-commit hook installed successfully"
echo ""
echo "The hook will run automatically before each commit."
echo "To bypass the hook, use: git commit --no-verify"
echo ""
echo "To uninstall, run: rm $HOOK_TARGET"
