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

    if (tabName === 'Settings') refreshSettings();

    if (tabName === 'Settings') refreshSettings();
    if (tabName === 'Logs') refreshLogs();
    if (tabName === 'Epub') loadEpubList();
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


  // --- Device Actions ---


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

  // --- Epub Logic ---
  async function loadEpubList() {
    try {
      var r = await fetch('/api/epub/list');
      var list = await r.json();
      var html = '<ul style="list-style:none; padding:0;">';
      if (!list || list.length === 0) {
        html += '<li>No books found.</li>';
      } else {
        list.forEach(function (item) {
          html += '<li style="padding:5px; border-bottom:1px solid #eee; display:flex; justify-content:space-between;">';
          html += '<span>' + item.name + ' (' + Math.round(item.size / 1024) + ' KB)</span>';
          html += '<div>';
          html += '<button data-ren="' + item.name + '" style="margin-right:5px;">Rename</button>';
          html += '<button data-del="' + item.name + '" style="color:red;">Delete</button>';
          html += '</div></li>';
        });
      }
      html += '</ul>';
      var div = document.getElementById('epubList');
      if (div) {
        div.innerHTML = html;
        // Add listeners
        div.querySelectorAll('button[data-del]').forEach(function (b) {
          b.addEventListener('click', function () { deleteEpub(b.getAttribute('data-del')); });
        });
        div.querySelectorAll('button[data-ren]').forEach(function (b) {
          b.addEventListener('click', function () { renameEpub(b.getAttribute('data-ren')); });
        });
      }
    } catch (e) {
      console.log(e);
    }
  }

  async function renameEpub(oldName) {
    var newName = prompt("New name (must end with .epub):", oldName);
    if (!newName || newName === oldName) return;
    if (!newName.endsWith(".epub")) { alert("Must end with .epub"); return; }

    try {
      var form = new FormData();
      form.append("oldName", oldName);
      form.append("newName", newName);
      var r = await fetch('/api/epub/rename', { method: 'POST', body: form });
      if (r.ok) loadEpubList();
      else alert("Rename failed");
    } catch (e) {
      console.log(e);
    }
  }

  async function uploadEpub() {
    var input = document.getElementById('epubFile');
    if (input.files.length === 0) return;
    var file = input.files[0];
    var formData = new FormData();
    formData.append("file", file);

    var status = document.getElementById('uploadStatus');
    status.innerText = "Uploading...";

    try {
      var r = await fetch('/api/epub/upload', {
        method: 'POST',
        body: formData
      });
      if (r.ok) {
        status.innerText = "Done!";
        loadEpubList();
        input.value = ''; // clear
      } else {
        status.innerText = "Error.";
      }
    } catch (e) {
      status.innerText = "Fail: " + e;
    }
  }

  async function deleteEpub(name) {
    if (!confirm("Delete " + name + "?")) return;
    try {
      var form = new FormData();
      form.append("name", name);
      var r = await fetch('/api/epub/delete', {
        method: 'POST',
        body: form
      });
      if (r.ok) loadEpubList();
    } catch (e) {
      console.log(e);
    }
  }

  // --- Initialization ---
  document.addEventListener('DOMContentLoaded', function () {
    setupTabs();

    // Hook buttons
    var el = function (id) { return document.getElementById(id); };



    if (el('btnRefreshSettings')) el('btnRefreshSettings').addEventListener('click', refreshSettings);

    if (el('btnRefreshSettings')) el('btnRefreshSettings').addEventListener('click', refreshSettings);
    if (el('btnRefreshLogs')) el('btnRefreshLogs').addEventListener('click', refreshLogs);
    if (el('btnRefreshEpub')) el('btnRefreshEpub').addEventListener('click', loadEpubList);
    if (el('btnUploadEpub')) el('btnUploadEpub').addEventListener('click', uploadEpub);

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


