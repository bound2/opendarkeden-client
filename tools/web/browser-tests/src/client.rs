//! Smoke test of the real browser client: load, menu, typing, canvas sizing,
//! fullscreen, normal exit and settings persistence.

use std::env;
use std::path::{Path, PathBuf};
use std::sync::mpsc::Receiver;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use anyhow::{anyhow, bail, ensure, Context, Result};
use base64::prelude::{Engine, BASE64_STANDARD};
use headless_chrome::browser::tab::RequestPausedDecision;
use headless_chrome::browser::transport::{SessionId, Transport};
use headless_chrome::protocol::cdp::Fetch::events::{RequestPausedEvent, RequestPausedEventParams};
use headless_chrome::protocol::cdp::Fetch::{
    FulfillRequest, GetResponseBody, RequestPattern, RequestStage,
};
use headless_chrome::protocol::cdp::Network;
use headless_chrome::Tab;
use regex::Regex;
use serde_json::{json, Value};

use crate::chrome::{self, Chrome, Launch, PageEvent};
use crate::Usage;

const DEFAULT_URL: &str = "http://127.0.0.1:18739/";
const LOAD_TIMEOUT: Duration = Duration::from_secs(300);
const WAIT_TIMEOUT: Duration = Duration::from_secs(30);
const SETTINGS_FILE: &str = "/UserSet/UserOption.set";

/// Counts animation frames so the test can tell the game loop is running.
const FRAME_COUNTER: &str = r"(() => {
window.browserFrames = 0;
const request = window.requestAnimationFrame.bind(window);
window.requestAnimationFrame = callback => request(time => { ++window.browserFrames; callback(time); });
})();";

const CANVAS_SIZE: &str = r"(() => {
  const canvas = document.querySelector('canvas');
  const rect = canvas.getBoundingClientRect();
  const scale = Math.min(devicePixelRatio || 1, 4096 / rect.width, 4096 / rect.height);
  return { width: canvas.width, height: canvas.height,
    expectedWidth: Math.round(rect.width * scale), expectedHeight: Math.round(rect.height * scale) };
})()";

pub fn run(args: &[String]) -> Result<()> {
    let url = match args {
        [] => DEFAULT_URL,
        [url] => url.as_str(),
        _ => return Err(Usage("client takes at most one URL").into()),
    };
    let height: u32 = env_value("WEB_TEST_HEIGHT", 900)?;
    let scale: f64 = env_value("WEB_TEST_DPR", 1.0)?;
    let software_sprites = env::var("WEB_TEST_SPRITE_RENDERER").is_ok_and(|value| value == "cpu");
    let output = output_directory()?;
    std::fs::create_dir_all(&output)
        .with_context(|| format!("cannot create {}", output.display()))?;

    let chrome = Chrome::launch(Launch {
        args: Vec::new(),
        profile: Some(output.join("profile")),
        window: Some((1280, height)),
    })?;
    let tab = &chrome.tab;
    chrome::set_viewport(tab, 1280, height, scale)?;
    let patch_failure = patch_launcher_responses(tab, software_sprites)?;
    chrome::call::<Network::Enable>(tab, json!({}))?;
    tab.call_method(Network::ClearBrowserCache(None))?;
    tab.call_method(Network::SetCacheDisabled {
        cache_disabled: true,
    })?;
    chrome::add_init_script(tab, FRAME_COUNTER)?;
    let mut transcript = Transcript {
        events: chrome::page_events(tab)?,
        lines: Vec::new(),
        errors: Vec::new(),
    };

    let smoke = Smoke {
        tab,
        output: &output,
        scale,
        patch_failure: &patch_failure,
    };
    let result = smoke.run(url, &mut transcript);
    transcript.collect();
    let log = output.join("client.log");
    std::fs::write(&log, transcript.lines.join("\n") + "\n")
        .with_context(|| format!("cannot write {}", log.display()))?;
    match result {
        Ok(()) => {
            println!(
                "Client passed startup, typing, resize, fullscreen, shutdown and settings reload checks."
            );
            let notable = Regex::new(r"renderer|effects:|xBRZ:|ERROR|Error|error")?;
            let notable: Vec<&str> = transcript
                .lines
                .iter()
                .map(String::as_str)
                .filter(|line| notable.is_match(line))
                .collect();
            println!("{}", notable[notable.len().saturating_sub(20)..].join("\n"));
            Ok(())
        }
        Err(error) => {
            let recent = &transcript.lines[transcript.lines.len().saturating_sub(25)..];
            eprintln!("{}", recent.join("\n"));
            Err(error)
        }
    }
}

