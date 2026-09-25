//! Browser-generated touches exercise the shipped DOM controls. The module's
//! game callbacks are recorded here; client-touch covers the actual WASM/UI.
use crate::{
    chrome::{self, Chrome, Launch},
    Usage,
};
use anyhow::{ensure, Result};
use headless_chrome::{
    protocol::cdp::{Emulation, Input},
    Tab,
};
use serde_json::{json, Value};
use std::{sync::Arc, thread, time::Duration};

pub fn enable(tab: &Arc<Tab>) -> Result<()> {
    chrome::call::<Emulation::SetTouchEmulationEnabled>(
        tab,
        json!({"enabled": true, "maxTouchPoints": 5}),
    )?;
    Ok(())
}

pub fn points(tab: &Arc<Tab>, kind: &str, points: &[(i32, f64, f64)]) -> Result<()> {
    let points: Vec<_> = points
        .iter()
        .map(|(id, x, y)| json!({"id": id, "x": x, "y": y}))
        .collect();
    chrome::call::<Input::DispatchTouchEvent>(tab, json!({"type": kind, "touchPoints": points}))?;
    Ok(())
}

pub fn tap_at(tab: &Arc<Tab>, x: f64, y: f64) -> Result<()> {
    points(tab, "touchStart", &[(1, x, y)])?;
    points(tab, "touchEnd", &[])?;
    thread::sleep(Duration::from_millis(80));
    Ok(())
}

pub fn centre(tab: &Arc<Tab>, selector: &str) -> Result<(f64, f64)> {
    let selector = serde_json::to_string(selector)?;
    let position = chrome::eval(tab, &format!("(() => {{ const e = document.querySelector({selector}); e.scrollIntoView({{block:'nearest'}}); const r=e.getBoundingClientRect(); if (!r.width || !r.height) throw Error('hidden control'); return [r.x+r.width/2,r.y+r.height/2]; }})()"))?;
    Ok(serde_json::from_value(position)?)
}

pub fn tap(tab: &Arc<Tab>, selector: &str) -> Result<()> {
    let (x, y) = centre(tab, selector)?;
    tap_at(tab, x, y)
}

fn check(tab: &Arc<Tab>, condition: &str) -> Result<()> {
    ensure!(
        chrome::eval(tab, condition)? == Value::Bool(true),
        "touch check failed: {condition}; events: {}",
        chrome::eval(tab, "({mouseEvents, keyEvents})")?
    );
    Ok(())
}

