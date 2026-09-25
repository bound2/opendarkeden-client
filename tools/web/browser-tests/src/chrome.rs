//! Chrome launch and the small DevTools protocol toolkit shared by the checks.

use std::env;
use std::error::Error;
use std::ffi::OsStr;
use std::fmt::{self, Debug, Display};
use std::path::{Path, PathBuf};
use std::sync::mpsc::{self, Receiver};
use std::sync::Arc;
use std::thread;
use std::time::{Duration, Instant};

use anyhow::{anyhow, bail, Context, Result};
use base64::prelude::{Engine, BASE64_STANDARD};
use headless_chrome::protocol::cdp::types::{Event, Method};
use headless_chrome::protocol::cdp::{Emulation, Input, Page, Runtime};
use headless_chrome::{Browser, LaunchOptions, Tab};
use serde::de::DeserializeOwned;
use serde::Serialize;
use serde_json::{json, Value};

/// headless_chrome drops the DevTools connection once its browser-level event
/// loop has seen no target event for this long, and browser-level events are
/// rare. It must therefore outlast a whole check (the renderer waits up to
/// 15 minutes); stuck pages are caught by `CALL_TIMEOUT` instead. It also
/// bounds how long a browser that stops answering can hold the process at
/// exit, so it is no longer than the longest check needs.
const IDLE_TIMEOUT: Duration = Duration::from_secs(16 * 60);
/// The longest a single protocol call may take. Calls into a page wait for its
/// main thread, which a stuck WebAssembly module can block indefinitely.
pub const CALL_TIMEOUT: Duration = Duration::from_secs(60);
const POLL_INTERVAL: Duration = Duration::from_millis(100);

/// Browser processes to try, in order, when `WEB_TEST_BROWSER` and `CHROME`
/// are unset.
const BROWSER_NAMES: &[&str] = &[
    "google-chrome",
    "google-chrome-stable",
    "chromium",
    "chromium-browser",
];

#[cfg(windows)]
const BROWSER_PATHS: &[&str] = &[
    "C:/Program Files/Google/Chrome/Application/chrome.exe",
    "C:/Program Files (x86)/Google/Chrome/Application/chrome.exe",
];
#[cfg(target_os = "macos")]
const BROWSER_PATHS: &[&str] = &["/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"];
#[cfg(not(any(windows, target_os = "macos")))]
const BROWSER_PATHS: &[&str] = &[];

pub struct Launch {
    /// Additional Chrome switches.
    pub args: Vec<String>,
    /// A persistent profile; a temporary one is used when unset.
    pub profile: Option<PathBuf>,
    /// Initial window size in CSS pixels.
    pub window: Option<(u32, u32)>,
}

/// A running headless Chrome with one page. Chrome is killed when this drops.
pub struct Chrome {
    // Owns the browser process and connection that `tab` talks through.
    _browser: Browser,
    pub tab: Arc<Tab>,
}

impl Chrome {
    pub fn launch(launch: Launch) -> Result<Self> {
        let executable = find_browser()?;
        let mut args = vec![
            "--headless=new".to_owned(),
            "--hide-scrollbars".to_owned(),
            "--mute-audio".to_owned(),
            // Let WebGL fall back to SwiftShader where there is no usable
            // GPU (containers, VMs), as Playwright always did; a GPU is
            // still preferred when present.
            "--enable-unsafe-swiftshader".to_owned(),
        ];
        args.extend(launch.args);
        let options = LaunchOptions {
            // `headless` would add the legacy `--headless` switch; the new
            // headless mode is requested explicitly above.
            headless: false,
            sandbox: !needs_no_sandbox(),
            enable_gpu: true,
            ignore_certificate_errors: false,
            window_size: launch.window,
            path: Some(executable.clone()),
            user_data_dir: launch.profile,
            args: args.iter().map(OsStr::new).collect(),
            idle_browser_timeout: IDLE_TIMEOUT,
            ..LaunchOptions::default()
        };
        let browser = Browser::new(options)
            .with_context(|| format!("cannot start {}", executable.display()))?;
        let tab = browser.new_tab().context("cannot open a browser tab")?;
        Ok(Self {
            _browser: browser,
            tab,
        })
    }
}