fn env_value<T: std::str::FromStr>(name: &str, default: T) -> Result<T> {
    match env::var(name) {
        Ok(value) => value
            .parse()
            .map_err(|_| anyhow!("{name}={value} is not a valid number")),
        Err(_) => Ok(default),
    }
}

/// `build/web-smoke` in the checkout this tool belongs to, wherever it is run from.
fn output_directory() -> Result<PathBuf> {
    Path::new(env!("CARGO_MANIFEST_DIR"))
        .ancestors()
        .nth(3)
        .map(|root| root.join("build").join("web-smoke"))
        .context("the crate is not inside the client checkout")
}

/// The page's console and uncaught errors, collected as the test goes.
struct Transcript {
    events: Receiver<PageEvent>,
    lines: Vec<String>,
    errors: Vec<String>,
}

impl Transcript {
    fn collect(&mut self) {
        while let Ok(event) = self.events.try_recv() {
            match event {
                PageEvent::Console(line) => self.lines.push(line),
                PageEvent::Error(message) => {
                    eprintln!("{message}");
                    self.lines.push(format!("PAGE ERROR: {message}"));
                    self.errors.push(message);
                }
            }
        }
    }
}

/// The test reads the game's file system through `window.testClient`, and a
/// CPU-renderer run sets the renderer before the module starts. Both are
/// patched into `launcher.mjs` as it is served, so the shipped launcher needs
/// no test hooks.
fn patch_launcher(source: &str, software_sprites: bool) -> Result<String> {
    let mut patched = replace_once(
        source,
        "client.FS.mkdirTree('/UserSet');",
        "window.testClient = client; client.FS.mkdirTree('/UserSet');",
    )?;
    if software_sprites {
        patched = replace_once(
            &patched,
            "preRun: [module => {",
            "preRun: [module => { module.ENV.DARKEDEN_SPRITE_RENDERER = \"software\";",
        )?;
    }
    Ok(patched)
}

fn replace_once(source: &str, from: &str, to: &str) -> Result<String> {
    ensure!(
        source.contains(from),
        "launcher.mjs no longer contains `{from}`; update the client smoke test"
    );
    Ok(source.replacen(from, to, 1))
}

/// Intercepts every `launcher.mjs` response and serves the patched script.
/// Returns where a failure to patch is reported: the interceptor runs on the
/// DevTools event thread and cannot fail the test itself.
fn patch_launcher_responses(
    tab: &Tab,
    software_sprites: bool,
) -> Result<Arc<Mutex<Option<String>>>> {
    let failure = Arc::new(Mutex::new(None));
    let report = Arc::clone(&failure);
    tab.enable_fetch(
        Some(&[RequestPattern {
            url_pattern: Some("*/launcher.mjs".to_owned()),
            resource_Type: None,
            request_stage: Some(RequestStage::Response),
        }]),
        None,
    )?;
    tab.enable_request_interception(Arc::new(
        move |transport: Arc<Transport>, session: SessionId, event: RequestPausedEvent| {
            match patched_response(&transport, session, &event.params, software_sprites) {
                Ok(response) => RequestPausedDecision::Fulfill(response),
                Err(error) => {
                    if let Ok(mut failure) = report.lock() {
                        failure.get_or_insert(format!("{error:#}"));
                    }
                    RequestPausedDecision::Continue(None)
                }
            }
        },
    ))?;
    Ok(failure)
}

