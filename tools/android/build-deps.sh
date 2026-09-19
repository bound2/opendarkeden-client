#!/usr/bin/env bash
#----------------------------------------------------------------------
# build-deps.sh - build the game's libraries for one Android ABI
#----------------------------------------------------------------------
#
#   tools/android/build-deps.sh [abi] [api]
#
#   abi   arm64-v8a (default), armeabi-v7a, x86_64 or x86
#   api   minimum Android API level, default 28 (bionic gained the
#         iconv the string code needs in 28; anything older would need
#         a libiconv build here as well)
#
# Android has no package manager the build can ask for SDL2, so the
# five libraries the desktop takes from vcpkg, apt or Homebrew are
# built from source with the NDK toolchain and installed under one
# prefix, build/android/deps/<abi>, which the android CMake preset
# (CMakePresets.json) and the Gradle project (android/) hand to
# find_package through CMAKE_FIND_ROOT_PATH. Nothing in the game's own
# CMake changes: every find_package(SDL2) site resolves against the
# prefix exactly as it resolves against vcpkg on Windows.
#
# What is built, and why in that shape:
#   SDL2          shared. SDLActivity loads libSDL2.so by name before
#                 it loads the game, so it cannot be static.
#   SDL2_image    shared, stb backends for PNG and JPEG (nothing vendored
#                 despite the flag; the script says why it is set).
#   SDL2_ttf      shared, with its vendored FreeType (and no HarfBuzz).
#   SDL2_mixer    shared, minimp3 and stb_vorbis built in; the formats
#                 the game does not ship (FLAC, MOD, MIDI, Opus,
#                 WavPack, GME) off, so nothing else is vendored.
#                 The three satellites are shared like SDL2 itself
#                 because that is the shape their CMake exports by
#                 default and the shape SDLActivity.getLibraries()
#                 loads; the APK carries the four .so files.
#   libjpeg-turbo static, at the libjpeg 6.2 ABI that
#                 Client/JpegLib/jpeglib.h declares, linked into the
#                 game's own library.
#   Two directories beside the prefix are what the Gradle project
#   (android/) points at: build/android/java holds the SDL Java classes
#   (org.libsdl.app, copied from the SDL source tree, which cmake
#   --install does not ship) and build/android/jniLibs/<abi> the four
#   shared libraries, in the <abi>/lib*.so layout jniLibs wants.
#
# Sources: the pinned release tarballs below, downloaded into
# build/android/src unless DARKEDEN_ANDROID_SRC names a directory that
# already holds the unpacked trees (SDL2-<v>, SDL2_image-<v>, ...).
#
# Requires: ANDROID_NDK_HOME (or ANDROID_NDK_ROOT), cmake >= 3.21,
# ninja, curl, tar. Exit status non-zero on the first failing step,
# with that step's log tail on stderr; logs under
# build/android/deps-logs/<abi>/.
#----------------------------------------------------------------------
set -u

abi="${1:-arm64-v8a}"
api="${2:-28}"

case "$abi" in
	arm64-v8a|armeabi-v7a|x86_64|x86) ;;
	*) echo "usage: $0 [arm64-v8a|armeabi-v7a|x86_64|x86] [api]" >&2; exit 2 ;;
esac

ndk="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
if [ -z "$ndk" ] || [ ! -f "$ndk/build/cmake/android.toolchain.cmake" ]; then
	echo "ANDROID_NDK_HOME must name an NDK (build/cmake/android.toolchain.cmake not found under '$ndk')" >&2
	exit 2
fi

cd "$(dirname "$0")/../.." || exit 2
root="$(pwd)"

SDL2_VERSION=2.30.11
SDL2_IMAGE_VERSION=2.8.4
SDL2_TTF_VERSION=2.22.0
SDL2_MIXER_VERSION=2.8.1
JPEG_TURBO_VERSION=3.0.4

src="${DARKEDEN_ANDROID_SRC:-$root/build/android/src}"
prefix="$root/build/android/deps/$abi"
logdir="$root/build/android/deps-logs/$abi"
mkdir -p "$src" "$prefix" "$logdir"

run () {
	local name="$1"; shift
	local log="$logdir/$name.log"
	echo "== $name: $*"
	if ! "$@" > "$log" 2>&1; then
		echo "-- $name FAILED; last 60 lines of $log:" >&2
		tail -n 60 "$log" >&2
		exit 1
	fi
}

# fetch <dir name> <url>: unpack the tarball into $src unless the tree
# is already there.
fetch () {
	local dir="$1" url="$2"
	if [ -d "$src/$dir" ]; then
		return
	fi
	echo "== fetch $dir"
	if ! curl -sS -L -o "$src/$dir.tar.gz" "$url"; then
		echo "-- cannot download $url" >&2
		exit 1
	fi
	tar -xzf "$src/$dir.tar.gz" -C "$src" && rm -f "$src/$dir.tar.gz"
}

