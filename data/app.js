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

    // --- Bitmap Logic ---
    var bmpCanvas, bmpCtx;
    var bmpWidth = 128; // Default E-Paper (vertical)
    var bmpHeight = 296;
    var isDrawing = false;
    var lastX = -1, lastY = -1;

    function initBitmapTab() {
      bmpCanvas = document.getElementById('bmpCanvas');
      if (!bmpCanvas) return;
      bmpCtx = bmpCanvas.getContext('2d');

      // Controls
      var sel = document.getElementById('bmpTarget');
      if (sel) {
        sel.addEventListener('change', function () {
          if (sel.value === 'oled') { bmpWidth = 128; bmpHeight = 64; }
          else if (sel.value === 'epd') { bmpWidth = 128; bmpHeight = 296; }
          resizeCanvas();
        });
      }

      // Buttons
      var btnClear = document.getElementById('btnBmpClear');
      if (btnClear) btnClear.addEventListener('click', function () {
        bmpCtx.fillStyle = '#FFFFFF';
        bmpCtx.fillRect(0, 0, bmpWidth, bmpHeight);
      });

      var btnInvert = document.getElementById('btnBmpInvert');
      if (btnInvert) btnInvert.addEventListener('click', invertCanvas);

      var btnExport = document.getElementById('btnBmpExport');
      if (btnExport) btnExport.addEventListener('click', exportBitmap);

      var btnSetWallpaper = document.getElementById('btnBmpSetWallpaper');
      if (btnSetWallpaper) btnSetWallpaper.addEventListener('click', uploadWallpaper);

      // Upload
      var upload = document.getElementById('bmpUpload');
      if (upload) upload.addEventListener('change', handleImageUpload);

      // Gallery handlers
      document.querySelectorAll('.gallery-item').forEach(function (item) {
        item.addEventListener('click', function () {
          var preset = this.getAttribute('data-preset');
          if (preset === 'noise') {
            loadNoisePreset();
          } else {
            alert('This wallpaper will be available soon!');
          }
        });
      });

      // Initialize noise thumbnail
      generateNoiseThumbnail();

      // Drawing Events
      // scale to visual size
      bmpCanvas.style.width = (bmpWidth * 2) + 'px';
      bmpCanvas.style.height = (bmpHeight * 2) + 'px';

      var draw = function (e) {
        if (!isDrawing) return;
        var rect = bmpCanvas.getBoundingClientRect();
        var x = Math.floor((e.clientX - rect.left) / 2); // divide by 2 due to scaling style
        var y = Math.floor((e.clientY - rect.top) / 2);

        // Simple Bresenham or line? For now, just dots. 
        // If we move fast, we might miss dots. Let's do a simple line from last.
        if (lastX !== -1) {
          drawLine(lastX, lastY, x, y);
        } else {
          drawPixel(x, y);
        }
        lastX = x;
        lastY = y;
      };

      bmpCanvas.addEventListener('mousedown', function (e) {
        isDrawing = true;
        lastX = -1; lastY = -1;
        draw(e);
      });
      bmpCanvas.addEventListener('mousemove', draw);
      window.addEventListener('mouseup', function () { isDrawing = false; });

      // Initial size
      resizeCanvas();
    }

    function resizeCanvas() {
      if (!bmpCanvas) return;
      bmpCanvas.width = bmpWidth;
      bmpCanvas.height = bmpHeight;
      // CSS scale for visibility
      bmpCanvas.style.width = (bmpWidth * 2) + 'px';
      bmpCanvas.style.height = (bmpHeight * 2) + 'px';

      // Fill white
      bmpCtx.fillStyle = '#FFFFFF';
      bmpCtx.fillRect(0, 0, bmpWidth, bmpHeight);
    }

    function drawPixel(x, y) {
      bmpCtx.fillStyle = '#000000';
      bmpCtx.fillRect(x, y, 1, 1);
    }

    function drawLine(x0, y0, x1, y1) {
      var dx = Math.abs(x1 - x0);
      var dy = Math.abs(y1 - y0);
      var sx = (x0 < x1) ? 1 : -1;
      var sy = (y0 < y1) ? 1 : -1;
      var err = dx - dy;
      while (true) {
        drawPixel(x0, y0);
        if ((x0 === x1) && (y0 === y1)) break;
        var e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
      }
    }

    function invertCanvas() {
      var imgData = bmpCtx.getImageData(0, 0, bmpWidth, bmpHeight);
      var d = imgData.data;
      for (var i = 0; i < d.length; i += 4) {
        d[i] = 255 - d[i];     // R
        d[i + 1] = 255 - d[i + 1]; // G
        d[i + 2] = 255 - d[i + 2]; // B
      }
      bmpCtx.putImageData(imgData, 0, 0);
    }

    function handleImageUpload(e) {
      var file = e.target.files[0];
      if (!file) return;

      var reader = new FileReader();
      reader.onload = function (evt) {
        var img = new Image();
        img.onload = function () {
          // Draw image resized
          bmpCtx.fillStyle = 'white';
          bmpCtx.fillRect(0, 0, bmpWidth, bmpHeight);

          // rudimentary aspect ratio fit
          var scale = Math.min(bmpWidth / img.width, bmpHeight / img.height);
          var w = img.width * scale;
          var h = img.height * scale;
          var x = (bmpWidth - w) / 2;
          var y = (bmpHeight - h) / 2;

          bmpCtx.drawImage(img, x, y, w, h);
          ditherCanvas();
        };
        img.src = evt.target.result;
      };
      reader.readAsDataURL(file);
    }

    function ditherCanvas() {
      // Implementing Floyd-Steinberg or simple threshold
      // Let's do simple threshold for now for simplicity, or F-S if possible.
      // Floyd-Steinberg gives better results for photos.

      var imgData = bmpCtx.getImageData(0, 0, bmpWidth, bmpHeight);
      var d = imgData.data;
      var w = bmpWidth;

      for (var y = 0; y < bmpHeight; y++) {
        for (var x = 0; x < w; x++) {
          var i = (y * w + x) * 4;
          // Grayscale conversion
          var gray = 0.299 * d[i] + 0.587 * d[i + 1] + 0.114 * d[i + 2];
          var newPixel = gray < 128 ? 0 : 255;
          var err = gray - newPixel;

          d[i] = d[i + 1] = d[i + 2] = newPixel;

          // F-S Distribute Error
          // right, down-left, down, down-right
          function distribute(dx, dy, factor) {
            var nx = x + dx;
            var ny = y + dy;
            if (nx >= 0 && nx < w && ny >= 0 && ny < bmpHeight) {
              var ni = (ny * w + nx) * 4;
              var ng = 0.299 * d[ni] + 0.587 * d[ni + 1] + 0.114 * d[ni + 2];
              ng += err * factor;
              // clamp? actually we only read gray again. 
              // Simplest: just add to R,G,B
              d[ni] += err * factor;
              d[ni + 1] += err * factor;
              d[ni + 2] += err * factor;
            }
          }

          distribute(1, 0, 7 / 16);
          distribute(-1, 1, 3 / 16);
          distribute(0, 1, 5 / 16);
          distribute(1, 1, 1 / 16);
        }
      }
      bmpCtx.putImageData(imgData, 0, 0);
    }

    function exportBitmap() {
      var imgData = bmpCtx.getImageData(0, 0, bmpWidth, bmpHeight);
      var d = imgData.data;

      // Generate C array. 1 bit per pixel.
      // Rows top->bottom, each byte MSB first left->right.
      var bytes = [];
      var currentByte = 0;
      var bits = 0;

      var totalPixels = bmpWidth * bmpHeight;
      var byteCount = Math.ceil(bmpWidth / 8) * bmpHeight; // not exactly, depends on stride
      // Typically OLED/EPD use row-based packing. Width might not be multiple of 8.
      // E.g. 128 is mult of 8. 296 is mult of 8.

      for (var y = 0; y < bmpHeight; y++) {
        for (var x = 0; x < bmpWidth; x++) {
          var i = (y * bmpWidth + x) * 4;
          var isWhite = (d[i] > 127); // >127 is white
          var bit = isWhite ? 0 : 1; // Assuming 1=Black/On, 0=White/Off for typical usage? 
          // Wait, EPD/OLED often use 1=White, 0=Black OR 1=Black, 0=White.
          // Let's stick to standard: 1=Black (Draw Color), 0=White (Background).
          // If user wants inverted, they can click Invert.

          currentByte = (currentByte << 1) | bit;
          bits++;

          if (bits === 8) {
            bytes.push(currentByte);
            currentByte = 0;
            bits = 0;
          }
        }
        // If width is not multiple of 8, do we pad each row?
        // GxEPD often expects padded rows? No, it usually packs.
        // But SSD1306 usually packs.
        // Let's assume packed continuous unless row padding needed.
        // Actually standard usually pad rows to byte boundary.
        // But let's handle the leftover bits for file.
      }
      if (bits > 0) {
        // Shift remaining
        currentByte = currentByte << (8 - bits);
        bytes.push(currentByte);
      }

      // Format as Hex
      var s = "const unsigned char bitmap_" + bmpWidth + "x" + bmpHeight + "[] PROGMEM = {\n";
      for (var k = 0; k < bytes.length; k++) {
        if (k > 0 && k % 12 === 0) s += "\n";
        var h = bytes[k].toString(16).toUpperCase();
        if (h.length < 2) h = "0" + h;
        s += "0x" + h + ", ";
      }
      s += "\n};\n";

      var out = document.getElementById('bmpOutput');
      if (out) out.value = s;
    }

    async function uploadWallpaper() {
      var imgData = bmpCtx.getImageData(0, 0, bmpWidth, bmpHeight);
      var d = imgData.data;

      // Generate bitmap bytes (same as export)
      var bytes = [];
      var currentByte = 0;
      var bits = 0;

      for (var y = 0; y < bmpHeight; y++) {
        for (var x = 0; x < bmpWidth; x++) {
          var i = (y * bmpWidth + x) * 4;
          var isWhite = (d[i] > 127);
          var bit = isWhite ? 0 : 1;

          currentByte = (currentByte << 1) | bit;
          bits++;

          if (bits === 8) {
            bytes.push(currentByte);
            currentByte = 0;
            bits = 0;
          }
        }
      }
      if (bits > 0) {
        currentByte = currentByte << (8 - bits);
        bytes.push(currentByte);
      }

      // Convert to base64
      var binary = '';
      for (var i = 0; i < bytes.length; i++) {
        binary += String.fromCharCode(bytes[i]);
      }
      var base64 = btoa(binary);

      // Send to device
      try {
        var payload = JSON.stringify({
          width: bmpWidth,
          height: bmpHeight,
          data: base64
        });

        var r = await fetch('/api/wallpaper/upload', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: payload
        });

        if (r.ok) {
          alert('Wallpaper set! Reboot to see it.');
        } else {
          alert('Failed to set wallpaper.');
        }
      } catch (e) {
        alert('Error: ' + e);
      }
    }

    function seededRandom(seed) {
      // Simple seeded RNG
      return function () {
        seed = (seed * 9301 + 49297) % 233280;
        return seed / 233280;
      };
    }

    function generateNoiseThumbnail() {
      // Generate noise preview for thumbnail
      var thumb = document.getElementById('thumb-noise');
      if (!thumb) return;

      var canvas = document.createElement('canvas');
      // Generate at full resolution for crisp display
      canvas.width = 128;
      canvas.height = 296;
      var ctx = canvas.getContext('2d');
      var imgData = ctx.createImageData(128, 296);

      var rng = seededRandom(42);
      for (var i = 0; i < imgData.data.length; i += 4) {
        var val = rng() > 0.5 ? 255 : 0;
        imgData.data[i] = val;
        imgData.data[i + 1] = val;
        imgData.data[i + 2] = val;
        imgData.data[i + 3] = 255;
      }
      ctx.putImageData(imgData, 0, 0);

      thumb.style.backgroundImage = 'url(' + canvas.toDataURL() + ')';
      thumb.style.backgroundSize = 'contain';
      thumb.style.backgroundRepeat = 'no-repeat';
      thumb.style.backgroundPosition = 'center';
      thumb.textContent = '';
    }

    function loadNoisePreset() {
      // Generate noise directly on main canvas with fixed seed
      var imgData = bmpCtx.createImageData(bmpWidth, bmpHeight);
      var rng = seededRandom(42);
      for (var i = 0; i < imgData.data.length; i += 4) {
        var val = rng() > 0.5 ? 255 : 0;
        imgData.data[i] = val;
        imgData.data[i + 1] = val;
        imgData.data[i + 2] = val;
        imgData.data[i + 3] = 255;
      }
      bmpCtx.putImageData(imgData, 0, 0);
    }


    // Hook buttons
    var el = function (id) { return document.getElementById(id); };

    if (el('btnRefreshSettings')) el('btnRefreshSettings').addEventListener('click', refreshSettings);

    initBitmapTab();

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


