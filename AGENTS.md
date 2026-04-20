# AGENTS.md

## Project

`guerrillamail-cpp` is a C++ API client for GuerrillaMail.

The intended implementation is a **C++ port of** `C:\Users\Philip\source\repos\guerillamail-rs`.

## Source Of Truth

When behavior is unclear, use these references in this order:

1. `C:\Users\Philip\source\repos\guerillamail-rs`
2. `C:\Users\Philip\source\repos\guerrillamail-client-c`
3. `C:\Users\Philip\source\repos\mega\mega-up\src\mail`

Interpretation:

- The Rust repo is the primary behavioral reference.
- The C client is the reference for practical blocking flows and lifecycle expectations.
- `mega-up` is a reference for a small, idiomatic C++ wrapper shape.

## Current Goal

Build the library in small, verifiable increments until it can:

- bootstrap a GuerrillaMail session
- create an email address
- list inbox messages
- fetch full message details
- delete / forget an address
- support attachment listing and download if parity with the Rust client is maintained

## Working Rules

- Port behavior, not Rust syntax.
- Keep public behavior aligned with `guerillamail-rs` unless there is a strong C++ reason not to.
- Prefer small, reviewable vertical slices over broad scaffolding.
- Do not add compatibility layers or speculative abstractions before they are needed.
- Keep dependencies minimal and justify each one.
- Preserve clear error reporting; transport, parse, and argument errors should stay distinguishable.

## API Expectations

- Favor an idiomatic RAII C++ surface over a C-style ownership model.
- Keep the first public API small and focused.
- Mirror the Rust client's core operations and naming closely enough that the mapping is obvious.
- Avoid introducing async behavior by default unless we explicitly decide to support it.

## Implementation Expectations

- Reuse the Rust repo to validate request flow, headers, token/bootstrap behavior, and response parsing.
- Keep parsing models close to the upstream JSON fields until there is a good reason to wrap or rename them.
- Prefer explicit types for messages, email details, attachments, and errors.
- Separate transport logic from response-model parsing when that improves testability without adding unnecessary layers.

## Validation

- Add tests alongside new behavior where practical.
- Prefer deterministic tests for parsing, URL/query construction, and error handling.
- Treat live GuerrillaMail calls as integration or manual validation, not the only proof of correctness.
- Keep at least one small example program that demonstrates the intended end-to-end flow.

## Decision Guardrails

Before locking in architecture, explicitly evaluate:

- C++ standard level
- build system
- HTTP library
- JSON library
- error model
- sync-only vs async-capable surface
- test strategy

Do not assume these choices are settled just because a first implementation exists.

## Initial Architecture Decisions

For the first implementation, the project is intentionally using:

- C++20
- CMake
- `vcpkg` from a git submodule at `third_party/vcpkg`
- `libcurl` for HTTP transport
- `nlohmann/json` for JSON parsing
- a sync-first API
- a small RAII public surface built around `Client` and `ClientOptions`
- typed library exceptions for error reporting
- Catch2 plus CTest for automated tests

Rationale:

- C++20 is the best portability / ergonomics balance for a new library.
- CMake is the most practical cross-platform build system for a reusable C++ library.
- `vcpkg` as a pinned submodule keeps dependency acquisition reproducible across machines and CI.
- `libcurl` best matches the required session, cookie, proxy, TLS, timeout, and download behavior.
- `nlohmann/json` keeps parsing straightforward while the API shape is still being validated.
- A synchronous surface is the smallest correct starting point and matches the current project goal.
- Exceptions keep the public API small while still allowing transport, parse, and argument failures to remain distinguishable through typed error codes.

## Dependency Management

Use `vcpkg` from a git submodule located at `third_party/vcpkg`.

Guidance:

