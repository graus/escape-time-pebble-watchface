// Escape Time's settings page. It is a small HTML page opened as a data URL,
// so the face needs no server and no library: the phone keeps the settings
// to show them next time, and sends them to the watch (as the message keys
// in package.json, read by src/c/main.c) when saved.

// Mode: the look, in the order of tools/design.py MODES (FACE_MODE_*).
var MODES = [
  ['Light', 'White digits outlined in black, over bright bands.'],
  ['Dark', 'Black digits outlined in white, over deep blues, purples and embers.']
];
var DEFAULTS = {Mode: 0, NightDark: false, NightStart: 22, NightEnd: 7, Saver: false};

function load() {
  var saved = {};
  try {
    saved = JSON.parse(localStorage.getItem('settings')) || {};
  } catch (e) {
    saved = {};
  }
  var settings = {};
  for (var key in DEFAULTS) {
    settings[key] = key in saved ? saved[key] : DEFAULTS[key];
  }
  if (!MODES[settings.Mode]) {
    settings.Mode = DEFAULTS.Mode;  // not a look this version has
  }
  return settings;
}

function hourOptions(selected) {
  var html = '';
  for (var h = 0; h < 24; h++) {
    html += '<option value="' + h + '"' + (h === selected ? ' selected' : '') + '>' +
            (h < 10 ? '0' : '') + h + ':00</option>';
  }
  return html;
}

function page(settings) {
  var modes = '';
  for (var i = 0; i < MODES.length; i++) {
    modes += '<label class="row"><input type="radio" name="mode" value="' + i + '"' +
             (settings.Mode === i ? ' checked' : '') + '><span><b>' + MODES[i][0] +
             '</b><p>' + MODES[i][1] + '</p></span></label>';
  }
  return '<!DOCTYPE html><html><head><meta charset="utf-8">' +
    '<meta name="viewport" content="width=device-width, initial-scale=1">' +
    '<title>Escape Time</title><style>' +
    'body{margin:0;padding:24px 16px;font:16px -apple-system,Roboto,sans-serif;' +
    'background:#111;color:#eee}' +
    'h1{font-size:22px;margin:0 0 20px}' +
    'h2{font-size:13px;text-transform:uppercase;letter-spacing:.06em;color:#999;' +
    'margin:24px 0 8px}' +
    '.group{border-radius:12px;background:#222;overflow:hidden}' +
    '.row{display:flex;align-items:center;gap:14px;padding:14px 16px}' +
    '.row+.row{border-top:1px solid #333}' +
    '.row span{flex:1}' +
    'input{width:22px;height:22px;flex:none;margin:0}' +
    'p{margin:4px 0 0;color:#aaa;font-size:14px}' +
    'select{font:inherit;padding:6px;border-radius:8px;background:#333;color:#eee;' +
    'border:1px solid #444}' +
    '#hours.off{opacity:.4}' +
    'button{margin-top:24px;width:100%;padding:14px;border:0;border-radius:12px;' +
    'font:inherit;font-weight:600;background:#5af;color:#000}' +
    '</style></head><body>' +
    '<h1>Escape Time</h1>' +
    '<h2>Look</h2><div class="group">' + modes + '</div>' +
    '<h2>Night</h2><div class="group">' +
    '<label class="row"><span><b>Dark at night</b>' +
    '<p>Switch to Dark between these hours, even when Light is chosen.</p></span>' +
    '<input type="checkbox" id="night"' + (settings.NightDark ? ' checked' : '') + '></label>' +
    '<div class="row" id="hours"><span>From</span><select id="start">' +
    hourOptions(settings.NightStart) + '</select><span>to</span><select id="end">' +
    hourOptions(settings.NightEnd) + '</select></div></div>' +
    '<h2>Battery</h2><div class="group">' +
    '<label class="row"><span><b>Battery saver</b>' +
    '<p>Never animate: each minute the new view appears at once, with the time, ' +
    'and a tap leaves the colors still.</p></span>' +
    '<input type="checkbox" id="saver"' + (settings.Saver ? ' checked' : '') + '></label></div>' +
    '<button id="save">Save</button>' +
    '<script>' +
    // The page hands its values back by navigating to pebblejs://close#...,
    // or to the return_to address the emulator (pebble emu-app-config) adds.
    'var m=/[?&]return_to=([^&]*)/.exec(location.search);' +
    'var returnTo=m?decodeURIComponent(m[1]):"pebblejs://close#";' +
    'var $=function(id){return document.getElementById(id);};' +
    'function sync(){$("hours").className="row"+($("night").checked?"":" off");}' +
    '$("night").onchange=sync;sync();' +
    '$("save").onclick=function(){' +
    'var s={Mode:+document.querySelector("input[name=mode]:checked").value,' +
    'NightDark:$("night").checked,NightStart:+$("start").value,NightEnd:+$("end").value,' +
    'Saver:$("saver").checked};' +
    'location.href=returnTo+encodeURIComponent(JSON.stringify(s));};' +
    '</script></body></html>';
}

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL('data:text/html;charset=utf-8,' + encodeURIComponent(page(load())));
});

function toInt(v, min, max, fallback) {
  v = parseInt(v, 10);
  return isNaN(v) || v < min || v > max ? fallback : v;
}

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) {
    return;  // closed without saving
  }
  var saved;
  try {
    saved = JSON.parse(decodeURIComponent(e.response));
  } catch (err) {
    console.log('settings: bad response ' + e.response);
    return;
  }
  var settings = {
    Mode: toInt(saved.Mode, 0, MODES.length - 1, DEFAULTS.Mode),
    NightDark: !!saved.NightDark,
    NightStart: toInt(saved.NightStart, 0, 23, DEFAULTS.NightStart),
    NightEnd: toInt(saved.NightEnd, 0, 23, DEFAULTS.NightEnd),
    Saver: !!saved.Saver
  };
  localStorage.setItem('settings', JSON.stringify(settings));
  Pebble.sendAppMessage({
    Mode: settings.Mode,
    NightDark: settings.NightDark ? 1 : 0,
    NightStart: settings.NightStart,
    NightEnd: settings.NightEnd,
    Saver: settings.Saver ? 1 : 0
  }, function() {
    console.log('settings sent');
  }, function() {
    console.log('settings: the watch did not take them');
  });
});