fn patched_response(
    transport: &Transport,
    session: SessionId,
    paused: &RequestPausedEventParams,
    software_sprites: bool,
) -> Result<FulfillRequest> {
    let status = paused.response_status_code.unwrap_or(0);
    ensure!(
        status == 200,
        "launcher.mjs was served with HTTP status {status}"
    );
    let body = transport.call_method_on_target(
        session,
        GetResponseBody {
            request_id: paused.request_id.clone(),
        },
    )?;
    let source = if body.base_64_encoded {
        String::from_utf8(BASE64_STANDARD.decode(body.body)?)?
    } else {
        body.body
    };
    let patched = patch_launcher(&source, software_sprites)?;
    // The body changes length; let Chrome frame the new one.
    let headers = paused
        .response_headers
        .iter()
        .flatten()
        .filter(|header| {
            !header.name.eq_ignore_ascii_case("content-length")
                && !header.name.eq_ignore_ascii_case("content-encoding")
        })
        .cloned()
        .collect();
    Ok(FulfillRequest {
        request_id: paused.request_id.clone(),
        response_code: status,
        response_headers: Some(headers),
        binary_response_headers: None,
        body: Some(BASE64_STANDARD.encode(patched)),
        response_phrase: None,
    })
}

struct Smoke<'a> {
    tab: &'a Arc<Tab>,
    output: &'a Path,
    scale: f64,
    patch_failure: &'a Mutex<Option<String>>,
}

