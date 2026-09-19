#!/usr/bin/env bash
#----------------------------------------------------------------------
# build-apk.sh - the release package for Android
#----------------------------------------------------------------------
#
#   tools/android/build-apk.sh <version>
#
#   version   what the package is called and what the app reports:
#             the release tag without its v (0.0.5), or anything for
#             a build that is not a release (0.0.0-abc1234)
#
# The Android counterpart of the local Release builds the Windows and
# Linux packages come from, and what .github/workflows/android.yml
# runs: the Gradle project's release build through its wrapper, with
# the version passed in, then the APK copied to
# build/android/dist/darkeden-client-<version>-android-arm64.apk -
# the release naming the other platforms use - with a .sha256 beside
# it in the sha256sum format, and its signature checked with the
# SDK's apksigner when the SDK is at hand. The libraries come from
# tools/android/build-deps.sh, run first here when they are missing.
#
# Signing: a keystore named by ANDROID_KEYSTORE_FILE (with
# ANDROID_KEYSTORE_PASSWORD, ANDROID_KEY_ALIAS, ANDROID_KEY_PASSWORD)
# signs the release; without one the debug key does, and the script
# says so, because such a package cannot upgrade an install signed
# with another key (app/build.gradle has the whole note).
#
# Requires ANDROID_HOME (the SDK: platform 34, build-tools 34.0.0,
# cmake 3.22.1) and ANDROID_NDK_HOME (a 27.x NDK; its version is
# passed to Gradle), a JDK 17 or later on JAVA_HOME or the path, and
# what build-deps.sh needs. Exit status non-zero on the first failing
# step; logs under build/verification/android-apk/.
#----------------------------------------------------------------------
set -u

version="${1:-}"
if [ -z "$version" ]; then
	echo "usage: $0 <version>" >&2
	exit 2
fi
abi=arm64-v8a

cd "$(dirname "$0")/../.." || exit 2
root="$(pwd)"

if [ -z "${ANDROID_HOME:-}" ] || [ ! -d "$ANDROID_HOME" ]; then
	echo "ANDROID_HOME must name the Android SDK" >&2
	exit 2
fi
if [ -z "${ANDROID_NDK_HOME:-}" ] || [ ! -f "$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" ]; then
	echo "ANDROID_NDK_HOME must name an NDK" >&2
	exit 2
fi

logdir="build/verification/android-apk"
dist="build/android/dist"
mkdir -p "$logdir" "$dist"

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

# The NDK's version as Gradle wants it, read from the NDK itself
# (source.properties: Pkg.Revision = 27.2.12479018).
ndk_version="$(sed -n 's/^Pkg\.Revision *= *//p' "$ANDROID_NDK_HOME/source.properties" | head -n 1)"
if [ -z "$ndk_version" ]; then
	echo "-- cannot read the NDK version from $ANDROID_NDK_HOME/source.properties" >&2
	exit 1
fi

if [ -n "${ANDROID_KEYSTORE_FILE:-}" ]; then
	echo "== signing with $ANDROID_KEYSTORE_FILE"
else
	echo "== no ANDROID_KEYSTORE_FILE: signing with the debug key (cannot upgrade an install signed with another key)"
fi

run gradle-assembleRelease sh -c "cd android && ./gradlew --no-daemon assembleRelease -PdarkedenVersion='$version' -PndkVersion='$ndk_version'"

apk="android/app/build/outputs/apk/release/app-release.apk"
if [ ! -f "$apk" ]; then
	echo "-- $apk was not produced" >&2
	exit 1
fi

name="darkeden-client-$version-android-arm64.apk"
cp "$apk" "$dist/$name"
(cd "$dist" && sha256sum "$name" > "$name.sha256")

# apksigner is the SDK's; the check is skipped, not failed, without it.
apksigner="$(ls -d "$ANDROID_HOME"/build-tools/*/apksigner 2>/dev/null | sort -V | tail -n 1)"
if [ -n "$apksigner" ]; then
	run apksigner-verify "$apksigner" verify --print-certs "$dist/$name"
	grep -E "Signer .* certificate (DN|SHA-256)" "$logdir/apksigner-verify.log" | head -n 2
else
	echo "== apksigner not found under $ANDROID_HOME/build-tools; signature not checked"
fi

echo "== ok: $dist/$name ($(du -h "$dist/$name" | cut -f1)), version $version, NDK $ndk_version"
cat "$dist/$name.sha256"
