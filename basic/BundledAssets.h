/*-----------------------------------------------------------------------------

	BundledAssets.h

	The game's data tree, shipped inside the application package and
	installed to a writable directory on first launch.

	The game reads its data through ordinary file opens relative to a
	working directory it changes into (Client.cpp, the data-root search),
	and writes beside it (UserSet/, Log/, the profile directory). An
	Android APK's assets are entries in a zip, readable only through the
	asset manager and never writable, so the tree has to be copied out
	once; an iOS bundle is on the file system but read-only and signed,
	so the same copy applies there. This does the copy from a manifest,
	because the Android asset manager can list the files of a directory
	but not its subdirectories, and a walk of the tree is not possible
	from the native side.

	The manifest (tools/android/fetch-assets.sh writes it):

		darkeden-assets 1
		version <id>
		dir <path>
		file <size> <path>
		...

	Paths are relative, '/'-separated, and may contain spaces (the size
	comes first for that reason). Every source is opened through
	SDL_RWFromFile with the path appended to a source root: an empty
	root on Android, where a relative path names an asset, and a
	directory anywhere else, which is how the tests run it. The
	destination gets the directories, the files, and last a marker file
	(darkeden-assets.version) holding the manifest's id; a later launch
	that finds the marker naming the same id copies nothing. A copy
	interrupted before the marker is redone in full next time; a file
	whose bytes do not match the manifest's size is an error and the
	marker is not written. Files of an older version that the newer
	manifest does not name are left where they are.

	docs/android-port-2026-09-19.md, "Bundled assets".

-----------------------------------------------------------------------------*/

#ifndef __BUNDLED_ASSETS_H__
#define __BUNDLED_ASSETS_H__

#include <cstddef>
#include <string>

namespace Basic {

// The marker file's name under the destination directory.
extern const char* const BUNDLED_ASSETS_MARKER;

struct SBundledAssetsResult
{
	bool		bInstalled = false;		// the destination now carries the manifest's version
	bool		bAlreadyCurrent = false;	// nothing was copied: the marker already named it
	std::string	sVersion;			// the manifest's id, when the manifest was readable
	size_t		nFiles = 0;			// files written this run
	size_t		nDirectories = 0;		// directories created this run
	std::string	sError;				// empty on success; otherwise the first failure, with its path
};

// Called before each file is copied: how many of the manifest's files are
// done, how many there are, and the path about to be written.
typedef void (*BundledAssetsProgressFn)(size_t nDone, size_t nTotal, const char* pPath, void* pUser);

// Installs the tree the manifest describes under sDestDir (created if
// missing). sSourceRoot is prepended to the manifest's name and to every
// path in it with a '/', or to nothing when empty.
SBundledAssetsResult InstallBundledAssets(
	const std::string& sSourceRoot,
	const std::string& sManifestName,
	const std::string& sDestDir,
	BundledAssetsProgressFn pfnProgress = nullptr,
	void* pUser = nullptr);

} // namespace Basic

#endif // __BUNDLED_ASSETS_H__
