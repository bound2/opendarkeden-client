package org.opendarkeden.client;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URI;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Enumeration;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * The game data, fetched on the first launch.
 *
 * The data tree is not in the APK: it is a release of the repository (one
 * zip holding Data/ and an empty UserSet/), and the APK would be a
 * gigabyte with it inside. This downloads that zip into the app's cache,
 * verifies it against the SHA-256 pinned below, unpacks it into the app's
 * internal files directory - the directory the native side's data-root
 * search looks under (Client/Client.cpp), and the one SDL reports as
 * internal storage - and writes a marker holding the release tag last, so
 * a later launch returns at once, an interrupted install is redone, and an
 * upgrade of the constants below fetches the new release. The download
 * resumes from where it stopped (Range request), since 862 MB over a
 * phone's link is often interrupted; the hash is checked over the whole
 * file once it is complete, and a file that fails is deleted so the next
 * attempt starts clean.
 *
 * Plain Java: nothing from android.* is used, so the same class runs on a
 * desktop JVM through main() below, which is how it was verified against
 * the real release (android/README.md says how). BootstrapActivity drives
 * it on the device.
 *
 * Two kinds of Windows leftover ride in the release and are skipped: .lnk
 * shortcut files, and files whose names carry characters outside ASCII
 * (assets-v2 has one of each, a shortcut to a sound and a "copy" of
 * ServerInfo.inf beside the one the tables name). The game's tables spell
 * every path in ASCII (Data/Info/FileDef.inf), so nothing it opens is lost.
 */
public final class AssetInstaller {

	/** The release: bump all four together (the size and hash are the release page's). */
	public static final String TAG = "assets-v2";
	public static final String URL_STRING =
		"https://github.com/bound2/opendarkeden-client/releases/download/" + TAG + "/darkeden-" + TAG + ".zip";
	public static final String SHA256 = "96f8d7bd8f1578f3368c88f0dee81a66e00bac77120e5fac8a2063ca5b81d065";
	public static final long ZIP_BYTES = 861904447L;

	/** The unpacked tree, rounded up, for the free-space check. */
	public static final long UNPACKED_BYTES = 1_900_000_000L;

	/** Under the files directory: the tag of the release installed there. */
	public static final String MARKER = "darkeden-assets.version";

	/** The marker file the native data-root search keys on. */
	public static final String DATA_MARKER = "Data/Info/FileDef.inf";

	public interface Listener {
		/** percent is 0..100, or -1 when the step has no measurable length. */
		void onProgress(String status, int percent);
	}

	private AssetInstaller() {}

	/** True when the marker under filesDir names this class's release. */
	public static boolean isInstalled(File filesDir) {
		File marker = new File(filesDir, MARKER);
		if (!marker.isFile()) {
			return false;
		}
		try {
			String tag = new String(Files.readAllBytes(marker.toPath()), StandardCharsets.UTF_8).trim();
			return TAG.equals(tag) && new File(filesDir, DATA_MARKER).isFile();
		} catch (IOException e) {
			return false;
		}
	}

	/**
	 * Downloads, verifies and unpacks the release under filesDir, using
	 * cacheDir for the zip. Throws with a message fit for the screen.
	 */
	public static void install(File filesDir, File cacheDir, Listener listener) throws IOException {
		if (!filesDir.isDirectory() && !filesDir.mkdirs()) {
			throw new IOException("Cannot create " + filesDir);
		}
		if (!cacheDir.isDirectory() && !cacheDir.mkdirs()) {
			throw new IOException("Cannot create " + cacheDir);
		}

		File zip = new File(cacheDir, "darkeden-" + TAG + ".zip.part");

		long have = zip.isFile() ? zip.length() : 0;
		long need = Math.max(0, ZIP_BYTES - have) + UNPACKED_BYTES;
		long free = filesDir.getUsableSpace();
		if (free < need) {
			throw new IOException(String.format("Not enough space: %d MB free, about %d MB needed",
				free / (1024 * 1024), need / (1024 * 1024)));
		}

		// A stale marker from an older release goes first, so an install
		// that stops half way leaves no claim behind.
		new File(filesDir, MARKER).delete();

		download(zip, listener);
		verify(zip, listener);
		unpack(zip, filesDir, listener);

		File marker = new File(filesDir, MARKER);
		try (OutputStream out = new FileOutputStream(marker)) {
			out.write((TAG + "\n").getBytes(StandardCharsets.UTF_8));
		}
		zip.delete();
		listener.onProgress("Ready", 100);
	}

	private static void download(File zip, Listener listener) throws IOException {
		long have = zip.isFile() ? zip.length() : 0;
		if (have >= ZIP_BYTES) {
			return; // complete, or over-long: verify() decides
		}

		HttpURLConnection connection = (HttpURLConnection) URI.create(URL_STRING).toURL().openConnection();
		connection.setInstanceFollowRedirects(true);
		connection.setConnectTimeout(30_000);
		connection.setReadTimeout(60_000);
		if (have > 0) {
			connection.setRequestProperty("Range", "bytes=" + have + "-");
		}
		connection.connect();

		int code = connection.getResponseCode();
		boolean append;
		if (code == HttpURLConnection.HTTP_PARTIAL && have > 0) {
			append = true;
		} else if (code == HttpURLConnection.HTTP_OK) {
			append = false;
			have = 0;
		} else {
			connection.disconnect();
			throw new IOException("Download failed: HTTP " + code);
		}

		long total = have + Math.max(0, connection.getContentLengthLong());
		if (total <= 0) {
			total = ZIP_BYTES;
		}

		byte[] buffer = new byte[65536];
		long done = have;
		long lastReport = -1;
		try (InputStream in = new BufferedInputStream(connection.getInputStream(), buffer.length);
		     OutputStream out = new FileOutputStream(zip, append)) {
			int n;
			while ((n = in.read(buffer)) > 0) {
				out.write(buffer, 0, n);
				done += n;
				if (done - lastReport >= (1 << 20)) {
					lastReport = done;
					listener.onProgress(String.format("Downloading game data: %d of %d MB",
						done / (1024 * 1024), total / (1024 * 1024)), (int) (done * 100 / total));
				}
			}
		} finally {
			connection.disconnect();
		}

		if (done < total) {
			throw new IOException(String.format("Download stopped at %d of %d MB; try again to resume",
				done / (1024 * 1024), total / (1024 * 1024)));
		}
	}