- Keep the `vcpkg` submodule pinned to a known commit for reproducible builds.
- Use `vcpkg` to provide at least `libcurl`, `nlohmann-json`, and Catch2.
- Prefer standard CMake integration through the `vcpkg` toolchain file rather than custom dependency scripts.
- Keep third-party source dependencies out of the project tree unless there is a clear reason to vendor more than the package manager itself.

## Public API Shape

The first public API should stay small and centered on these types:

- `guerrillamail::Client`
- `guerrillamail::ClientOptions`
- `guerrillamail::Message`
- `guerrillamail::EmailDetails`
- `guerrillamail::Attachment`
- `guerrillamail::Error`

The first operation set should mirror the Rust client closely:

- `Client::create(...)`
- `create_email(...)`
- `get_messages(...)`
- `fetch_email(...)`
- `delete_email(...)`
- `list_attachments(...)`
- `fetch_attachment(...)`

API guidance:

- Keep names close to the Rust client unless there is a clear C++ reason not to.
- Prefer `ClientOptions` over a builder until configuration becomes large enough to justify one.
- Keep the client copyable via shared internal state only if that materially simplifies ownership without exposing transport details.
- Do not expose `libcurl` or JSON library types in public headers.

## Error Model

The public API should throw a typed `guerrillamail::Error` derived from `std::runtime_error`.

At minimum, keep these error categories distinct:

- `invalid_argument`
- `transport`
- `http_status`
- `token_parse`
- `response_parse`
- `json_parse`
- `internal`

## Internal Layout

Prefer a small internal layout that separates transport, protocol behavior, and public API glue:

- `include/guerrillamail/client.hpp`
- `include/guerrillamail/types.hpp`
- `include/guerrillamail/error.hpp`
- `src/client.cpp`
- `src/error.cpp`
- `src/transport/curl_session.hpp`
- `src/transport/curl_session.cpp`
- `src/protocol/bootstrap.hpp`
- `src/protocol/bootstrap.cpp`
- `src/protocol/requests.hpp`
- `src/protocol/requests.cpp`
- `src/protocol/parsing.hpp`
- `src/protocol/parsing.cpp`

Guidance:

- Keep transport concerns out of public headers.
- Keep request construction and response parsing testable without live network calls.
- Do not introduce generic service layers, transport interfaces, or speculative abstractions before they are needed.

## Testing Strategy

Use three layers of validation:

- unit tests for parsing, alias extraction, token extraction, query/form construction, and error classification
- local mock-server integration tests for bootstrap, cookies, create/list/fetch/delete flow, and attachment download flow
- manual live validation against GuerrillaMail for end-to-end confirmation

Keep at least one small example program that demonstrates the intended happy path.

## Implementation Sequence

Implement in small vertical slices:

1. project skeleton, dependencies, and test wiring
2. bootstrap flow and token extraction
3. create email
4. list inbox messages
5. fetch full email details
6. delete / forget address
7. attachment listing and download
8. example program and manual live validation

## Follow Rust Closely

Stay closely aligned with `guerillamail-rs` for:

- endpoint URLs
- bootstrap flow
- token extraction behavior
- cookie/session handling
- request parameters and headers
- alias extraction behavior
- JSON field mapping
- attachment download behavior
- core operation naming and sequencing

## Prefer C++ Adaptation

Intentionally adapt the Rust design where C++ benefits from a different surface:

- use a synchronous API first
- use RAII and value types
- use `ClientOptions` instead of a builder for now
- use typed exceptions instead of a Rust-style `Result<T, Error>` public API
- keep implementation details behind project-owned `.hpp` / `.cpp` boundaries

## Early Validation Risks

Validate these early before building too much around assumptions:

- homepage token extraction stability
- whether bootstrap cookies are required for later AJAX calls
- which browser-like headers are actually required for reliable behavior
- attachment and `sid_token` behavior
- Windows dependency and packaging expectations for `libcurl`

## Change Policy

- Update this file when project direction or core constraints change.
- If a proposed change would intentionally diverge from the Rust client, document the reason in code review notes or project docs.
