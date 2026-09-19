package org.opendarkeden.client;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;

/**
 * The launcher activity: makes sure the game data is installed, then
 * starts the game. On a launch that finds the data (the usual case) it
 * shows nothing and hands over at once; on the first launch, or after an
 * upgrade that pins a new release, it shows a progress bar over
 * AssetInstaller's download, check and unpack, with a retry button on a
 * failure - a download of this size over a phone's link stops sometimes,
 * and the installer resumes it. SDLActivity is not the place for this: it
 * starts the native side in its onCreate, and the game would sit in a
 * black window while the tree arrived.
 *
 * A tree pushed by hand to the app's external files directory (see
 * android/README.md) counts as installed too, so a developer's own data
 * is never overwritten and never waits for a download; the native side
 * searches that directory first for the same reason.
 */
public class BootstrapActivity extends Activity {

	private TextView statusView;
	private ProgressBar progressBar;
	private Button retryButton;
	private volatile boolean running;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		if (AssetInstaller.isInstalled(getFilesDir()) || hasDeveloperTree()) {
			launchGame();
			return;
		}

		buildView();
		startInstall();
	}

	private boolean hasDeveloperTree() {
		File external = getExternalFilesDir(null);
		return external != null && new File(external, AssetInstaller.DATA_MARKER).isFile();
	}

	private void buildView() {
		int padding = dp(24);

		LinearLayout layout = new LinearLayout(this);
		layout.setOrientation(LinearLayout.VERTICAL);
		layout.setGravity(Gravity.CENTER);
		layout.setPadding(padding, padding, padding, padding);

		TextView title = new TextView(this);
		title.setText(R.string.app_name);
		title.setTextSize(TypedValue.COMPLEX_UNIT_SP, 28);
		title.setGravity(Gravity.CENTER);
		layout.addView(title);

		statusView = new TextView(this);
		statusView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
		statusView.setGravity(Gravity.CENTER);
		statusView.setPadding(0, dp(16), 0, dp(8));
		statusView.setText(getString(R.string.bootstrap_preparing,
			AssetInstaller.ZIP_BYTES / (1024 * 1024)));
		layout.addView(statusView);

		progressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
		progressBar.setMax(100);
		progressBar.setIndeterminate(true);
		layout.addView(progressBar, new LinearLayout.LayoutParams(
			LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));

		retryButton = new Button(this);
		retryButton.setText(R.string.bootstrap_retry);
		retryButton.setVisibility(View.GONE);
		retryButton.setOnClickListener(v -> startInstall());
		layout.addView(retryButton);

		setContentView(layout);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
	}

	private int dp(int value) {
		return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value,
			getResources().getDisplayMetrics());
	}

	private void startInstall() {
		if (running) {
			return;
		}
		running = true;
		retryButton.setVisibility(View.GONE);
		progressBar.setIndeterminate(true);

		final File filesDir = getFilesDir();
		final File cacheDir = getCacheDir();

		Thread worker = new Thread(() -> {
			try {
				AssetInstaller.install(filesDir, cacheDir, (status, percent) ->
					runOnUiThread(() -> showProgress(status, percent)));
				runOnUiThread(this::launchGame);
			} catch (Exception e) {
				String message = e.getMessage() != null ? e.getMessage() : e.toString();
				runOnUiThread(() -> showFailure(message));
			} finally {
				running = false;
			}
		}, "AssetInstaller");
		worker.setDaemon(true);
		worker.start();
	}

	private void showProgress(String status, int percent) {
		if (isFinishing()) {
			return;
		}
		statusView.setText(status);
		if (percent < 0) {
			progressBar.setIndeterminate(true);
		} else {
			progressBar.setIndeterminate(false);
			progressBar.setProgress(percent);
		}
	}

	private void showFailure(String message) {
		if (isFinishing()) {
			return;
		}
		statusView.setText(getString(R.string.bootstrap_failed, message));
		progressBar.setIndeterminate(false);
		retryButton.setVisibility(View.VISIBLE);
	}

	private void launchGame() {
		startActivity(new Intent(this, DarkEdenActivity.class));
		finish();
	}
}
