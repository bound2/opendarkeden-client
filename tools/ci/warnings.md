# Compiler warning budgets

`project_warnings` supplies `/W3 /w14311 /w14312 /w14668` on MSVC and
`-Wall -Wextra -Wundef` on GCC/Clang to every compiled target (forwarded with
`/clang:` when using clang-cl). It does not
enable `-Werror`. The `warning_policy` CTest checks the generated transitive
options for all targets in both C and C++ contexts; `warning_budget_parser`
exercises the log parser and its rejection paths.

The verification scripts use `--clean-first`, then compare the full build log
with `warning-baselines.json`. Each preset/architecture has its own budget.
`warnings.json` in the uploaded diagnostics records both emitted warning lines
and distinct diagnostics grouped by compiler warning ID. Repeated includes and
MSBuild summaries count once per source coordinate and ID; linker/driver messages
without a coordinate are distinguished by message. Untagged Unix linker warnings
are grouped as `LD`; source warnings without an ID are `UNTAGGED`, distinguished
by coordinate and message. A count is a debt metric,
not a promise that every individual warning is unchanged: replacing one warning
with another of the same ID can leave the count unchanged.

An increased count fails. A decrease also fails until the baseline is tightened
in that change. An unrecognised warning syntax, failed/no-compilation log, or absent
profile fails instead of silently returning zero. The parser cannot establish
from diagnostic text that every translation unit was compiled; the verification
script's clean build is the owner of that requirement.

After fixing warnings, build the complete preset and record its measured count:

```sh
perl tools/ci/check-warnings.pl \
  --log build/verification/macos/build.log --profile macos-arm64 \
  --baseline tools/ci/warning-baselines.json \
  --report build/verification/macos/warnings.json --record
```

Use the actual log and profile for the run: Unix profiles append `uname -m` to
the preset (`macos-arm64`, `linux-x86_64`, etc.); Windows uses
`windows-debug-x64`, `windows-release-x64`, or `windows-asan-x64`.
`--record` is an explicit maintenance command and is never invoked by CI.
Review the counts before committing them. A toolchain upgrade that changes
diagnostics needs a measured rebaseline and an explanation, rather than an
automatic increase. Do not record an incremental build or a failed build.
