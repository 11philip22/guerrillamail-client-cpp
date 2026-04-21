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
- Bootstrap session initialization with homepage token extraction and mock-server coverage for bootstrap success, failure classification, and cookie continuity.
- Minimal internal AJAX request helpers for Rust-aligned `check_email` probe URL and header construction.
- Opt-in live GuerrillaMail validation coverage for bootstrap success, token extraction, AJAX header sufficiency, and cookie-backed session behavior.
- Integration test guidance and WOL-172 notes for rerunning and interpreting live protocol validation findings.
- Working `Client::create_email(...)` support using the bootstrapped session, shared authenticated AJAX request helpers, and response parsing for returned `email_addr` values.
- Focused unit and integration coverage for the `set_email_user` flow, including session reuse, alias handling, request-shape checks, and malformed-response classification.
- Optional `ClientOptions.site` override for the `create_email(...)` `site` form field when protocol compatibility requires a fixed logical site value.

### Changed
- Expanded `guerrillamail::Error` to carry optional HTTP status information for `http_status` failures.
- Replaced transport and parsing placeholders with working internal foundations for later bootstrap and API-request implementation.
- Expanded `ClientOptions` with bootstrap transport knobs for timeout, proxy, and TLS verification.
- Made `Client` move-only to avoid sharing one mutable transport session across copies.
- Derived AJAX probe `site` and origin metadata from the configured endpoint instead of hardcoding production-only request values.
- Aligned AJAX probe URL validation with header construction so malformed endpoint URLs fail early as `invalid_argument` errors.
- Kept `create_email(...)` default `site` behavior aligned with the configured AJAX host while allowing an explicit per-client override for compatibility cases.
- Documented that `ClientOptions.site` currently applies only to `create_email(...)` until later AJAX flows make an explicit decision about honoring it.

### Fixed
- Hardened internal libcurl setup and response-code handling by checking configuration and info-query return codes instead of silently ignoring failures.
- Made request-header list construction fail safely instead of risking dropped headers and leaked state on `curl_slist_append` failure.
- Validated transport timeout values before passing them to libcurl so negative and oversized values become clear `invalid_argument` errors.
- Removed a dangling-reference footgun from the internal JSON parsing helper surface.
- Added explicit POST transport coverage alongside additional transport-failure tests for connection, proxy, and TLS verification errors.
- Made request-header cleanup exception-safe in the transport layer so header lists are released even when setup throws before request execution.
- Corrected the documented `ctest` live-test command to match the Catch-discovered test name.
- Rejected empty explicit `ClientOptions.site` overrides as `invalid_argument` instead of silently falling back to the derived host value.
