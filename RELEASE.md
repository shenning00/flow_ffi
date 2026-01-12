# Flow FFI Package Release Guide

This document describes how to create and distribute releases of the flow_ffi Dart package using the GitLab CI/CD pipeline.

## Overview

The flow_ffi package is a **PRIVATE** package that is NOT published to pub.dev. Instead, it is distributed as a tarball artifact through GitLab CI/CD that can be downloaded and used as a local dependency in other projects.

## Release Process

### Step 1: Prepare the Release

Before creating a release, ensure you have:

1. **Updated the version** in `dart_package/pubspec.yaml`
   ```yaml
   version: 1.0.1
   ```

2. **Updated the CHANGELOG** in `dart_package/CHANGELOG.md`
   ```markdown
   ## [1.0.1] - 2025-11-15

   ### Fixed
   - Bug fix description

   ### Added
   - New feature description
   ```

3. **Tested your changes** locally
   - Run tests: `cd dart_package && dart test`
   - Verify FFI bindings: `dart run ffigen --config ffigen.yaml`
   - Check formatting: `dart format --set-exit-if-changed .`

### Step 2: Create and Push a Git Tag

The release process is triggered by creating a git tag that follows semantic versioning:

```bash
# Update version in pubspec.yaml (e.g., 1.0.1)
# Update CHANGELOG.md

# Commit your changes
git add dart_package/pubspec.yaml dart_package/CHANGELOG.md
git commit -m "Release v1.0.1"

# Create a semantic version tag (with 'v' prefix)
git tag v1.0.1

# Push commits and tag to GitLab
git push origin main
git push origin v1.0.1
```

**Version Tag Format:**
- Standard releases: `v1.0.0`, `v1.0.1`, `v2.0.0`
- Pre-releases: `v2.0.0-alpha.1`, `v2.0.0-beta.1`, `v2.0.0-rc.1`

**IMPORTANT:** The git tag version (without the 'v' prefix) MUST match the version in `pubspec.yaml`:
- Git tag: `v1.0.1`
- Pubspec: `version: 1.0.1`

The CI job will verify this and fail if there's a mismatch.

### Step 3: Trigger the Release Job

1. Go to your GitLab project's **CI/CD > Pipelines**
2. Find the pipeline for your tag (e.g., `v1.0.1`)
3. The `publish-dart-package` job in the **release** stage will be waiting for manual approval
4. Click on the job and press **Play** to trigger it

The job will:
- Verify the version in `pubspec.yaml` matches the git tag
- Download build artifacts from `dart-build` and `cpp-build` jobs
- Validate the package structure with `dart pub publish --dry-run`
- Bundle native libraries into the package
- Create a compressed tarball: `flow_ffi-v1.0.1.tar.gz`
- Generate a manifest file with installation instructions
- Store artifacts for 90 days

### Step 4: Download the Release Artifact

Once the job completes successfully:

1. Navigate to the completed job page
2. Click on **Browse** under the artifacts section
3. Download:
   - `flow_ffi-v1.0.1.tar.gz` - The complete package archive
   - `flow_ffi-v1.0.1.manifest.txt` - Installation instructions and metadata

Alternatively, use the GitLab API or download button to get the artifacts.

## Using the Released Package

### Installation

1. **Download and extract** the release tarball:
   ```bash
   wget https://gitlab.example.com/your-project/flow_ffi/-/jobs/123456/artifacts/download -O flow_ffi-v1.0.1.tar.gz
   tar xzf flow_ffi-v1.0.1.tar.gz
   ```

2. **Add to your project's `pubspec.yaml`** as a path dependency:
   ```yaml
   dependencies:
     flow_ffi:
       path: /absolute/path/to/extracted/dart_package
   ```

3. **Install the package**:
   ```bash
   dart pub get
   ```

4. **Import in your Dart code**:
   ```dart
   import 'package:flow_ffi/flow_ffi.dart';
   ```

### Package Contents

Each release tarball includes:
- Complete Dart package source code
- Generated FFI bindings (`lib/src/ffi/bindings_generated.dart`)
- Compiled native libraries (`lib/native/linux/libflow_ffi.so`, `libflow-core.so`)
- Locked dependencies (`pubspec.lock`) with exact versions used during build
- Documentation and examples
- README and CHANGELOG

### Platform Compatibility

The CI pipeline currently builds native libraries for:
- **Linux x86_64** (libflow_ffi.so, libflow-core.so)

For other platforms (Windows, macOS, Android, iOS), you will need to:
1. Build the C++ library separately for that platform
2. Place the compiled libraries in `lib/native/<platform>/`
3. Configure your Dart code to load the correct library for each platform

