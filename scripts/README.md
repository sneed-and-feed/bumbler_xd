# 📦 Bumbler XD Release & Packaging Scripts

This directory contains automation utilities for packaging, archiving, and generating cryptographic checksums for **Bumbler XD** distribution artifacts.

---

## Overview

| Script | Language | Dependencies | Primary Output |
|---|---|---|---|
| [`package_release.py`](package_release.py) | Python 3.8+ | Standard Library (`os`, `zipfile`, `hashlib`) | `releases/*.zip`, `releases/SHA256SUMS.txt` |

---

## `package_release.py`

### 1. Purpose

The [`package_release.py`](package_release.py) script automates the creation of release packages from compiled build artifacts produced by CMake and JUCE 8. It packages standalone executables and VST3 plugin bundles, includes necessary project documentation and licensing, and computes cryptographically secure SHA-256 digests for all generated distribution archives.

### 2. Prerequisites & Inputs

Before executing the packaging script, ensure the project has been built in **Release** mode:

```bash
# Build standalone and VST3 targets in Release mode
cmake --build build --config Release --parallel
```

The script expects compiled artifacts in the JUCE/CMake build directory:
- **Standalone Executable**: `build/BumblerXD_artefacts/Release/Standalone/Bumbler XD.exe`
- **VST3 Plugin Bundle**: `build/BumblerXD_artefacts/Release/VST3/Bumbler XD.vst3/`
- **Project Root Documents**: `README.md`, `LICENSE`

### 3. Usage

Run the script from the project root or from the `scripts/` directory:

```bash
# From repository root
python scripts/package_release.py
```

### 4. Output Directories & Artifacts

All packaged files are staged into the `releases/` folder at the repository root:

```text
releases/
├── BUMBLER_XD-v1.0.3-Windows-x64.zip       # Full distribution package (Standalone + VST3 + Docs)
├── BUMBLER_XD-v1.0.3-VST3-Windows-x64.zip  # VST3-only distribution package
└── SHA256SUMS.txt                          # SHA-256 cryptographic checksums manifest
```

### 5. Zip Archive Internal Layouts

#### A. Full Package (`BUMBLER_XD-v<version>-Windows-x64.zip`)
Designed for general end users and DAW producers requiring both the standalone synthesizer application and the VST3 plugin:

```text
BUMBLER_XD-v1.0.3-Windows-x64.zip
├── Bumbler XD.exe                     # Standalone synthesizer executable
├── Bumbler XD.vst3/                   # VST3 plugin bundle directory hierarchy
│   └── Contents/
│       ├── x86_64-win/
│       │   └── Bumbler XD.vst3       # 64-bit VST3 binary module
│       └── Resources/                 # Plugin resources and module info
├── README.md                          # Full user guide, feature specs, and architecture overview
└── LICENSE                            # MIT License file
```

#### B. VST3-Only Package (`BUMBLER_XD-v<version>-VST3-Windows-x64.zip`)
Tailored for automated DAW plugin installers, package managers, and producers who exclusively use VST3 hosts:

```text
BUMBLER_XD-v1.0.3-VST3-Windows-x64.zip
├── Bumbler XD.vst3/                   # VST3 plugin bundle directory hierarchy
│   └── Contents/
│       ├── x86_64-win/
│       │   └── Bumbler XD.vst3       # 64-bit VST3 binary module
│       └── Resources/                 # Plugin resources and module info
├── README.md                          # Full documentation
└── LICENSE                            # MIT License
```

### 6. Cryptographic Manifest (`SHA256SUMS.txt`)

Following archive creation, the script computes standard SHA-256 hashes for all `.zip` files in `releases/` starting with `BUMBLER_XD` and outputs them in standard GNU `sha256sum` format:

```text
<sha256_hash>  BUMBLER_XD-v1.0.3-VST3-Windows-x64.zip
<sha256_hash>  BUMBLER_XD-v1.0.3-Windows-x64.zip
```

#### Integrity Verification Commands

Consumers can verify package integrity and authenticity using native command-line utilities:

- **Windows (PowerShell)**:
  ```powershell
  Get-FileHash -Algorithm SHA256 .\releases\*.zip
  ```

- **Linux**:
  ```bash
  cd releases && sha256sum -c SHA256SUMS.txt
  ```

- **macOS**:
  ```bash
  cd releases && shasum -a 256 -c SHA256SUMS.txt
  ```

---

## CI/CD Pipeline Integration

The release packaging process is also executed automatically during tagged GitHub release builds defined in [`.github/workflows/release-builds.yml`](../.github/workflows/release-builds.yml). Releases published to GitHub Releases include these identical zip archives and SHA-256 digests.
