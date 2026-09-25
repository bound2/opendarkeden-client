// Pointer events own touch/pen input; SDL continues to own real mouse input.
// Synthetic mouse events use CSS coordinates so SDL applies exactly the same
// high-DPI and letterbox transform as it does for a desktop mouse.
export function installTouchControls(client, canvas, shell) {
  const controls = document.querySelector('#touch-controls');
  const toggle = document.querySelector('#touch-toggle');
  const entry = document.querySelector('#text-entry');
  const input = document.querySelector('#text-value');
  const message = document.querySelector('#text-message');
  let enabled = false;
  let stopped = false;
  let mode = 'left';
  let pointer;
  let target = 0;
  let textLimit = 0;
  const held = new Map();
  const latched = new Set();
  const key = (code, down) => client._dxlib_input_virtual_key(code, Number(down));

  function mouse(type, point, button = 0) {
    canvas.dispatchEvent(new MouseEvent(type, {
      bubbles: true, cancelable: true, view: window,
      clientX: point.x, clientY: point.y, button,
      buttons: type === 'mouseup' ? 0 : pointer?.button === 2 ? 2 : pointer?.button === 0 ? 1 : 0,
    }));
  }

  function endPointer() {
    if (!pointer) return;
    const previous = pointer;
    pointer = undefined;
    if (previous.button !== null) mouse('mouseup', previous, previous.button);
    if (canvas.hasPointerCapture(previous.id)) canvas.releasePointerCapture(previous.id);
  }

  function reset() {
    endPointer();
    held.clear();
    latched.clear();
    if (!stopped) client._dxlib_input_virtual_reset();
    controls.querySelectorAll('[data-toggle-key]').forEach(button => button.setAttribute('aria-pressed', 'false'));
  }

  function setEnabled(value) {
    reset();
    enabled = value;
    controls.hidden = !value;
    toggle.setAttribute('aria-pressed', String(value));
  }
  toggle.addEventListener('click', () => setEnabled(!enabled));
  setEnabled(navigator.maxTouchPoints > 0 || matchMedia('(any-pointer: coarse)').matches);

  // Suppress SDL's separate touch-to-mouse path and browser compatibility
  // mouse events, which would otherwise turn one tap into two clicks.
  for (const type of ['touchstart', 'touchmove', 'touchend', 'touchcancel']) {
    canvas.addEventListener(type, event => {
      event.preventDefault();
      event.stopImmediatePropagation();
    }, { capture: true, passive: false });
  }

  canvas.addEventListener('pointerdown', event => {
    if (event.pointerType === 'mouse' || stopped) return;
    event.preventDefault();
    if (pointer || !entry.hidden) return;
    canvas.focus({ preventScroll: true });
    pointer = { id: event.pointerId, x: event.clientX, y: event.clientY,
      button: mode === 'hover' ? null : mode === 'right' ? 2 : 0 };
    canvas.setPointerCapture(event.pointerId);
    mouse('mousemove', pointer);
    if (pointer.button !== null) mouse('mousedown', pointer, pointer.button);
  });
  canvas.addEventListener('pointermove', event => {
    if (pointer?.id !== event.pointerId) return;
    event.preventDefault();
    const rect = canvas.getBoundingClientRect();
    pointer.x = Math.max(rect.left, Math.min(rect.right - 1, event.clientX));
    pointer.y = Math.max(rect.top, Math.min(rect.bottom - 1, event.clientY));
    mouse('mousemove', pointer);
  });
  for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) {
    canvas.addEventListener(type, event => {
      if (pointer?.id === event.pointerId) endPointer();
    });
  }

  controls.querySelectorAll('[data-mode]').forEach(button => button.addEventListener('click', () => {
    endPointer();
    mode = button.dataset.mode;
    controls.querySelectorAll('[data-mode]').forEach(other =>
      other.setAttribute('aria-pressed', String(other === button)));
  }));

  const quick = document.querySelector('#quick-keys');
  const extra = document.querySelector('#extra-keys');
  function addKey(container, label, code, latch = false) {
    const button = document.createElement('button');
    button.textContent = label;
    button.dataset[latch ? 'toggleKey' : 'key'] = code;
    if (latch) button.setAttribute('aria-pressed', 'false');
    container.append(button);
  }
  for (let i = 1; i <= 5; ++i) addKey(quick, String(i), i + 1);
  for (let i = 1; i <= 4; ++i) addKey(quick, `F${i}`, 58 + i);
  addKey(extra, 'Shift', 42, true);
  addKey(extra, 'Ctrl', 29, true);
  for (let i = 6; i <= 10; ++i) addKey(extra, String(i % 10), i + 1);
  for (let i = 5; i <= 12; ++i) addKey(extra, `F${i}`, i <= 10 ? 58 + i : 76 + i);
  for (const [letters, start] of [['QWERTYUIOP', 16], ['ASDFGHJKL', 30], ['ZXCVBNM', 44]]) {
    for (let i = 0; i < letters.length; ++i) addKey(extra, letters[i], start + i);
  }
  for (const [label, code] of [['Tab', 15], ['Space', 57], ['Backspace', 14], ['↑', 200], ['↓', 208], ['←', 203], ['→', 205], ['Page up', 201], ['Page down', 209]])
    addKey(extra, label, code);
  for (const [label, delta] of [['Scroll up', 1], ['Scroll down', -1]]) {
    const button = document.createElement('button');
    button.textContent = label;
    button.addEventListener('click', () => canvas.dispatchEvent(new WheelEvent('wheel', {
      bubbles: true, cancelable: true, deltaY: -delta, deltaMode: 1,
    })));
    extra.append(button);
  }
  document.querySelector('#more-keys').addEventListener('click', event => {
    reset();
    extra.hidden = !extra.hidden;
    event.currentTarget.setAttribute('aria-expanded', String(!extra.hidden));
  });
  controls.querySelectorAll('[data-toggle-key]').forEach(button => button.addEventListener('click', () => {
    const code = Number(button.dataset.toggleKey);
    if (latched.has(code)) latched.delete(code); else latched.add(code);
    key(code, latched.has(code));
    button.setAttribute('aria-pressed', String(latched.has(code)));
  }));
  controls.querySelectorAll('[data-key]').forEach(button => {
    const code = Number(button.dataset.key);
    const tapKey = () => { if (!stopped) { key(code, true); key(code, false); } };
    // The expanded keyboard must scroll on an iPad. Its keys activate on
    // click, after the browser decides this was a tap rather than a swipe.
    if (extra.contains(button)) {
      button.addEventListener('click', tapKey);
      return;
    }
    button.addEventListener('pointerdown', event => {
      event.preventDefault();
      if (stopped) return;
      button.setPointerCapture(event.pointerId);
      held.set(event.pointerId, code);
      key(code, true);
    });
    const release = event => {
      if (!held.delete(event.pointerId)) return;
      if (![...held.values()].includes(code)) key(code, false);
    };
    for (const type of ['pointerup', 'pointercancel', 'lostpointercapture']) button.addEventListener(type, release);
    // Keyboard/assistive activation has no pointerdown; pointer clicks already
    // delivered their edges above. Zero-detail click is keyboard activation.
    button.addEventListener('click', event => {
      if (event.detail === 0) tapKey();
    });
  });

  function closeText() {
    input.blur();
    input.value = '';
    entry.hidden = true;
    controls.hidden = !enabled;
    canvas.focus({ preventScroll: true });
  }
  document.querySelector('#keyboard').addEventListener('click', () => {
    reset();
    target = client._darkeden_text_target();
    if (!target) {
      document.querySelector('#touch-help').textContent = 'Tap a text field first, then Keyboard. For chat, tap Enter first.';
      return;
    }
    input.type = client._darkeden_text_password() ? 'password' : 'text';
    textLimit = client._darkeden_text_limit();
    input.value = client.ccall('darkeden_text_value', 'string', [], []);
    message.textContent = '';
    entry.hidden = false;
    controls.hidden = true;
    // Keep focus synchronous with the user's gesture: iPadOS will not open
    // its software keyboard from an animation-frame or a timer callback.
    input.focus({ preventScroll: true });
    input.setSelectionRange(input.value.length, input.value.length);
  });
  // SDL listens on window. Install before callMain so typing in the DOM editor
  // (including IME and paste) cannot also reach the game's focused editor.
  // Let keyup reach SDL: a hardware key held before this editor opened still
  // needs its release, or it would remain held when the player returns.
  for (const type of ['keydown', 'keypress']) {
    window.addEventListener(type, event => {
      if (entry.hidden || !entry.contains(event.target)) return;
      event.stopImmediatePropagation();
      if (type === 'keydown' && event.key === 'Escape') { event.preventDefault(); closeText(); }
    }, true);
  }
  entry.addEventListener('submit', event => {
    event.preventDefault();
    if ([...input.value].length > textLimit) {
      message.textContent = `Use at most ${textLimit} characters.`;
      return;
    }
    if (client.ccall('darkeden_text_replace', 'number', ['number', 'string'], [target, input.value])) closeText();
    else { input.value = ''; message.textContent = 'The selected field changed. Cancel and select the field again.'; }
  });
  document.querySelector('#text-cancel').addEventListener('click', closeText);
  window.addEventListener('blur', reset);
  window.addEventListener('pagehide', reset);
  document.addEventListener('visibilitychange', () => { if (document.hidden) reset(); });
  window.addEventListener('resize', reset);
  document.addEventListener('fullscreenchange', reset);
  return {
    stop() {
      stopped = true;
      reset();
      input.value = '';
      shell.querySelectorAll('button:not(#fullscreen), input').forEach(control => { control.disabled = true; });
    },
  };
}