impl Smoke<'_> {
    fn run(&self, url: &str, transcript: &mut Transcript) -> Result<()> {
        let tab = self.tab;
        self.load(Some(url))?;
        chrome::click_element(tab, "#play")?;
        chrome::wait_for(
            tab,
            "document.querySelector('#play').textContent === 'Play' ||
             document.querySelector('#status').textContent.includes('Reload')",
            LOAD_TIMEOUT,
            "the game data to load",
        )?;
        let status = self.status()?;
        ensure!(status == "Ready to play.", "{status}");

        chrome::eval(tab, "window.browserFrames = 0")?;
        chrome::click_element(tab, "#play")?;
        chrome::wait_for(
            tab,
            "window.browserFrames >= 120",
            Duration::from_secs(120),
            "120 animation frames",
        )?;
        // The original title overlay fades for two seconds before exposing the menu.
        thread::sleep(Duration::from_secs(3));
        self.check_canvas_size()?;
        let status = self.status()?;
        ensure!(status.is_empty(), "{status}");
        transcript.collect();
        let failure = Regex::new(
            r"(?i)Rejected pack index|file open failed|RuntimeError|TypeError|Aborted\(",
        )?;
        ensure!(
            !transcript.lines.iter().any(|line| failure.is_match(line)),
            "The game reported a resource or runtime failure."
        );
        self.screenshot("client.png")?;

        self.click_game(714.0, 380.0)?;
        let login = self.screenshot("login.png")?;
        chrome::type_text(tab, "browserprobe", Duration::from_millis(60))?;
        thread::sleep(Duration::from_millis(400));
        let typed = self.screenshot("login-text.png")?;
        ensure!(login != typed, "Typing must update the login field");
        self.click_game(452.0, 188.0)?;

        chrome::click_element(tab, "#fullscreen")?;
        chrome::wait_for(
            tab,
            "document.fullscreenElement === document.querySelector('canvas')",
            WAIT_TIMEOUT,
            "fullscreen",
        )?;
        thread::sleep(Duration::from_millis(400));
        self.check_canvas_size()?;
        self.screenshot("fullscreen.png")?;
        chrome::eval(tab, "document.exitFullscreen()")?;
        chrome::set_viewport(tab, 1000, 780, self.scale)?;
        thread::sleep(Duration::from_millis(400));
        self.check_canvas_size()?;

        self.click_game(714.0, 524.0)?;
        chrome::wait_for(
            tab,
            "document.querySelector('#status').textContent.startsWith('The game has closed.')",
            WAIT_TIMEOUT,
            "the game to close",
        )?;
        self.screenshot("exit.png")?;
        let settings = self.settings()?;
        ensure!(
            !settings.is_empty(),
            "Normal shutdown must save game settings"
        );

        self.load(None)?;
        chrome::click_element(tab, "#play")?;
        chrome::wait_for(
            tab,
            "document.querySelector('#play').textContent === 'Play'",
            LOAD_TIMEOUT,
            "the reloaded game data",
        )?;
        let restored = self.settings()?;
        ensure!(restored == settings, "Settings must survive a page reload");

        transcript.collect();
        ensure!(
            transcript.errors.is_empty(),
            "{}",
            transcript.errors.join("\n")
        );
        Ok(())
    }

    fn load(&self, url: Option<&str>) -> Result<()> {
        chrome::load(self.tab, url)?;
        match self.patch_failure.lock() {
            Ok(failure) => match failure.as_deref() {
                Some(failure) => bail!("cannot patch launcher.mjs: {failure}"),
                None => Ok(()),
            },
            Err(_) => bail!("the launcher interceptor panicked"),
        }
    }

    fn status(&self) -> Result<String> {
        match chrome::eval(self.tab, "document.querySelector('#status').textContent")? {
            Value::String(status) => Ok(status),
            other => bail!("unexpected #status text: {other}"),
        }
    }

    fn check_canvas_size(&self) -> Result<()> {
        let size = chrome::eval(self.tab, CANVAS_SIZE)?;
        ensure!(
            size["width"] == size["expectedWidth"],
            "Canvas width: {size}"
        );
        ensure!(
            size["height"] == size["expectedHeight"],
            "Canvas height: {size}"
        );
        Ok(())
    }

    /// Clicks a point of the game's 800x600 screen, letterboxed in the canvas.
    fn click_game(&self, x: f64, y: f64) -> Result<()> {
        let rect: [f64; 4] = serde_json::from_value(chrome::eval(
            self.tab,
            "(() => { const r = document.querySelector('canvas').getBoundingClientRect();
               return [r.x, r.y, r.width, r.height]; })()",
        )?)?;
        let [left, top, width, height] = rect;
        let scale = (width / 800.0).min(height / 600.0);
        chrome::click_at(
            self.tab,
            left + (width - 800.0 * scale) / 2.0 + x * scale,
            top + (height - 600.0 * scale) / 2.0 + y * scale,
            Duration::from_millis(100),
        )?;
        thread::sleep(Duration::from_millis(400));
        Ok(())
    }

    fn settings(&self) -> Result<Vec<u8>> {
        let script = format!("Array.from(window.testClient.FS.readFile('{SETTINGS_FILE}'))");
        serde_json::from_value(chrome::eval(self.tab, &script)?)
            .with_context(|| format!("cannot read {SETTINGS_FILE}"))
    }

    fn screenshot(&self, name: &str) -> Result<Vec<u8>> {
        chrome::screenshot(self.tab, &self.output.join(name))
    }
}

#[cfg(test)]
mod tests {
    use super::patch_launcher;

    const LAUNCHER: &str = include_str!("../../../../web/launcher.mjs");

    #[test]
    fn the_shipped_launcher_can_be_patched() {
        let patched = patch_launcher(LAUNCHER, false).unwrap();
        assert!(patched.contains("window.testClient = client;"));
        assert!(!patched.contains("DARKEDEN_SPRITE_RENDERER"));
        let software = patch_launcher(LAUNCHER, true).unwrap();
        assert!(software.contains("module.ENV.DARKEDEN_SPRITE_RENDERER = \"software\";"));
    }

    #[test]
    fn a_changed_launcher_is_reported() {
        assert!(patch_launcher("export {};", false).is_err());
    }
}
