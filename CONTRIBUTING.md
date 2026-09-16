# Contributing

Target Windows 10/11 x64. Follow README build instructions. Use a branch and pull request; never rewrite upstream history. Retain DDNet coding conventions, licenses and native graphics/UI abstractions. Do not modify physics or network state for visuals. No gameplay automation, telemetry or unrelated refactors.

For Debug, configure a separate `build-debug` directory with `-DCMAKE_BUILD_TYPE=Debug -DCMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=.` and use `--config Debug` in build/test commands. MSVC Debug runs C++ tests; upstream skips Rust tests there because its Rust runtime uses the release CRT.

Run `cmake --build build --config Release --target run_tests`. Prism tests use the actual DDNet console serialization and range validation. `PrismConfig.*` selects Prism cases with `build/testrunner.exe --gtest_filter=PrismConfig.*`.

Package with `package_default`, then `python scripts/package_prism.py build`. The script reopens the archive, checks resources, records source hashes and commit SHA, preserves notices, and checks PE imports on Windows. `scripts/smoke_prism.py` exercises the packaged client/config using an isolated save directory. Build completion does not prove visual correctness; use the matrix in VALIDATION.md.

Format changed C++ with the upstream `.clang-format`. Keep render loops allocation-free. Include actual validation evidence and untested areas in every PR.
