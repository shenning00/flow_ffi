# Development Scripts

This directory contains helper scripts for development workflow.

## Available Scripts

### format_all.sh
Automatically formats all C++ and Dart code in the project.

```bash
./scripts/format_all.sh
```

**What it does:**
- Formats all `.cpp`, `.h`, `.hpp` files using clang-format
- Formats all `.dart` files using dart format
- Applies changes directly to files

**When to use:**
- Before committing changes
- After writing new code
- To fix formatting issues found by CI

### check_format.sh
Checks code formatting and runs static analysis without modifying files.

```bash
./scripts/check_format.sh
```

**What it does:**
- Checks C++ formatting (clang-format)
- Checks Dart formatting
- Runs Dart static analysis
- Reports issues without making changes

**When to use:**
- Before pushing to remote
- To verify code meets CI requirements
- To check if formatting is needed

**Exit codes:**
- `0`: All checks passed
- `1`: Some checks failed

### install_hooks.sh
Installs Git pre-commit hooks to automatically check formatting.

```bash
./scripts/install_hooks.sh
```

**What it does:**
- Installs pre-commit hook that checks formatting
- Hook runs automatically before each commit
- Prevents commits with formatting issues

**When to use:**
- Once after cloning the repository
- To enable automatic formatting checks

### pre-commit-hook.sh
The actual pre-commit hook script (called by Git automatically).

**What it does:**
- Checks formatting of staged files only
- Runs before `git commit` completes
- Blocks commit if formatting issues found

**Note:** This script is called by Git, not directly by developers.

### test_ffigen_ci.sh
Tests FFI bindings generation in a CI-like environment.

```bash
./scripts/test_ffigen_ci.sh
```

**What it does:**
- Checks for libclang installation (required for ffigen)
- Installs Dart dependencies
- Generates FFI bindings from C headers
- Verifies bindings were created successfully
- Compiles Dart code to check for errors
- Runs analyzer on generated bindings

**When to use:**
- To test local ffigen setup before pushing to CI
- To verify libclang is correctly installed
- To debug FFI bindings generation issues
- To simulate the dart-build CI job locally

**Requirements:**
- libclang-dev (Ubuntu/Debian) or llvm (macOS)
- Dart SDK installed

## Quick Start

1. **Initial setup:**
   ```bash
   ./scripts/install_hooks.sh
   ```

2. **Before committing:**
   ```bash
   # Format your code
   ./scripts/format_all.sh

   # Verify everything is good
   ./scripts/check_format.sh

   # Commit (hook will run automatically)
   git add .
   git commit -m "Your commit message"
   ```

3. **Bypass hook (if needed):**
   ```bash
   git commit --no-verify
   ```

## Requirements

### For C++ Scripts:
- `clang-format` (version 10+)
  - Ubuntu/Debian: `sudo apt-get install clang-format`
  - macOS: `brew install clang-format`

### For Dart Scripts:
- Dart SDK (3.0+)
  - Download from: https://dart.dev/get-dart
  - Or install Flutter which includes Dart

## Troubleshooting

### "clang-format: command not found"
Install clang-format for your system (see Requirements above).

### "dart: command not found"
Install Dart SDK or add it to your PATH.

### Pre-commit hook not running
Ensure the hook is executable:
```bash
chmod +x .git/hooks/pre-commit
```

### Hook fails but you need to commit urgently
Use `--no-verify` to bypass the hook:
```bash
git commit --no-verify -m "Urgent fix"
```
**Note:** Fix formatting in a follow-up commit.

## CI/CD Integration

These scripts mirror the checks performed in GitLab CI:
- `check_format.sh` ≈ format-check stage
- Running these locally ensures CI will pass

## Customization

All scripts are bash scripts that can be modified:
- Located in: `/home/shenning/Development/flow_ffi/scripts/`
- Well-commented for easy understanding
- Can be adapted to project needs

## Support

For issues with these scripts:
1. Check script output for specific errors
2. Ensure all requirements are installed
3. Review CI_README.md for additional context
4. Contact the development team
