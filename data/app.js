/*
  app.js - Client-side script for the built-in web UI

  - Polls /ui_state to keep the UI in sync with the device
  - Provides actions to call server endpoints (buttons, apps, diag)
*/

(function () {
  'use strict';

  var lastState = -1;

  async function refresh() {
    try {
      var resp = await fetch('/ui_state');
      var j = await resp.json();
      var s = j.state;
      var busy = j.epdBusy;

      // highlight menu
      document.querySelectorAll('#menu .menu-item').forEach(function (it, idx) {
        it.classList.toggle('active', idx === s);
      });

      // settings area
      if (s === 3) {
        document.getElementById('settingsArea').classList.remove('hidden');
        refreshSettings();
      } else {
        document.getElementById('settingsArea').classList.add('hidden');
      }

      // app area
      if (s === 1) {
        document.getElementById('appArea').classList.remove('hidden');
        if (lastState !== 1) loadTextOptions();
      } else {
        document.getElementById('appArea').classList.add('hidden');
      }

      // epd busy indicator
      var epdBusyEl = document.getElementById('epdBusy');
      if (epdBusyEl) epdBusyEl.classList.toggle('hidden', !busy);

      lastState = s;
    } catch (e) {
      console.log('refresh error', e);
    }
  }

  async function loadTextOptions() {
    try {
      var r = await fetch('/apps/text/list');
      var j = await r.json();
      var opts = j.options || [];
      var html = '';
      for (var i = 0; i < opts.length; ++i) {
        html += '<div>' + i + ': ' + opts[i] + ' <button data-idx="' + i + '">Show</button></div>';
      }
      var container = document.getElementById('textOptions');
      if (!container) return;
      container.innerHTML = html;
      container.querySelectorAll('button[data-idx]').forEach(function (b) {
        b.addEventListener('click', function () {
          postSelect(Number(b.getAttribute('data-idx')));
        });
      });
    } catch (e) {
      console.log('loadTextOptions error', e);
    }
  }

  function postSelect(i) {
    fetch('/apps/text/select', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ index: Number(i) })
    }).then(refresh).catch(function (e) { console.log('postSelect error', e); });
  }

  function enterApp() { fetch('/button/select', { method: 'POST' }).then(refresh); }
  function exitApp() { fetch('/button/back', { method: 'POST' }).then(refresh); }
  function btnNext() { fetch('/button/next', { method: 'POST' }).then(refresh); }
  function btnSelect() { fetch('/button/select', { method: 'POST' }).then(refresh); }
  function btnBack() { fetch('/button/back', { method: 'POST' }).then(refresh); }

  // Map settings actions (currently: simple simulation via next->select).
  function postSettingAction(action) {
    if (action === 'ip') {
      // noop: just refresh status
      refresh();
      return;
    }
    // Default behaviour: simulate next + select so the device menu can act
    fetch('/button/next', { method: 'POST' })
      .then(function () { return fetch('/button/select', { method: 'POST' }); })
      .then(refresh)
      .catch(function (e) { console.log('postSettingAction error', e); });
  }

  async function refreshSettings() {
    try {
      var r = await fetch('/status');
      var j = await r.json();
      document.getElementById('ipline').innerText = 'IP: ' + j.ip + ' | Partial: ' + (j.partialEnabled ? 'ON' : 'OFF');
    } catch (e) {
      console.log('refreshSettings error', e);
    }
  }

  function doDiag() {
    fetch('/diag').then(function (r) { return r.json(); }).then(function (j) {
      document.getElementById('diag').innerText = JSON.stringify(j);
    }).catch(function (e) { console.log('doDiag error', e); });
  }

  document.addEventListener('DOMContentLoaded', function () {
    // Hook buttons (guard in case elements are missing)
    var el = function (id) { return document.getElementById(id); };
    if (el('btnNext')) el('btnNext').addEventListener('click', btnNext);
    if (el('btnSelect')) el('btnSelect').addEventListener('click', btnSelect);
    if (el('btnBack')) el('btnBack').addEventListener('click', btnBack);
    if (el('enterApp')) el('enterApp').addEventListener('click', enterApp);
    if (el('exitApp')) el('exitApp').addEventListener('click', exitApp);
    if (el('doDiag')) el('doDiag').addEventListener('click', doDiag);

    // Settings buttons (use data-action attribute)
    document.querySelectorAll('#settingsArea button').forEach(function (b) {
      b.addEventListener('click', function () {
        postSettingAction(b.getAttribute('data-action'));
      });
    });

    // Start polling and initial refresh
    setInterval(refresh, 700);
    refresh();
  });

  // Export a tiny API for console / debugging
  window.bringer = {
    refresh: refresh,
    postSelect: postSelect
  };

})();
