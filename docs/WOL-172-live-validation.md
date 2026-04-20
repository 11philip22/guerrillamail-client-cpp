# WOL-172 Live Protocol Validation

## Purpose

This note records the early live-validation pass for bootstrap and the first post-bootstrap AJAX probe against the real GuerrillaMail service.

It exists to keep later passes, especially `WOL-165`, aligned with what the service actually accepts rather than what mock-server assumptions happen to allow.

## Validation Flow

1. Bootstrap with a real GET to `https://www.guerrillamail.com`.
2. Extract `api_token` from the returned HTML.
3. Issue a `check_email` GET to `https://www.guerrillamail.com/ajax.php` using:
   - `Authorization: ApiToken ...`
   - browser-like AJAX headers derived from the Rust client
   - the bootstrapped session cookies
4. Repeat the same probe from a fresh session without bootstrap cookies.

## Current Request Shape

The validation probe intentionally mirrors the Rust client for GET-based AJAX calls:

- query: `f=check_email&seq=1&site=guerrillamail.com&in=<alias>&_=<timestamp>`
- headers:
  - `Host`
  - `User-Agent`
  - `Accept: application/json, text/javascript, */*; q=0.01`
  - `Accept-Language: en-US,en;q=0.5`
  - `Authorization: ApiToken <token>`
  - `X-Requested-With: XMLHttpRequest`
  - `Origin`
  - `Referer`
  - `Sec-Fetch-Dest: empty`
  - `Sec-Fetch-Mode: cors`
  - `Sec-Fetch-Site: same-origin`
  - `Priority: u=0`

## Findings

Run the opt-in live test to refresh these observations:

```powershell
$env:GUERRILLAMAIL_CPP_ENABLE_LIVE_TESTS = "1"
ctest --output-on-failure --tests-regex live_protocol_validation_test
```

Observed on the current Windows development environment:

- Bootstrap succeeds against the live service.
- The homepage still exposes `api_token` in a form that the current extractor can parse.
- The default Rust-aligned AJAX header set is sufficient for a live `check_email` probe when the request reuses the bootstrapped curl session.
- Reusing the `ApiToken` header without the bootstrap cookie jar does not produce the same inbox-shaped response. The live response was:

```json
{"error":"Please call get_email_address or set_email_user first","stats":{"sequence_mail":"86,123,788","created_addresses":38163242,"received_emails":"20,809,800,942","total":"20,723,677,154","total_per_hour":"110298"},"auth":{"success":true,"error_codes":[]}}
```

- That response indicates the token is accepted (`auth.success: true`), but session-backed mailbox state is still required to get the normal `check_email` `list` payload.
- A successful live test run on Windows also confirms that the local `vcpkg`-provided `libcurl` runtime can complete a real HTTPS request in practice from the current development environment.

## Downstream Implications

- `WOL-165` should reuse the same derived `Host` / `Origin` / `Referer` and `Authorization: ApiToken ...` formatting for shared AJAX request construction.
- Later AJAX calls must preserve the cookie-backed session created during bootstrap. Carrying only the `ApiToken` header is not enough to reproduce the normal mailbox response shape.
- Keep live validation opt-in only. It is a manual/integration proof point, not a default CI dependency.
- If the fresh-session result changes, update this document before broadening mailbox feature work.
