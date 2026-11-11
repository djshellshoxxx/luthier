# Native lipo argument-order fix

The macOS validator and beta packaging checks now put the executable path before
`-verify_arch`. Apple cctools' parser first records unflagged arguments as input
files, then consumes every remaining argument after `-verify_arch` as an
architecture name. The checked source is:

- `https://github.com/apple-oss-distributions/cctools/blob/e0d56624eca2a76c2ace4c21850df9e666de4ca5/misc/lipo.c#L431-L470`
  (the `-verify_arch` parser and the positional input-file branch)
- `https://github.com/apple-oss-distributions/cctools/blob/e0d56624eca2a76c2ace4c21850df9e666de4ca5/misc/lipo.c#L2701-L2716`
  (usage synopsis: `lipo <input_file> <command> [<options> ...]`)

The focused shell regression uses a strict fake `lipo` command that requires an
existing executable file, followed by `-verify_arch`, followed by exactly one
native architecture. Its existing missing-architecture negative case remains.

## RED before changing the two shell calls

Command:

```text
python3 -m unittest scripts.test_native_diagnostics.DiagnosticTests.test_diagnostic_validator_requires_both_and_never_calls_au
```

Exit code: `1`.

```text
F
======================================================================
FAIL: test_diagnostic_validator_requires_both_and_never_calls_au (scripts.test_native_diagnostics.DiagnosticTests.test_diagnostic_validator_requires_both_and_never_calls_au)
----------------------------------------------------------------------
Traceback (most recent call last):
  File "/workspace/luthier/scripts/test_native_diagnostics.py", line 90, in test_diagnostic_validator_requires_both_and_never_calls_au
    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
AssertionError: 1 != 0 : Native macOS failure evidence skipped: this host is not macOS.
fixture lipo rejected argv: expected executable -verify_arch arm64|x86_64; got: -verify_arch arm64 /tmp/tmpz4wlt2rh/tools/pluginval.app/Contents/MacOS/pluginval


----------------------------------------------------------------------
Ran 1 test in 0.041s

FAILED (failures=1)
```

## GREEN after changing the two shell calls

Command:

```text
python3 -m unittest scripts.test_native_diagnostics.DiagnosticTests.test_diagnostic_validator_requires_both_and_never_calls_au
```

Exit code: `0`.

```text
.
----------------------------------------------------------------------
Ran 1 test in 0.246s

OK
```