pub fn run(args: &[String]) -> Result<()> {
    let url = match args {
        [] => "http://127.0.0.1:18739/",
        [url] => url,
        _ => return Err(Usage("touch takes at most one URL").into()),
    };
    let chrome = Chrome::launch(Launch {
        args: vec![],
        profile: None,
        window: Some((1024, 768)),
    })?;
    let tab = &chrome.tab;
    chrome::set_viewport(tab, 1024, 768, 2.0)?;
    enable(tab)?;
    let events = chrome::page_events(tab)?;
    chrome::load(tab, Some(url))?;
    check(
        tab,
        "document.querySelector('#game-shell').getBoundingClientRect().height === 0",
    )?;
    chrome::eval(
        tab,
        r"(async () => {
      const {installTouchControls} = await import('./touch-controls.mjs');
      window.mouseEvents = []; window.keyEvents = []; window.field = 'old'; window.fieldTarget = 1;
      const client = {
        _dxlib_input_virtual_key: (code, down) => keyEvents.push([code, down]),
        _dxlib_input_virtual_reset: () => keyEvents.push(['reset']),
        _darkeden_text_target: () => fieldTarget,
        _darkeden_text_password: () => 1,
        _darkeden_text_limit: () => 20,
        ccall: (name, result, types, args) => {
          if (name === 'darkeden_text_value') return field;
          if (args[0] !== fieldTarget) return 0;
          field = args[1]; return 1;
        }
      };
      const canvas = document.querySelector('canvas'), shell = document.querySelector('#game-shell');
      canvas.hidden = shell.hidden = false; document.body.classList.add('playing');
      for (const type of ['mousedown', 'mouseup', 'mousemove']) canvas.addEventListener(type, e => mouseEvents.push([type, e.button, e.clientX, e.clientY]));
      window.touchFixture = installTouchControls(client, canvas, shell);
    })()",
    )?;
    check(tab, "!document.querySelector('#touch-controls').hidden")?;
    let (x, y) = centre(tab, "canvas")?;

    // One tap, exactly one left click; no SDL touch emulation/compatibility click.
    tap_at(tab, x, y)?;
    check(tab, "mouseEvents.filter(e=>e[0]==='mousedown').length === 1 && mouseEvents.filter(e=>e[0]==='mouseup').length === 1")?;
    // A second finger cannot move the first finger's target or release its hold.
    chrome::eval(tab, "mouseEvents=[]")?;
    points(tab, "touchStart", &[(1, x, y)])?;
    points(tab, "touchStart", &[(1, x, y), (2, x + 80.0, y + 60.0)])?;
    points(tab, "touchEnd", &[(2, x + 80.0, y + 60.0)])?;
    check(tab, "mouseEvents.filter(e=>e[0]==='mouseup').length===0 && mouseEvents.filter(e=>e[0]==='mousedown').length===1")?;
    points(tab, "touchMove", &[(1, x + 40.0, y + 20.0)])?;
    points(tab, "touchEnd", &[])?;
    check(tab, &format!("mouseEvents.at(-1)[0]==='mouseup' && Math.abs(mouseEvents.at(-1)[2]-{})<1 && Math.abs(mouseEvents.at(-1)[3]-{})<1", x+40.0, y+20.0))?;

    tap(tab, "[data-mode=right]")?;
    chrome::eval(tab, "mouseEvents=[]")?;
    tap_at(tab, x, y)?;
    check(tab, "mouseEvents.filter(e=>e[0]==='mousedown').length===1 && mouseEvents.find(e=>e[0]==='mousedown')[1]===2")?;
    tap(tab, "[data-mode=hover]")?;
    chrome::eval(tab, "mouseEvents=[]")?;
    tap_at(tab, x, y)?;
    check(
        tab,
        "mouseEvents.length > 0 && mouseEvents.every(e=>e[0]==='mousemove')",
    )?;

    tap(tab, "[data-mode=left]")?;
    chrome::eval(tab, "mouseEvents=[]; keyEvents=[]")?;
    // A quick-slot tap can be made with the other hand during movement.
    let (kx, ky) = centre(tab, "[data-key='2']")?;
    points(tab, "touchStart", &[(1, x, y)])?;
    points(tab, "touchStart", &[(1, x, y), (2, kx, ky)])?;
    points(tab, "touchEnd", &[(2, kx, ky)])?;
    check(tab, "JSON.stringify(keyEvents)==='[[2,1],[2,0]]' && mouseEvents.filter(e=>e[0]==='mouseup').length===0")?;
    points(tab, "touchCancel", &[])?;
    check(tab, "mouseEvents.filter(e=>e[0]==='mouseup').length===1")?;
    tap(tab, "[data-toggle-key='56']")?;
    check(
        tab,
        "document.querySelector('[data-toggle-key]').getAttribute('aria-pressed')==='true'",
    )?;
    points(tab, "touchStart", &[(1, x, y)])?;
    chrome::eval(tab, "window.dispatchEvent(new Event('blur'))")?;
    points(tab, "touchEnd", &[])?;
    check(tab, "keyEvents.at(-1)[0]==='reset' && document.querySelector('[data-toggle-key]').getAttribute('aria-pressed')==='false'")?;

    // Software keyboard activation, masked passwords, edit/cancel, limits and
    // focus changes. DOM typing must never bubble to SDL's window listener.
    tap(tab, "#keyboard")?;
    check(tab, "document.activeElement.id==='text-value' && document.activeElement.type==='password' && document.activeElement.value==='old'")?;
    chrome::eval(tab, "window.leakedKeys=0; window.releasedKeys=0; window.addEventListener('keydown',()=>++leakedKeys); window.addEventListener('keyup',()=>++releasedKeys); document.querySelector('#text-value').value=''")?;
    chrome::type_text(tab, "touchprobe", Duration::ZERO)?;
    check(tab, "leakedKeys===0 && releasedKeys===10 && field==='old'")?;
    tap(tab, "#text-entry button[type=submit]")?;
    check(tab, "field==='touchprobe' && document.querySelector('#text-entry').hidden && document.querySelector('#text-value').value===''")?;
    tap(tab, "#keyboard")?;
    chrome::eval(
        tab,
        "document.querySelector('#text-value').value='x'.repeat(21)",
    )?;
    tap(tab, "#text-entry button[type=submit]")?;
    check(tab, "field==='touchprobe' && !document.querySelector('#text-entry').hidden && document.querySelector('#text-message').textContent.includes('20')")?;
    chrome::eval(
        tab,
        "fieldTarget=2; document.querySelector('#text-value').value='stale'",
    )?;
    tap(tab, "#text-entry button[type=submit]")?;
    check(tab, "field==='touchprobe' && document.querySelector('#text-message').textContent.includes('changed')")?;
    tap(tab, "#text-cancel")?;
    check(tab, "document.querySelector('#text-value').value===''")?;

    // Portrait layout, extra controls and the non-Fullscreen-API fallback.
    chrome::set_viewport(tab, 768, 1024, 2.0)?;
    tap(tab, "#more-keys")?;
    check(tab, "!document.querySelector('#extra-keys').hidden && document.documentElement.scrollWidth<=innerWidth")?;
    tap(tab, "[data-key='87']")?; // F11 has a non-contiguous DIK code.
    check(
        tab,
        "JSON.stringify(keyEvents.slice(-2))==='[[87,1],[87,0]]'",
    )?;
    chrome::set_viewport(tab, 768, 600, 2.0)?;
    thread::sleep(Duration::from_millis(200));
    chrome::eval(
        tab,
        "document.querySelector('#extra-keys').scrollTop=0; keyEvents=[]",
    )?;
    let (sx, sy) = centre(tab, "#extra-keys [data-key='7']")?;
    points(tab, "touchStart", &[(1, sx, sy)])?;
    for step in 1..=8 {
        points(tab, "touchMove", &[(1, sx, sy - f64::from(step * 10))])?;
        thread::sleep(Duration::from_millis(20));
    }
    points(tab, "touchEnd", &[])?;
    check(
        tab,
        "document.querySelector('#extra-keys').scrollTop>0 && keyEvents.length===0",
    )?;
    chrome::set_viewport(tab, 768, 1024, 2.0)?;
    chrome::eval(tab, "document.querySelector('#game-shell').requestFullscreen=()=>Promise.reject(Error('unavailable'))")?;
    tap(tab, "#fullscreen")?;
    check(tab, "document.querySelector('#game-shell').classList.contains('expanded') && document.querySelector('#touch-controls').getBoundingClientRect().height>0")?;
    tap(tab, "#fullscreen")?;
    check(
        tab,
        "!document.querySelector('#game-shell').classList.contains('expanded')",
    )?;
    chrome::eval(tab, "touchFixture.stop()")?;
    check(tab, "document.querySelector('#keyboard').disabled")?;
    for event in events.try_iter() {
        if let chrome::PageEvent::Error(error) = event {
            anyhow::bail!("{error}");
        }
    }
    println!("Touch controls passed tap, drag, multitouch, right click, hover, shortcuts, cancel/blur, text entry, portrait and fullscreen fallback checks.");
    Ok(())
}
