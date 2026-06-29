# Integration Tests

The integration suite contains opt-in live tests that validate current GuerrillaMail behavior against the real service.

## Live Tests

Live tests are not built by default. Enable the CMake option, rebuild, then set the runtime guard:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="third_party/vcpkg/scripts/buildsystems/vcpkg.cmake" -DGUERRILLAMAIL_CPP_BUILD_LIVE_TESTS=ON
cmake --build build --config Debug
$env:GUERRILLAMAIL_CPP_ENABLE_LIVE_TESTS = "1"
ctest -C Debug --output-on-failure --test-dir build --tests-regex "live"
```

What the live tests validate:

- bootstrap succeeds against `https://www.guerrillamail.com`
- current homepage HTML still exposes `api_token`
- the default AJAX header set is sufficient for a real `check_email` probe
- a follow-up probe without bootstrap cookies is compared and the result is surfaced in test output
- create/list/delete works through the public `Client`
- the local Windows `libcurl` runtime can complete a real HTTPS request in practice

## Attachment Flow Notes

- Attachment download uses the `/inbox` endpoint, not `ajax.php`.
- The download query intentionally omits `site`; `ClientOptions.site` does not affect attachment downloads.
- `sid_token` is included only when present and non-empty on the fetched email details.
- Live attachment validation remains a manual check because it depends on having a real message with an attachment available during the test run.