fn find_browser() -> Result<PathBuf> {
    if let Some(name) = env::var_os("WEB_TEST_BROWSER").filter(|name| !name.is_empty()) {
        let path = PathBuf::from(&name);
        if path.is_file() {
            return Ok(path);
        }
        return search_path(&name)
            .ok_or_else(|| anyhow!("WEB_TEST_BROWSER={} is not an executable", path.display()));
    }
    if let Some(path) = env::var_os("CHROME").map(PathBuf::from) {
        if path.is_file() {
            return Ok(path);
        }
    }
    BROWSER_NAMES
        .iter()
        .find_map(|name| search_path(OsStr::new(name)))
        .or_else(|| {
            BROWSER_PATHS
                .iter()
                .map(PathBuf::from)
                .find(|path| path.is_file())
        })
        .ok_or_else(|| {
            anyhow!("no Chrome or Chromium found; set WEB_TEST_BROWSER to the browser executable")
        })
}

fn search_path(name: &OsStr) -> Option<PathBuf> {
    let paths = env::var_os("PATH")?;
    env::split_paths(&paths).find_map(|directory| {
        let candidate = directory.join(name);
        if candidate.is_file() {
            return Some(candidate);
        }
        let candidate = candidate.with_extension(env::consts::EXE_EXTENSION);
        candidate.is_file().then_some(candidate)
    })
}

/// Chrome refuses to start its sandbox as root (containers), and CI runners
/// may not permit the user namespaces it needs.
fn needs_no_sandbox() -> bool {
    if env::var_os("CI").is_some() {
        return true;
    }
    #[cfg(unix)]
    {
        use std::os::unix::fs::MetadataExt;
        if let Ok(process) = std::fs::metadata("/proc/self") {
            return process.uid() == 0;
        }
    }
    false
}

/// Something the page reported.
pub enum PageEvent {
    /// Text of a `console.*` call.
    Console(String),
    /// An uncaught exception or unhandled promise rejection.
    Error(String),
}

/// Starts forwarding console output and uncaught errors of `tab`.
pub fn page_events(tab: &Tab) -> Result<Receiver<PageEvent>> {
    let (sender, receiver) = mpsc::channel();
    tab.add_event_listener(Arc::new(move |event: &Event| {
        let forwarded = match event {
            Event::RuntimeConsoleAPICalled(call) => {
                let text: Vec<String> = call.params.args.iter().map(remote_text).collect();
                PageEvent::Console(text.join(" "))
            }
            Event::RuntimeExceptionThrown(thrown) => {
                PageEvent::Error(exception_message(&thrown.params.exception_details))
            }
            // Browser-side log entries: failed resource loads, deprecation
            // and WebGL warnings, refused WebSocket connections.
            Event::LogEntryAdded(added) => PageEvent::Console(added.params.entry.text.clone()),
            _ => return,
        };
        // The receiver is gone once the check has finished; nothing to do.
        let _ = sender.send(forwarded);
    }))?;
    tab.enable_runtime()?;
    tab.enable_log()?;
    Ok(receiver)
}

fn remote_text(object: &Runtime::RemoteObject) -> String {
    match &object.value {
        Some(Value::String(text)) => text.clone(),
        Some(value) => value.to_string(),
        None => object
            .unserializable_value
            .clone()
            .or_else(|| object.description.clone())
            .unwrap_or_else(|| format!("{:?}", object.Type).to_lowercase()),
    }
}

fn exception_message(details: &Runtime::ExceptionDetails) -> String {
    let text = details
        .exception
        .as_ref()
        .map(remote_text)
        .unwrap_or_else(|| details.text.clone());
    // Error descriptions carry the stack after the first line.
    text.lines().next().unwrap_or_default().to_owned()
}

/// A protocol call that got no answer in time: the page is not responding.
#[derive(Debug)]
pub struct NoAnswer {
    method: &'static str,
    timeout: Duration,
}

