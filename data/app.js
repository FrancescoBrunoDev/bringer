/*
  app.js - Client-side script for the device server UI
*/

(function () {
  'use strict';

  var currentTab = 'Home';

  // --- Tab Management ---
  function setupTabs() {
    var tabs = document.querySelectorAll('.tab-link');
    tabs.forEach(function (tab) {
      tab.addEventListener('click', function (e) {
        openTab(e.currentTarget.getAttribute('data-tab'));
      });
    });
  }

  function openTab(tabName) {
    currentTab = tabName;

    // Hide all tab content
    var contents = document.querySelectorAll('.tab-content');
    contents.forEach(function (content) {
      content.classList.remove('active');
    });

    // Remove active class from all tabs
    var links = document.querySelectorAll('.tab-link');
    links.forEach(function (link) {
      link.classList.remove('active');
    });

    // Show the specific tab content
    var activeContent = document.getElementById(tabName);
    if (activeContent) activeContent.classList.add('active');

    // Add active class to the button that opened the tab
    var activeLink = document.querySelector('.tab-link[data-tab="' + tabName + '"]');
    if (activeLink) activeLink.classList.add('active');

    // Load specific data if needed
    if (tabName === 'Apps') loadTextOptions();
    if (tabName === 'Settings') refreshSettings();
    if (tabName === 'Logs') refreshLogs();
  }

  // --- Polling & Status ---
  async function refresh() {
    try {
      // 1. Check UI State (always)
      var resp = await fetch('/ui_state');
      var j = await resp.json();
      var busy = j.epdBusy;

      // Removed EPD busy indicator in this update since Control UI is gone,
      // but we could keep it if it was floating. Just ignoring it for now as per minimal requests.

      // 2. Refresh Logs if active
      if (currentTab === 'Logs') {
        var chk = document.getElementById('chkAutoRefreshLogs');
        if (chk && chk.checked) {
          // mild throttling or just call it
          refreshLogs();
        }
      }

    } catch (e) {
      console.log('refresh error', e);
    }
  }

  // --- Logs Logic ---
  async function refreshLogs() {
    try {
      var r = await fetch('/logs');
      var logs = await r.json();
      // logs is array of strings
      var el = document.getElementById('logOutput');
      if (el) {
        el.innerText = logs.join('\n');
        el.scrollTop = el.scrollHeight; // Auto-scroll to bottom
      }
    } catch (e) {
      console.log('logs error', e);
    }
  }

  // --- Apps Logic ---
  async function loadTextOptions() {
    try {
      var r = await fetch('/apps/text/list');
      var j = await r.json();
      var opts = j.options || [];
      var html = '';
      if (opts.length === 0) {
        html = '<div>No text files found.</div>';
      } else {
        for (var i = 0; i < opts.length; ++i) {
          html += '<div>' + i + ': ' + opts[i] + ' <button data-idx="' + i + '">Show</button></div>';
        }
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
      var container = document.getElementById('textOptions');
      if (container) container.innerHTML = 'Error loading list.';
    }
  }

  function postSelect(i) {
    fetch('/apps/text/select', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ index: Number(i) })
    }).catch(function (e) { console.log('postSelect error', e); });
  }

  // --- Device Actions ---
  function enterApp() { fetch('/button/select', { method: 'POST' }); }
  function exitApp() { fetch('/button/back', { method: 'POST' }); }

  // --- Settings Logic ---
  function postSettingAction(action) {
    if (action === 'ip') {
      refreshSettings();
      return;
    }
    fetch('/button/next', { method: 'POST' })
      .then(function () { return fetch('/button/select', { method: 'POST' }); })
      .then(refresh) // Trigger a status check
      .catch(function (e) { console.log('postSettingAction error', e); });
  }

  async function refreshSettings() {
    try {
      var r = await fetch('/status');
      var j = await r.json();
      var label = 'IP: ' + j.ip + ' | Partial: ' + (j.partialEnabled ? 'ON' : 'OFF');
      var el = document.getElementById('ipline');
      if (el) el.innerText = label;
    } catch (e) {
      console.log('refreshSettings error', e);
    }
  }

  // --- Initialization ---
  document.addEventListener('DOMContentLoaded', function () {
    setupTabs();

    // Hook buttons
    var el = function (id) { return document.getElementById(id); };
    if (el('enterApp')) el('enterApp').addEventListener('click', enterApp);
    if (el('exitApp')) el('exitApp').addEventListener('click', exitApp);

    if (el('btnRefreshText')) el('btnRefreshText').addEventListener('click', loadTextOptions);
    if (el('btnRefreshSettings')) el('btnRefreshSettings').addEventListener('click', refreshSettings);
    if (el('btnRefreshLogs')) el('btnRefreshLogs').addEventListener('click', refreshLogs);

    // Settings content buttons
    var settingsContainer = document.getElementById('settingsParams');
    if (settingsContainer) {
      settingsContainer.querySelectorAll('button[data-action]').forEach(function (b) {
        b.addEventListener('click', function () {
          postSettingAction(b.getAttribute('data-action'));
        });
      });
    }

    // Start polling
    setInterval(refresh, 1000);
    // Initial data load 
    refresh();
  });

})();


