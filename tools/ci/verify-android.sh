#!/usr/bin/env bash
#----------------------------------------------------------------------
# verify-android.sh - build the game's Android library for one ABI
#----------------------------------------------------------------------
#
#   tools/ci/verify-android.sh
#
# The Android counterpart of verify-linux.sh, and the job
# .github/workflows/android.yml runs: the libraries from
# tools/android/build-deps.sh (built here when build/android/deps/<abi>
# is missing, reused when it is there), then the `android` preset's
# configure and build (CMakePresets.json), then a check that the
# artefact the Gradle project packages, bin/libmain.so, exists. No
# tests: the unit binary is an executable, and nothing here can run
# one for arm64; the Linux jobs run the same libraries' tests. No
# Gradle either: the APK wraps this library and needs the SDK, and
# what this proves is that the whole game compiles and links for the
# NDK, which is the part that can rot.
#
# Requires ANDROID_NDK_HOME (the preset's toolchain file lives under
# it), cmake >= 3.21, ninja, curl, tar. Exit status non-zero on the
# first failing step, with that step's log tail on stderr; logs under
# build/verification/android/.
#----------------------------------------------------------------------
set -u

abi=arm64-v8a

cd "$(dirname "$0")/../.." || exit 2

if [ -z "${ANDROID_NDK_HOME:-}" ]; then
	echo "ANDROID_NDK_HOME is not set" >&2
	exit 2
fi

logdir="build/verification/android"
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

if [ ! -d "build/android/deps/$abi/lib/cmake/SDL2" ]; then
	run deps tools/android/build-deps.sh "$abi"
fi

run configure cmake --preset android
run build cmake --build --preset android

if [ ! -f build/presets/android/bin/libmain.so ]; then
	echo "-- build/presets/android/bin/libmain.so was not produced" >&2
	exit 1
fi

echo "== ok: build/presets/android/bin/libmain.so"
