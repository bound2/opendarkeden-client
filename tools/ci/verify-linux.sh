#!/usr/bin/env bash
#----------------------------------------------------------------------
# verify-linux.sh - configure, build and test one Linux or macOS preset
#----------------------------------------------------------------------
#
#   tools/ci/verify-linux.sh linux          GCC, Debug, every target
#   tools/ci/verify-linux.sh linux-clang    Clang
#   tools/ci/verify-linux.sh linux-asan     GCC with ASan and UBSan
#   tools/ci/verify-linux.sh macos          Clang on macOS
#
# The same steps the Linux workflow runs, so a local run and the CI run
# cannot drift: the preset's configure, a full build (every library,
# unit_tests, and the DarkEden executable, which links on Linux since
# the port's area E), then ctest with the ratchets and the audit scripts.
# Under the sanitizer preset the tests run with UBSan halting on the
# first report, so a report is a failure and not a line in a log.
#
# Dependencies (Ubuntu 24.04 names; tools/linux/Dockerfile is the
# reference image): build-essential clang cmake ninja-build perl
# libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsdl2-mixer-dev
# libjpeg-dev libfreetype-dev fonts-dejavu-core fonts-noto-cjk.
#
# Exit status: non-zero on the first failing step, with that step's
# log tail on stderr. Logs go under build/verification/<preset>/.
#----------------------------------------------------------------------
set -u

preset="${1:-}"
case "$preset" in
	linux|linux-clang|linux-asan|macos) ;;
	*) echo "usage: $0 linux|linux-clang|linux-asan|macos" >&2; exit 2 ;;
esac

cd "$(dirname "$0")/../.." || exit 2

logdir="build/verification/$preset"
mkdir -p "$logdir"

run () {
	local name="$1"; shift
	local log="$logdir/$name.log"
	echo "== $name: $*"
	if ! "$@" > "$log" 2>&1; then
		echo "-- $name FAILED; last 150 lines of $log:" >&2
		tail -n 150 "$log" >&2
		exit 1
	fi
}

run configure cmake --preset "$preset"
run build cmake --build --preset "$preset"

builddir="build/presets/$preset"

# The sanitizer preset: every UBSan report is fatal, so the suite cannot
# pass while printing one. Leak detection stays off - the game's static
# tables are never freed by design, and the tests construct them.
export ASAN_OPTIONS="detect_leaks=0:abort_on_error=1"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"

run test ctest --test-dir "$builddir" --output-on-failure --timeout 300

echo "== $preset: configure, build and test passed"
