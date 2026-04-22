# Daemonized Lit Testing Demo

This branch is a demonstration of the [Lit daemonized testing project](https://discourse.llvm.org/t/rfc-reducing-process-creation-overhead-in-llvm-regression-tests/88612/9?u=bstott).
Commits marked "\[DEMO\]" are temporary workarounds for resetting existing
static state within LLVM, to verify that the daemonized testing is working
correctly. These should be fixed by refactoring the code to make sure the
state is reset properly.

For this demo, I have implemented the daemonization functionality into `opt`
and `FileCheck`, and gotten all of the `Transforms` tests working.
Daemonization is enabled by specifying the `LIT_USE_DAEMON_TOOLS` environment
variable before invoking Lit, for example:
```
$ LIT_USE_DAEMON_TOOLS=opt,FileCheck build/bin/llvm-lit llvm/test/Transforms -v
```
...to run the Transforms tests with both `opt` and `FileCheck` in daemon mode.

On my computer, I see a reduction in testing time for the Transforms tests
from ~160 seconds to ~50 seconds on Windows (with antivirus) and ~38 seconds
to ~30 seconds on Linux (WSL). These improvements are even more significant
if LLVM is built with shared libraries.

## Known remaining test failures (Transforms)
- `funcimport-cutoff.ll` - This is due to the static variable `ImportCount` in
  `ModuleImportsManager::computeImportForFunction` not being reset.
- Crashes in `SampleProfile` tests - This is due to the static members of
  `sampleprof::FunctionSamples` not being reset.
- There may be more failures, as the nature of tests failing due to persisting
  state is very flaky when the order of testing is non-deterministic.
