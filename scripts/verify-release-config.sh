#!/bin/bash
# Verification script for release job configuration
# This script checks that all prerequisites for creating a release are met

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DART_PACKAGE_DIR="$PROJECT_ROOT/dart_package"

echo "================================================"
echo "Flow FFI Release Configuration Verification"
echo "================================================"
echo ""

# Check 1: Verify pubspec.yaml exists and has version
echo "Check 1: Verifying pubspec.yaml..."
if [ ! -f "$DART_PACKAGE_DIR/pubspec.yaml" ]; then
    echo "❌ ERROR: pubspec.yaml not found"
    exit 1
fi

PUBSPEC_VERSION=$(grep '^version:' "$DART_PACKAGE_DIR/pubspec.yaml" | sed 's/version: *//' | tr -d ' ')
if [ -z "$PUBSPEC_VERSION" ]; then
    echo "❌ ERROR: No version found in pubspec.yaml"
    exit 1
fi

echo "   Current version: $PUBSPEC_VERSION"
echo "✅ pubspec.yaml found and has version"
echo ""

# Check 2: Verify CHANGELOG.md exists
echo "Check 2: Verifying CHANGELOG.md..."
if [ ! -f "$DART_PACKAGE_DIR/CHANGELOG.md" ]; then
    echo "❌ ERROR: CHANGELOG.md not found"
    exit 1
fi

if grep -q "\[$PUBSPEC_VERSION\]" "$DART_PACKAGE_DIR/CHANGELOG.md"; then
    echo "   CHANGELOG has entry for version $PUBSPEC_VERSION"
else
    echo "⚠️  WARNING: CHANGELOG.md does not contain entry for version $PUBSPEC_VERSION"
    echo "   Please add release notes before creating a tag"
fi
echo "✅ CHANGELOG.md found"
echo ""

# Check 3: Verify GitLab CI configuration
echo "Check 3: Verifying .gitlab-ci.yml..."
if [ ! -f "$PROJECT_ROOT/.gitlab-ci.yml" ]; then
    echo "❌ ERROR: .gitlab-ci.yml not found"
    exit 1
fi

if grep -q "publish-dart-package:" "$PROJECT_ROOT/.gitlab-ci.yml"; then
    echo "✅ publish-dart-package job found in .gitlab-ci.yml"
else
    echo "❌ ERROR: publish-dart-package job not found in .gitlab-ci.yml"
    exit 1
fi

if grep -q "stage: release" "$PROJECT_ROOT/.gitlab-ci.yml"; then
    echo "✅ release stage found in .gitlab-ci.yml"
else
    echo "❌ ERROR: release stage not found in .gitlab-ci.yml"
    exit 1
fi
echo ""

# Check 4: Verify YAML syntax
echo "Check 4: Validating YAML syntax..."
if command -v python3 >/dev/null 2>&1; then
    if python3 -c "import yaml; yaml.safe_load(open('$PROJECT_ROOT/.gitlab-ci.yml'))" 2>/dev/null; then
        echo "✅ YAML syntax is valid"
    else
        echo "❌ ERROR: YAML syntax error in .gitlab-ci.yml"
        exit 1
    fi
else
    echo "⚠️  WARNING: python3 not found, skipping YAML validation"
fi
echo ""

# Check 5: Verify git is initialized
echo "Check 5: Verifying git repository..."
if [ ! -d "$PROJECT_ROOT/.git" ]; then
    echo "❌ ERROR: Not a git repository"
    exit 1
fi

# Check if on main/master branch
CURRENT_BRANCH=$(git -C "$PROJECT_ROOT" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
echo "   Current branch: $CURRENT_BRANCH"

# Check for uncommitted changes
if git -C "$PROJECT_ROOT" diff-index --quiet HEAD -- 2>/dev/null; then
    echo "✅ No uncommitted changes"
else
    echo "⚠️  WARNING: You have uncommitted changes"
    echo "   Commit changes before creating a release tag"
fi
echo ""

# Check 6: Verify recommended tag format
echo "Check 6: Verifying tag format..."
RECOMMENDED_TAG="v$PUBSPEC_VERSION"
echo "   Recommended tag: $RECOMMENDED_TAG"

# Check if tag already exists
if git -C "$PROJECT_ROOT" rev-parse "$RECOMMENDED_TAG" >/dev/null 2>&1; then
    echo "⚠️  WARNING: Tag $RECOMMENDED_TAG already exists"
    echo "   To create a new release, update the version in pubspec.yaml"
else
    echo "✅ Tag $RECOMMENDED_TAG is available"
fi
echo ""

# Check 7: List existing tags
echo "Check 7: Listing existing tags..."
EXISTING_TAGS=$(git -C "$PROJECT_ROOT" tag -l "v*" | tail -5)
if [ -n "$EXISTING_TAGS" ]; then
    echo "   Recent tags:"
    echo "$EXISTING_TAGS" | sed 's/^/   - /'
else
    echo "   No version tags found"
fi
echo ""

# Summary
echo "================================================"
echo "Verification Summary"
echo "================================================"
echo ""
echo "Current Version: $PUBSPEC_VERSION"
echo "Recommended Tag: $RECOMMENDED_TAG"
echo ""
echo "To create a release:"
echo "  1. Ensure all changes are committed"
echo "  2. Update CHANGELOG.md with release notes for $PUBSPEC_VERSION"
echo "  3. Run: git tag $RECOMMENDED_TAG"
echo "  4. Run: git push origin main"
echo "  5. Run: git push origin $RECOMMENDED_TAG"
echo "  6. Go to GitLab CI/CD and trigger publish-dart-package job"
echo ""
echo "For detailed instructions, see:"
echo "  $PROJECT_ROOT/RELEASE.md"
echo "  $PROJECT_ROOT/.gitlab/RELEASE_QUICK_START.md"
echo ""
echo "✅ All checks passed!"
