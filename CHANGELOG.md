# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial C++20 project scaffold with CMake-based library, example, and test targets.
- `vcpkg` dependency management via `third_party/vcpkg` submodule and `vcpkg.json` manifest.
- Public placeholder headers for `Client`, `ClientOptions`, `Message`, `EmailDetails`, `Attachment`, `Error`, and `ErrorCode`.
- Internal placeholder source layout for transport and protocol modules.
- Catch2 smoke tests and shared test-support directories for future unit and integration coverage.
- Basic example program linked only through the public library target.
- Internal `CurlSession` transport wrapper with request/response types, cookie persistence, request headers, timeout, proxy, and TLS verification support.
- Focused unit and loopback integration tests covering error classification, JSON syntax vs response-shape parsing, cookie persistence, and transport failure mapping.

### Changed
- Expanded `guerrillamail::Error` to carry optional HTTP status information for `http_status` failures.
- Replaced transport and parsing placeholders with working internal foundations for later bootstrap and API-request implementation.
