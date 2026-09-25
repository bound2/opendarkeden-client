//! Headless Chrome checks for the WebAssembly client: the renderer pixel
//! oracle, the socket-adapter probe and the real-client smoke test.
//!
//! Exit status: 0 when the check passed, 1 when it failed or could not run,
//! 2 for invalid arguments.

mod chrome;
mod client;
mod renderer;
mod transport;

use std::fmt;
use std::process::ExitCode;

const USAGE: &str = "\
usage: browser-tests <command> [arguments]

commands:
  renderer [url]
      WebGL 2 sprite-renderer pixel oracle
      (default http://127.0.0.1:18739/web_sprite_tests.html)
  transport [origin] [endpoint...]
      WebSocket adapter probe against the server's gateway fixture
      (default origin http://127.0.0.1:18739, endpoints
      ws://127.0.0.1:18740/game and ws://127.0.0.1:18741/game)
  client [url]
      real-client smoke test; needs the packaged game data
      (default http://127.0.0.1:18739/)

environment:
  WEB_TEST_BROWSER          Chrome or Chromium executable
                            (default: $CHROME, then PATH, then the usual install paths)
  WEB_TEST_SOFTWARE_GL=1    renderer: SwiftShader instead of the GPU
  WEB_TEST_LOG              renderer: full console log (default web-renderer.log)
  WEB_TEST_HEIGHT           client: viewport height in CSS pixels (default 900)
  WEB_TEST_DPR              client: device pixel ratio (default 1)
  WEB_TEST_SPRITE_RENDERER  client: `cpu` selects software sprite composition
";

/// Invalid command-line arguments.
#[derive(Debug)]
pub struct Usage(pub &'static str);

impl fmt::Display for Usage {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(self.0)
    }
}

impl std::error::Error for Usage {}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let Some((command, rest)) = args.split_first() else {
        eprint!("{USAGE}");
        return ExitCode::from(2);
    };
    let result = match command.as_str() {
        "renderer" => renderer::run(rest),
        "transport" => transport::run(rest),
        "client" => client::run(rest),
        "help" | "-h" | "--help" => {
            print!("{USAGE}");
            return ExitCode::SUCCESS;
        }
        _ => {
            eprint!("unknown command `{command}`\n\n{USAGE}");
            return ExitCode::from(2);
        }
    };
    match result {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) if error.is::<Usage>() => {
            eprint!("{error}\n\n{USAGE}");
            ExitCode::from(2)
        }
        Err(error) => {
            eprintln!("error: {error:#}");
            ExitCode::FAILURE
        }
    }
}
