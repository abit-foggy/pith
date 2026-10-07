# Security Policy

## Supported Versions

Security updates are applied to the active release stream and rolling nightly builds.

| Version | Supported |
| ------- | --------- |
| 0.2.x   | Yes       |
| nightly | Yes       |
| < 0.2.0 | No        |

## Reporting a Vulnerability

The Pith project takes security vulnerabilities seriously. If you discover a vulnerability or security issue, please do not file a public issue on GitHub.

Please report security vulnerabilities through GitHub Private Vulnerability Reporting:
Navigate to the Security tab of the repository on GitHub, select "Report a vulnerability", and provide the details.

### What to Include in Your Report

To help us triage and resolve the issue quickly, please include:
- A clear description of the vulnerability and its potential impact.
- Affected component: compiler frontend (`src/`), code generator (`src/gen_qbe.c`), runtime memory / ARC (`runtime/memory.c`), package manager / archive extraction (`src/tar.c`), or native C FFI boundary (`include/pith.h`).
- Step-by-step reproduction instructions, including sample Pith (`.pi`) or C code.
- Target platform and architecture (e.g. Linux x86_64, macOS aarch64, Windows NT).
- Any proposed mitigations or proof-of-concept scripts.

## Response and Disclosure Process

1. **Acknowledgment**: We aim to acknowledge receipt of security reports within 48 hours.
2. **Investigation & Triage**: We will confirm the vulnerability, determine its severity, and provide regular progress updates.
3. **Patch Development**: Fixes are developed in private branches and tested against our cross-platform test matrix.
4. **Coordinated Disclosure**: Once a fix is verified and ready for release, we will coordinate public disclosure and publish a security advisory with credit to the reporter.

## Security Architecture & Invariants

Pith is designed with several defensive security principles in mind:
- **Archive Path Traversal**: Tar archive extraction rejects paths containing directory traversal (`..`), absolute roots, or paths escaping the extraction destination.
- **Reference Count Safety**: The Automated Reference Counting (ARC) runtime uses atomic operations on multithreaded targets and provides cycle breaking APIs to prevent memory leaks and dangling pointers.
- **FFI Boundary**: Native C imports follow explicit ownership and borrow semantics documented in `<pith.h>`.

## Authorship & Review

The majority of the code in this repository was written by an AI. All
architectural design was made by a human, and every change was
reviewed by both a human and an AI for flaws before it landed.
Security-sensitive components (parsing, code generation, archive
extraction, the FFI boundary, and subprocess handling) receive
additional review scrutiny.
