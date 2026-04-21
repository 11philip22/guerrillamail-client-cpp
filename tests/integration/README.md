# Integration Tests

The integration suite has two layers:

1. Mock-server tests that run by default and validate deterministic request/response behavior.
2. Opt-in live tests that validate current GuerrillaMail behavior against the real service.

## Live Tests

`live_protocol_validation_test.cpp` is intentionally skipped unless you opt in with:

```powershell
$env:GUERRILLAMAIL_CPP_ENABLE_LIVE_TESTS = "1"
ctest --output-on-failure --tests-regex "live bootstrap and ajax probe validate current GuerrillaMail behavior"
```

What the live test validates:

- bootstrap succeeds against `https://www.guerrillamail.com`
- current homepage HTML still exposes `api_token`
- the default AJAX header set is sufficient for a real `check_email` probe
- a follow-up probe without bootstrap cookies is compared and the result is surfaced in test output
- the local Windows `libcurl` runtime can complete a real HTTPS request in practice

The current findings for `WOL-172` live in `docs/WOL-172-live-validation.md`.

## Attachment Flow Notes

- Attachment download uses the `/inbox` endpoint, not `ajax.php`.
- The download query intentionally omits `site`; `ClientOptions.site` does not affect attachment downloads.
- `sid_token` is included only when present and non-empty on the fetched email details.
- Live attachment validation remains a manual check because it depends on having a real message with an attachment available during the test run.

## Public End-To-End Test

`public_api_end_to_end_test.cpp` is a mock-server happy-path test that uses only public library headers and the public CMake target while exercising the first-version public API end to end.
