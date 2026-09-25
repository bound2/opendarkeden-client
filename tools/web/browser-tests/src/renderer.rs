//! Runs the shared sprite-renderer pixel oracle in Chrome with WebGL 2.

use std::env;
use std::sync::mpsc::RecvTimeoutError;
use std::time::{Duration, Instant};

use anyhow::{bail, Context, Result};
use regex::Regex;

use crate::chrome::{self, Chrome, Launch, PageEvent};
use crate::Usage;

const DEFAULT_URL: &str = "http://127.0.0.1:18739/web_sprite_tests.html";
const TIMEOUT: Duration = Duration::from_secs(900);

/// Logs the WebGL 2 device the page's renderer actually received.
const DEVICE_PROBE: &str = r"
(() => {
  const original = HTMLCanvasElement.prototype.getContext;
  HTMLCanvasElement.prototype.getContext = function(type, ...args) {
    const context = original.call(this, type, ...args);
    if (type === 'webgl2' && context) {
      const debug = context.getExtension('WEBGL_debug_renderer_info');
      if (debug) console.log(`WebGL device: ${context.getParameter(debug.UNMASKED_RENDERER_WEBGL)}`);
    }
    return context;
  };
})();
";

pub fn run(args: &[String]) -> Result<()> {
    let url = match args {
        [] => DEFAULT_URL,
        [url] => url.as_str(),
        _ => return Err(Usage("renderer takes at most one URL").into()),
    };
    let mut switches = Vec::new();
    if env::var_os("WEB_TEST_SOFTWARE_GL").is_some_and(|value| !value.is_empty()) {
        switches.extend([
            "--use-angle=swiftshader".to_owned(),
            "--enable-unsafe-swiftshader".to_owned(),
        ]);
    }
    let chrome = Chrome::launch(Launch {
        args: switches,
        profile: None,
        window: None,
    })?;
    chrome::add_init_script(&chrome.tab, DEVICE_PROBE)?;
    let events = chrome::page_events(&chrome.tab)?;
    chrome::navigate(&chrome.tab, url)?;

    let summary = Regex::new(r"(\d+) test\(s\), (\d+) check\(s\), (\d+) failed")?;
    let mut lines = Vec::new();
    let deadline = Instant::now() + TIMEOUT;
    let failed = loop {
        let remaining = deadline.saturating_duration_since(Instant::now());
        match events.recv_timeout(remaining) {
            Ok(PageEvent::Console(line)) => {
                let result = summary
                    .captures(&line)
                    .map(|captures| captures[3].parse::<u64>());
                lines.push(line);
                if let Some(result) = result {
                    break result.context("the failure count does not fit in 64 bits")?;
                }
            }
            Ok(PageEvent::Error(message)) => {
                lines.push(format!("PAGE ERROR: {message}"));
                break 1;
            }
            Err(RecvTimeoutError::Timeout) => {
                lines.push("Renderer tests timed out".to_owned());
                break 1;
            }
            Err(RecvTimeoutError::Disconnected) => {
                lines.push("PAGE ERROR: the browser connection closed".to_owned());
                break 1;
            }
        }
    };

    let interesting = Regex::new(r"device:|shaders|\[FAIL\]|test\(s\)|PAGE ERROR|timed out")?;
    let mut shown: Vec<&str> = Vec::new();
    for line in &lines {
        if interesting.is_match(line) && !shown.contains(&line.as_str()) {
            shown.push(line);
        }
    }
    println!("{}", shown.join("\n"));

    let log = env::var("WEB_TEST_LOG").unwrap_or_else(|_| "web-renderer.log".to_owned());
    std::fs::write(&log, lines.join("\n") + "\n").with_context(|| format!("cannot write {log}"))?;

    if failed > 0 {
        bail!("the renderer checks failed; the full console is in {log}");
    }
    Ok(())
}
