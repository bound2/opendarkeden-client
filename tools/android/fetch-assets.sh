#!/usr/bin/env bash
#----------------------------------------------------------------------
# fetch-assets.sh - the game data for the Android package
#----------------------------------------------------------------------
#
#   tools/android/fetch-assets.sh [release tag]
#
# The data tree is not in this repository; it is a release of it
# (assets-v2 by default: https://github.com/bound2/opendarkeden-client/
# releases/tag/assets-v2, one 862 MB zip holding Data/ and an empty
# UserSet/). This downloads that zip into build/android/src, checks it
# against the release's .sha256, unpacks it under build/android/assets
# and writes the manifest beside it that basic/BundledAssets.cpp copies
# from on the device's first launch (the Android asset manager cannot
# list subdirectories, so the native side needs the list). The Gradle
# project packages build/android/assets as the APK's assets, and runs
# this script itself when the manifest is missing.
#
# The manifest (basic/BundledAssets.h documents it): a header line,
# `version <tag>`, then `dir <path>` for every directory and
# `file <size> <path>` for every file, size first because the tree has
# names with spaces. Sorted, so the same tree gives the same manifest.
#
# Requires curl, unzip, sha256sum (or shasum), find. Exit status
# non-zero on the first failure.
#----------------------------------------------------------------------
set -u

tag="${1:-assets-v2}"
name="darkeden-${tag}.zip"
url="https://github.com/bound2/opendarkeden-client/releases/download/$tag/$name"

cd "$(dirname "$0")/../.." || exit 2
root="$(pwd)"

src="$root/build/android/src"
assets="$root/build/android/assets"
manifest="$assets/darkeden-assets.manifest"
mkdir -p "$src"

if [ ! -f "$src/$name" ]; then
	echo "== fetch $name"
	if ! curl -sS -L -o "$src/$name.part" "$url"; then
		echo "-- cannot download $url" >&2
		exit 1
	fi
	mv "$src/$name.part" "$src/$name"
fi
if [ ! -f "$src/$name.sha256" ]; then
	if ! curl -sS -L -o "$src/$name.sha256" "$url.sha256"; then
		echo "-- cannot download $url.sha256" >&2
		exit 1
	fi
fi

echo "== verify $name"
expected="$(awk '{print $1}' "$src/$name.sha256" | head -n 1)"
if command -v sha256sum > /dev/null 2>&1; then
	actual="$(sha256sum "$src/$name" | awk '{print $1}')"
else
	actual="$(shasum -a 256 "$src/$name" | awk '{print $1}')"
fi
if [ -z "$expected" ] || [ "$expected" != "$actual" ]; then
	echo "-- $name: SHA-256 $actual does not match the release's $expected" >&2
	exit 1
fi

echo "== unpack into $assets"
rm -rf "$assets"
mkdir -p "$assets"
if ! unzip -q "$src/$name" -d "$assets"; then
	echo "-- cannot unpack $name" >&2
	exit 1
fi
if [ ! -f "$assets/Data/Info/FileDef.inf" ]; then
	echo "-- $name did not unpack to Data/Info/FileDef.inf under $assets" >&2
	exit 1
fi
# The game creates nothing at its root itself, and the zip's empty
# UserSet/ is what it writes into.
mkdir -p "$assets/UserSet"

# Two kinds of Windows leftover ride in the release and are dropped
# here: shortcut files (.lnk), and files whose names carry bytes
# outside ASCII (assets-v2 has one of each: "ServerInfo - <copy>.inf"
# beside the ServerInfo.inf the tables name, and a .lnk to a sound).
# The game's tables spell every path in ASCII (Data/Info/FileDef.inf),
# so nothing it opens is lost; the packaging cannot take them - Gradle
# hashes every asset by its name under the JVM's file-name encoding,
# which is the locale's and not always UTF-8 - and the asset manager's
# JNI path would be the next place to find out. Each one is named.
echo "== drop Windows leftovers"
(cd "$assets" && find . -type f \( -name '*.lnk' -o -name '*[^ -~]*' \) -print) | while IFS= read -r stray; do
	echo "   dropping $stray"
	rm -f "$assets/$stray"
done

# Written outside the tree it lists, so the walk cannot see it, and
# moved in last.
echo "== write $manifest"
tmp="$root/build/android/darkeden-assets.manifest.tmp"
{
	echo "darkeden-assets 1"
	echo "version $tag"
	(cd "$assets" && find . -mindepth 1 -type d | sed -e 's|^\./||' | LC_ALL=C sort | sed -e 's/^/dir /')
	(cd "$assets" && find . -type f -printf '%s %P\n' | LC_ALL=C sort -k2 | sed -e 's/^/file /')
} > "$tmp" && mv "$tmp" "$manifest"

echo "== done: $(grep -c '^file ' "$manifest") files, $(grep -c '^dir ' "$manifest") directories, version $tag"
