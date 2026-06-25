# Test Support

This directory is shared support for `guerrillamail-cpp` tests.

Conventions:

- `tests/unit/` holds deterministic unit and smoke tests.
- `tests/integration/` holds opt-in live tests.
- `tests/support/` holds shared helpers and reusable fixtures.

`WOL-166` only establishes the structure. Later passes should reuse this location instead of inventing new test support directories.