	private static void verify(File zip, Listener listener) throws IOException {
		listener.onProgress("Checking the download", -1);

		MessageDigest digest;
		try {
			digest = MessageDigest.getInstance("SHA-256");
		} catch (NoSuchAlgorithmException e) {
			throw new IOException("SHA-256 is not available", e);
		}

		byte[] buffer = new byte[65536];
		long total = zip.length();
		long done = 0;
		try (InputStream in = new BufferedInputStream(new FileInputStream(zip), buffer.length)) {
			int n;
			while ((n = in.read(buffer)) > 0) {
				digest.update(buffer, 0, n);
				done += n;
				if (total > 0 && (done & ((1 << 24) - 1)) == 0) {
					listener.onProgress("Checking the download", (int) (done * 100 / total));
				}
			}
		}

		StringBuilder hex = new StringBuilder();
		for (byte b : digest.digest()) {
			hex.append(String.format("%02x", b));
		}
		if (!SHA256.equals(hex.toString())) {
			zip.delete();
			throw new IOException("The download did not match the release (SHA-256 "
				+ hex + "); it has been removed, try again");
		}
	}

	private static void unpack(File zip, File filesDir, Listener listener) throws IOException {
		try (ZipFile file = new ZipFile(zip)) {
			int total = file.size();
			int index = 0;
			byte[] buffer = new byte[65536];

			Enumeration<? extends ZipEntry> entries = file.entries();
			while (entries.hasMoreElements()) {
				ZipEntry entry = entries.nextElement();
				index++;
				String name = entry.getName();

				if (!isSafeRelativePath(name)) {
					throw new IOException("The release names a path outside the install: " + name);
				}
				if (name.endsWith(".lnk") || !isAscii(name)) {
					System.err.println("AssetInstaller: skipping " + name);
					continue;
				}

				File target = new File(filesDir, name);
				if (entry.isDirectory()) {
					if (!target.isDirectory() && !target.mkdirs()) {
						throw new IOException("Cannot create " + target);
					}
					continue;
				}

				File parent = target.getParentFile();
				if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
					throw new IOException("Cannot create " + parent);
				}

				if ((index & 15) == 0) {
					listener.onProgress(String.format("Unpacking game data: %d of %d files", index, total),
						(int) ((long) index * 100 / total));
				}

				try (InputStream in = file.getInputStream(entry);
				     OutputStream out = new FileOutputStream(target)) {
					int n;
					while ((n = in.read(buffer)) > 0) {
						out.write(buffer, 0, n);
					}
				}
			}
		}

		// The game creates nothing at its root itself, and the zip's empty
		// UserSet/ - which a zip need not carry - is what it writes into.
		File userSet = new File(filesDir, "UserSet");
		if (!userSet.isDirectory() && !userSet.mkdirs()) {
			throw new IOException("Cannot create " + userSet);
		}
		if (!new File(filesDir, DATA_MARKER).isFile()) {
			throw new IOException("The release did not unpack to " + DATA_MARKER);
		}
	}

	private static boolean isSafeRelativePath(String name) {
		if (name.isEmpty() || name.startsWith("/") || name.startsWith("\\")) {
			return false;
		}
		if (name.length() >= 2 && name.charAt(1) == ':') {
			return false;
		}
		for (String part : name.split("[/\\\\]")) {
			if (part.equals("..")) {
				return false;
			}
		}
		return true;
	}

	private static boolean isAscii(String name) {
		for (int i = 0; i < name.length(); i++) {
			char c = name.charAt(i);
			if (c < 0x20 || c > 0x7e) {
				return false;
			}
		}
		return true;
	}

	/**
	 * Desktop entry point: installs the release under the directory given,
	 * with the cache beside it, printing progress. Verifies the class
	 * against the real release without a device:
	 *
	 *   javac -d /tmp/ai android/app/src/main/java/org/opendarkeden/client/AssetInstaller.java
	 *   java -cp /tmp/ai org.opendarkeden.client.AssetInstaller /tmp/darkeden
	 */
	public static void main(String[] args) throws IOException {
		if (args.length != 1) {
			System.err.println("usage: AssetInstaller <install directory>");
			System.exit(2);
		}
		File filesDir = new File(args[0], "files");
		File cacheDir = new File(args[0], "cache");
		if (isInstalled(filesDir)) {
			System.out.println(TAG + " is already installed under " + filesDir);
			return;
		}
		final String[] last = { "" };
		install(filesDir, cacheDir, (status, percent) -> {
			String line = percent >= 0 ? status + " (" + percent + "%)" : status;
			if (!line.equals(last[0])) {
				System.out.println(line);
				last[0] = line;
			}
		});
		System.out.println("installed " + TAG + " under " + filesDir);
	}
}