fetch "SDL2-$SDL2_VERSION"             "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VERSION/SDL2-$SDL2_VERSION.tar.gz"
fetch "SDL2_image-$SDL2_IMAGE_VERSION" "https://github.com/libsdl-org/SDL_image/releases/download/release-$SDL2_IMAGE_VERSION/SDL2_image-$SDL2_IMAGE_VERSION.tar.gz"
fetch "SDL2_ttf-$SDL2_TTF_VERSION"     "https://github.com/libsdl-org/SDL_ttf/releases/download/release-$SDL2_TTF_VERSION/SDL2_ttf-$SDL2_TTF_VERSION.tar.gz"
fetch "SDL2_mixer-$SDL2_MIXER_VERSION" "https://github.com/libsdl-org/SDL_mixer/releases/download/release-$SDL2_MIXER_VERSION/SDL2_mixer-$SDL2_MIXER_VERSION.tar.gz"
fetch "libjpeg-turbo-$JPEG_TURBO_VERSION" "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/$JPEG_TURBO_VERSION/libjpeg-turbo-$JPEG_TURBO_VERSION.tar.gz"

# build <name> <source dir> <cmake options...>: configure, build and
# install one library into the prefix. Release: the game's own build
# is what gets debugged, and a Debug SDL is a much slower present.
build () {
	local name="$1" dir="$2"; shift 2
	local bdir="$root/build/android/deps-build/$abi/$name"
	run "configure-$name" cmake -S "$dir" -B "$bdir" -G Ninja \
		-DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" \
		-DANDROID_ABI="$abi" -DANDROID_PLATFORM="android-$api" \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_INSTALL_PREFIX="$prefix" \
		-DCMAKE_FIND_ROOT_PATH="$prefix" \
		-DCMAKE_POSITION_INDEPENDENT_CODE=ON \
		"$@"
	run "build-$name" cmake --build "$bdir"
	run "install-$name" cmake --install "$bdir"
}

build SDL2 "$src/SDL2-$SDL2_VERSION" \
	-DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF

# VENDORED=ON with the stb backend vendors nothing (stb_image is in
# the tree and every other format is off); it is set because the
# exported SDL2_imageConfig.cmake asks its consumer for libpng and
# libjpeg whenever VENDORED is off and PNG or JPG is on, backend or
# not, and no consumer under the NDK has either.
build SDL2_image "$src/SDL2_image-$SDL2_IMAGE_VERSION" \
	-DBUILD_SHARED_LIBS=ON \
	-DSDL2IMAGE_SAMPLES=OFF -DSDL2IMAGE_VENDORED=ON -DSDL2IMAGE_BACKEND_STB=ON \
	-DSDL2IMAGE_AVIF=OFF -DSDL2IMAGE_JXL=OFF -DSDL2IMAGE_TIF=OFF -DSDL2IMAGE_WEBP=OFF

build SDL2_ttf "$src/SDL2_ttf-$SDL2_TTF_VERSION" \
	-DBUILD_SHARED_LIBS=ON \
	-DSDL2TTF_SAMPLES=OFF -DSDL2TTF_VENDORED=ON -DSDL2TTF_HARFBUZZ=OFF

build SDL2_mixer "$src/SDL2_mixer-$SDL2_MIXER_VERSION" \
	-DBUILD_SHARED_LIBS=ON \
	-DSDL2MIXER_SAMPLES=OFF -DSDL2MIXER_VENDORED=OFF \
	-DSDL2MIXER_MP3_MINIMP3=ON -DSDL2MIXER_VORBIS=STB \
	-DSDL2MIXER_OPUS=OFF -DSDL2MIXER_FLAC=OFF -DSDL2MIXER_MOD=OFF \
	-DSDL2MIXER_MIDI=OFF -DSDL2MIXER_WAVPACK=OFF -DSDL2MIXER_GME=OFF

build libjpeg-turbo "$src/libjpeg-turbo-$JPEG_TURBO_VERSION" \
	-DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=OFF -DWITH_SIMD=OFF

# The Java side of SDL and the shared libraries, where android/ looks
# for them (see the header).
rm -rf "$root/build/android/java"
mkdir -p "$root/build/android/java"
cp -R "$src/SDL2-$SDL2_VERSION/android-project/app/src/main/java/." "$root/build/android/java/"
rm -rf "$root/build/android/jniLibs/$abi"
mkdir -p "$root/build/android/jniLibs/$abi"
cp "$prefix"/lib/lib*.so "$root/build/android/jniLibs/$abi/"

echo "== done: $prefix"
