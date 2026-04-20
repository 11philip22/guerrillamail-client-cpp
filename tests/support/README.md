# Test Support

This directory is shared support for `guerrillamail-cpp` tests.

Conventions:

- `tests/unit/` holds deterministic unit and smoke tests.
- `tests/integration/` is reserved for local mock-server integration tests.
- `tests/support/` holds shared helpers, reusable fixtures, and future mock-server utilities.
- `tests/support/mock_server/` is reserved for future mock-server assets and helpers.

`WOL-166` only establishes the structure. Later passes should reuse this location instead of inventing new test support directories.