## CI/CD Pipeline Details

### Job Configuration

The `publish-dart-package` job:
- **Stage:** release
- **Trigger:** Only on git tags (manual approval required)
- **Dependencies:**
  - `dart-build` - Provides FFI bindings and pubspec.lock
  - `cpp-build` - Provides compiled native libraries
- **Image:** `dart:stable`
- **Artifacts:**
  - Tarball and manifest (kept for 90 days)
  - Named: `flow_ffi-dart-package-${CI_COMMIT_TAG}`

### Version Verification

The job performs strict version checking:
```bash
Git tag:        v1.0.1
Tag version:    1.0.1  (v prefix removed)
Pubspec version: 1.0.1

✅ Version verification passed
```

If versions don't match, the job fails with a clear error message.

### Validation Steps

The job runs `dart pub publish --dry-run` to validate:
- Package structure is correct
- All required fields are present in pubspec.yaml
- No banned or problematic dependencies
- License and description are valid

Note: Some warnings about private packages are expected since we're not actually publishing to pub.dev.

### CHANGELOG Verification

The job checks if `CHANGELOG.md` contains an entry for the release version:
```markdown
## [1.0.1] - 2025-11-15
```

If not found, it shows a warning but continues (non-blocking).

## Troubleshooting

### Version Mismatch Error

**Error:**
```
❌ ERROR: Version mismatch!
   pubspec.yaml version: 1.0.0
   Git tag version: 1.0.1
```

**Solution:** Update `dart_package/pubspec.yaml` to match the git tag:
```bash
# Update pubspec.yaml version to 1.0.1
git add dart_package/pubspec.yaml
git commit --amend --no-edit
git tag -d v1.0.1
git tag v1.0.1
git push --force origin main
git push --force origin v1.0.1
```

### FFI Bindings Not Found

**Error:**
```
❌ ERROR: FFI bindings not found!
```

**Solution:** Ensure the `dart-build` job completed successfully. Check that `lib/src/ffi/bindings_generated.dart` was created.

### Native Library Not Found

**Error:**
```
❌ ERROR: Native library libflow_ffi.so not found!
```

**Solution:** Ensure the `cpp-build` job completed successfully. Check that `build/libflow_ffi.so` exists in the artifacts.

### Package Validation Warnings

**Warning:**
```
⚠️  Package validation warnings detected
```

**Solution:** This is expected for private packages. Review warnings with:
```bash
cd dart_package
dart pub publish --dry-run
```

Common warnings for private packages are acceptable. Critical errors will fail the job.

## Advanced Usage

### Automatic Publishing on Tags

To automatically publish releases without manual approval, change the job configuration:

```yaml
publish-dart-package:
  # ... other config ...
  when: on_success  # Changed from 'when: manual'
```

### Custom Artifact Expiration

To change how long artifacts are kept:

```yaml
artifacts:
  expire_in: 180 days  # Changed from 90 days
```

### Multi-Platform Releases

To include libraries for multiple platforms, modify the script section:

```yaml
script:
  - mkdir -p lib/native/linux lib/native/windows lib/native/macos
  - cp ../build/linux/libflow_ffi.so lib/native/linux/
  - cp ../build/windows/flow_ffi.dll lib/native/windows/
  - cp ../build/macos/libflow_ffi.dylib lib/native/macos/
```

## Best Practices

1. **Always update CHANGELOG.md** before creating a release
2. **Test thoroughly** on the target platform before tagging
3. **Use semantic versioning** consistently
4. **Keep release notes** descriptive and user-focused
5. **Verify artifacts** after download to ensure completeness
6. **Document breaking changes** clearly in CHANGELOG
7. **Tag from main branch** to ensure stability

## Security Considerations

- Releases are stored as GitLab CI artifacts (private by default)
- Only users with project access can download releases
- Artifacts expire after 90 days (configurable)
- No credentials or secrets are included in the tarball
- Native libraries are built in CI with known, verified dependencies

## Support

For issues with the release process:
1. Check the CI job logs for detailed error messages
2. Verify all prerequisites are met (version match, CHANGELOG, tests pass)
3. Review this documentation for troubleshooting steps
4. Contact the maintainers if problems persist

## References

- [Semantic Versioning](https://semver.org/)
- [Keep a Changelog](https://keepachangelog.com/)
- [Dart Package Layout](https://dart.dev/tools/pub/package-layout)
- [GitLab CI/CD Artifacts](https://docs.gitlab.com/ee/ci/pipelines/job_artifacts.html)