impl Display for NoAnswer {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            formatter,
            "{} got no answer within {} s",
            self.method,
            self.timeout.as_secs()
        )
    }
}

impl Error for NoAnswer {}

/// Calls a DevTools method whose parameters are given in protocol JSON form
/// (most methods have many optional fields), failing with [`NoAnswer`] after
/// `timeout`. The call runs on a helper thread so that a page stuck in
/// WebAssembly cannot hold the check past its deadline.
pub fn call_within<M>(tab: &Arc<Tab>, params: Value, timeout: Duration) -> Result<M::ReturnObject>
where
    M: Method + Serialize + DeserializeOwned + Debug + Send + 'static,
    M::ReturnObject: Send + 'static,
{
    let method: M = serde_json::from_value(params)
        .with_context(|| format!("invalid parameters for {}", M::NAME))?;
    let (sender, receiver) = mpsc::channel();
    let tab = Arc::clone(tab);
    thread::spawn(move || {
        // The receiver is gone if the call timed out.
        let _ = sender.send(tab.call_method(method));
    });
    match receiver.recv_timeout(timeout) {
        Ok(result) => result.with_context(|| format!("{} failed", M::NAME)),
        Err(_) => Err(NoAnswer {
            method: M::NAME,
            timeout,
        }
        .into()),
    }
}

/// [`call_within`] with the default [`CALL_TIMEOUT`].
pub fn call<M>(tab: &Arc<Tab>, params: Value) -> Result<M::ReturnObject>
where
    M: Method + Serialize + DeserializeOwned + Debug + Send + 'static,
    M::ReturnObject: Send + 'static,
{
    call_within::<M>(tab, params, CALL_TIMEOUT)
}

/// Evaluates `expression` in the page, awaits a returned promise, and returns
/// the JSON value of the result. A thrown exception becomes an error.
pub fn eval_within(tab: &Arc<Tab>, expression: &str, timeout: Duration) -> Result<Value> {
    let evaluated = call_within::<Runtime::Evaluate>(
        tab,
        json!({ "expression": expression, "returnByValue": true, "awaitPromise": true }),
        timeout,
    )?;
    if let Some(details) = evaluated.exception_details {
        bail!("page script failed: {}", exception_message(&details));
    }
    Ok(evaluated.result.value.unwrap_or(Value::Null))
}

/// [`eval_within`] with the default [`CALL_TIMEOUT`].
pub fn eval(tab: &Arc<Tab>, expression: &str) -> Result<Value> {
    eval_within(tab, expression, CALL_TIMEOUT)
}

/// Polls `expression` until it evaluates to `true`.
pub fn wait_for(tab: &Arc<Tab>, expression: &str, timeout: Duration, what: &str) -> Result<()> {
    let deadline = Instant::now() + timeout;
    loop {
        if eval(tab, expression)? == Value::Bool(true) {
            return Ok(());
        }
        if Instant::now() >= deadline {
            bail!("timed out after {} s waiting for {what}", timeout.as_secs());
        }
        thread::sleep(POLL_INTERVAL);
    }
}

/// Runs `source` at the start of every document the tab loads.
pub fn add_init_script(tab: &Arc<Tab>, source: &str) -> Result<()> {
    call::<Page::AddScriptToEvaluateOnNewDocument>(tab, json!({ "source": source }))?;
    Ok(())
}

/// Starts loading `url` without waiting for it.
pub fn navigate(tab: &Arc<Tab>, url: &str) -> Result<()> {
    let navigated = call::<Page::Navigate>(tab, json!({ "url": url }))?;
    match navigated.error_text {
        Some(error) => bail!("cannot open {url}: {error}"),
        None => Ok(()),
    }
}

/// Loads `url` (or reloads the page when `url` is `None`) and waits until the
/// new document and its scripts have loaded.
pub fn load(tab: &Arc<Tab>, url: Option<&str>) -> Result<()> {
    // Mark the current document so the wait cannot mistake it for the new one.
    eval(tab, "window.__browserTestsStale = true")?;
    match url {
        Some(url) => navigate(tab, url)?,
        None => {
            call::<Page::Reload>(tab, json!({}))?;
        }
    }
    wait_for(
        tab,
        "!window.__browserTestsStale && document.readyState === 'complete'",
        Duration::from_secs(30),
        "the page to load",
    )
}

