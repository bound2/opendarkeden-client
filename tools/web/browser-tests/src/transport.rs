//! Runs the WebAssembly socket-adapter probe against gateway fixtures.

use std::sync::mpsc::{self, Receiver, RecvTimeoutError};
use std::sync::Arc;
use std::thread;
use std::time::Duration;

use anyhow::{anyhow, bail, Result};
use headless_chrome::Tab;
use serde_json::Value;

use crate::chrome::{self, Chrome, Launch, NoAnswer, PageEvent};

const DEFAULT_ORIGIN: &str = "http://127.0.0.1:18739";
const DEFAULT_ENDPOINTS: &[&str] = &["ws://127.0.0.1:18740/game", "ws://127.0.0.1:18741/game"];
/// The watchdog runs here rather than in the page: a WebAssembly main thread
/// that blocks cannot run its own JavaScript timeout.
const WATCHDOG: Duration = Duration::from_secs(35);
/// Console output still in flight when the probe exits arrives within this.
const SETTLE: Duration = Duration::from_millis(100);

pub fn run(args: &[String]) -> Result<()> {
    let origin = args.first().map_or(DEFAULT_ORIGIN, String::as_str);
    let endpoints: Vec<&str> = match args.get(1..) {
        Some(endpoints) if !endpoints.is_empty() => endpoints.iter().map(String::as_str).collect(),
        _ => DEFAULT_ENDPOINTS.to_vec(),
    };

    let chrome = Chrome::launch(Launch {
        args: Vec::new(),
        profile: None,
        window: None,
    })?;
    let events = chrome::page_events(&chrome.tab)?;
    for endpoint in endpoints {
        // A fresh document per endpoint gives every probe a new module instance.
        chrome::load(&chrome.tab, Some(origin))?;
        let exit_code = run_probe(&chrome.tab, &events, endpoint)?;
        if exit_code != 0 {
            bail!("Transport probe failed: {endpoint} (exit status {exit_code})");
        }
    }
    Ok(())
}

/// Runs one probe and echoes the page's console while it runs. Returns the
/// probe's exit status.
fn run_probe(tab: &Arc<Tab>, events: &Receiver<PageEvent>, endpoint: &str) -> Result<Value> {
    let script = format!(
        "import('./transport_tests.mjs').then(({{ default: createProbe }}) =>
          new Promise((resolve, reject) => {{
            createProbe({{ arguments: [{endpoint}], print: console.log, printErr: console.error,
              onExit: resolve,
              onAbort: reason => reject(new Error(String(reason))),
            }}).catch(reject);
          }}))",
        endpoint = serde_json::to_string(endpoint)?
    );
    let (sender, outcome) = mpsc::channel();
    let page = Arc::clone(tab);
    thread::spawn(move || {
        let _ = sender.send(chrome::eval_within(&page, &script, WATCHDOG));
    });
    let outcome = loop {
        match outcome.try_recv() {
            Ok(outcome) => break outcome,
            Err(_) => {
                echo(events, SETTLE);
            }
        }
    };
    // Let output that raced the result through before reporting it.
    while echo(events, SETTLE) {}
    outcome.map_err(|error| {
        if error.is::<NoAnswer>() {
            anyhow!("Transport probe did not exit: {endpoint}")
        } else {
            error.context(format!("Transport probe failed: {endpoint}"))
        }
    })
}

/// Prints one page event, waiting up to `timeout` for it. Returns whether
/// there was one.
fn echo(events: &Receiver<PageEvent>, timeout: Duration) -> bool {
    match events.recv_timeout(timeout) {
        Ok(PageEvent::Console(line)) => println!("{line}"),
        Ok(PageEvent::Error(message)) => eprintln!("PAGE ERROR: {message}"),
        Err(RecvTimeoutError::Timeout) => return false,
        Err(RecvTimeoutError::Disconnected) => {
            thread::sleep(timeout);
            return false;
        }
    }
    true
}