/// Sets the emulated viewport in CSS pixels.
pub fn set_viewport(
    tab: &Arc<Tab>,
    width: u32,
    height: u32,
    device_scale_factor: f64,
) -> Result<()> {
    call::<Emulation::SetDeviceMetricsOverride>(
        tab,
        json!({
            "width": width,
            "height": height,
            "deviceScaleFactor": device_scale_factor,
            "mobile": false,
        }),
    )?;
    Ok(())
}

fn mouse(tab: &Arc<Tab>, kind: &str, x: f64, y: f64) -> Result<()> {
    let (button, buttons) = match kind {
        "mouseMoved" => ("none", 0),
        "mousePressed" => ("left", 1),
        _ => ("left", 0),
    };
    call::<Input::DispatchMouseEvent>(
        tab,
        json!({
            "type": kind,
            "x": x,
            "y": y,
            "button": button,
            "buttons": buttons,
            "clickCount": 1,
        }),
    )?;
    Ok(())
}

/// Clicks the left button at viewport coordinates, holding it for `hold`.
pub fn click_at(tab: &Arc<Tab>, x: f64, y: f64, hold: Duration) -> Result<()> {
    mouse(tab, "mouseMoved", x, y)?;
    mouse(tab, "mousePressed", x, y)?;
    thread::sleep(hold);
    mouse(tab, "mouseReleased", x, y)
}

/// Clicks the centre of the element matching `selector`.
pub fn click_element(tab: &Arc<Tab>, selector: &str) -> Result<()> {
    let script = format!(
        "(() => {{
          const element = document.querySelector({selector});
          if (!element) return null;
          element.scrollIntoView({{ block: 'center', inline: 'center' }});
          const rect = element.getBoundingClientRect();
          return [rect.x + rect.width / 2, rect.y + rect.height / 2, rect.width * rect.height];
        }})()",
        selector = serde_json::to_string(selector)?
    );
    let centre: Option<(f64, f64, f64)> = serde_json::from_value(eval(tab, &script)?)?;
    match centre {
        None => bail!("no element matches {selector}"),
        Some((_, _, area)) if area <= 0.0 => bail!("{selector} is not visible"),
        Some((x, y, _)) => click_at(tab, x, y, Duration::ZERO),
    }
}

/// Types `text` key by key, holding each key for `hold`.
pub fn type_text(tab: &Arc<Tab>, text: &str, hold: Duration) -> Result<()> {
    for character in text.chars() {
        let (code, key_code) = match character {
            'a'..='z' => (
                format!("Key{}", character.to_ascii_uppercase()),
                u32::from(character.to_ascii_uppercase()),
            ),
            '0'..='9' => (format!("Digit{character}"), u32::from(character)),
            _ => bail!("typing {character:?} is not supported"),
        };
        let key = character.to_string();
        let event = |kind: &str, text: Option<&str>| {
            json!({
                "type": kind,
                "key": key,
                "code": code,
                "text": text,
                "windowsVirtualKeyCode": key_code,
                "nativeVirtualKeyCode": key_code,
            })
        };
        call::<Input::DispatchKeyEvent>(tab, event("keyDown", Some(&key)))?;
        thread::sleep(hold);
        call::<Input::DispatchKeyEvent>(tab, event("keyUp", None))?;
    }
    Ok(())
}

/// Captures the viewport as PNG, writes it to `path`, and returns the bytes.
pub fn screenshot(tab: &Arc<Tab>, path: &Path) -> Result<Vec<u8>> {
    let captured =
        call::<Page::CaptureScreenshot>(tab, json!({ "format": "png", "fromSurface": true }))?;
    let png = BASE64_STANDARD
        .decode(captured.data)
        .context("the screenshot is not valid base64")?;
    std::fs::write(path, &png).with_context(|| format!("cannot write {}", path.display()))?;
    Ok(png)
}
