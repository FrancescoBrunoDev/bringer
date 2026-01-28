// wallpapers.js - Wallpaper generation functions for bitmap editor

var WALLPAPER_CONFIG = {
    // Configuration for noise-based wallpaper
    noise: {
        seed: 42 // Seed for the random number generator
    },
    // Configuration for stripe-based wallpaper
    stripes: {
        waveWidth: 20, // Width of each stripe
        waveAmplitude: 5, // Amplitude of the wave pattern
        waveLength: 80, // Length of the wave cycle
        waveAngle: 45 // Angle of the stripes in degrees
    },
    // Configuration for grid-based wallpaper
    grid: {
        dotSpacing: 12, // Distance between dots in the grid
        dotRadius: 1 // Radius of each dot
    }
};

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

    // Reuse load logic
    loadNoisePreset(ctx, 128, 296);

    thumb.style.backgroundImage = 'url(' + canvas.toDataURL() + ')';
    thumb.style.backgroundSize = 'contain';
    thumb.style.backgroundRepeat = 'no-repeat';
    thumb.style.backgroundPosition = 'center';
    thumb.textContent = '';
}

function loadNoisePreset(ctx, width, height) {
    // Generate noise directly on canvas with fixed seed
    var imgData = ctx.createImageData(width, height);
    var rng = seededRandom(WALLPAPER_CONFIG.noise.seed);
    for (var i = 0; i < imgData.data.length; i += 4) {
        var val = rng() > 0.5 ? 255 : 0;
        imgData.data[i] = val;
        imgData.data[i + 1] = val;
        imgData.data[i + 2] = val;
        imgData.data[i + 3] = 255;
    }
    ctx.putImageData(imgData, 0, 0);
}

function generateStripeThumbnail() {
    var thumb = document.getElementById('thumb-stripes');
    if (!thumb) return;

    var canvas = document.createElement('canvas');
    canvas.width = 128;
    canvas.height = 296;
    var ctx = canvas.getContext('2d');

    // Reuse the load logic but specifically for the thumbnail canvas
    loadStripesPreset(ctx, 128, 296);

    thumb.style.backgroundImage = 'url(' + canvas.toDataURL() + ')';
    thumb.style.backgroundSize = 'contain';
    thumb.style.backgroundRepeat = 'no-repeat';
    thumb.style.backgroundPosition = 'center';
    thumb.textContent = '';
}

function loadStripesPreset(ctx, width, height) {
    // Clear canvas
    ctx.fillStyle = 'white';
    ctx.fillRect(0, 0, width, height);

    var waveWidth = WALLPAPER_CONFIG.stripes.waveWidth;
    var waveAmplitude = WALLPAPER_CONFIG.stripes.waveAmplitude;
    var waveLength = WALLPAPER_CONFIG.stripes.waveLength;
    var angle = WALLPAPER_CONFIG.stripes.waveAngle || 0;

    ctx.save();

    var drawWidth = width;
    var drawHeight = height;

    if (angle !== 0) {
        // Calculate diagonal to cover entire rotated area
        var diag = Math.ceil(Math.sqrt(width * width + height * height));
        drawWidth = diag;
        drawHeight = diag;

        // Translate to center, rotate, translate back
        ctx.translate(width / 2, height / 2);
        ctx.rotate(angle * Math.PI / 180);
        ctx.translate(-drawWidth / 2, -drawHeight / 2);
    }

    var numWaves = Math.ceil(drawWidth / waveWidth) + 2;

    for (var w = 0; w < numWaves; w++) {
        // Alternate between black and white
        ctx.fillStyle = (w % 2 === 0) ? 'black' : 'white';

        ctx.beginPath();
        var startX = w * waveWidth - waveWidth;

        // Left edge of wave
        for (var y = 0; y <= drawHeight; y++) {
            var x = startX + Math.sin((y / waveLength) * Math.PI * 2) * waveAmplitude;
            if (y === 0) ctx.moveTo(x, y);
            else ctx.lineTo(x, y);
        }

        // Right edge of wave (going back up)
        for (var y = drawHeight; y >= 0; y--) {
            var x = (startX + waveWidth) + Math.sin((y / waveLength) * Math.PI * 2) * waveAmplitude;
            ctx.lineTo(x, y);
        }

        ctx.closePath();
        ctx.fill();
    }

    ctx.restore();
}

function generateGridThumbnail() {
    var thumb = document.getElementById('thumb-grid');
    if (!thumb) return;

    var canvas = document.createElement('canvas');
    canvas.width = 128;
    canvas.height = 296;
    var ctx = canvas.getContext('2d');

    // Reuse load logic
    loadGridPreset(ctx, 128, 296);

    thumb.style.backgroundImage = 'url(' + canvas.toDataURL() + ')';
    thumb.style.backgroundSize = 'contain';
    thumb.style.backgroundRepeat = 'no-repeat';
    thumb.style.backgroundPosition = 'center';
    thumb.textContent = '';
}

function loadGridPreset(ctx, width, height) {
    // Clear canvas
    ctx.fillStyle = 'white';
    ctx.fillRect(0, 0, width, height);

    // Draw dots
    ctx.fillStyle = 'black';

    var dotSpacing = WALLPAPER_CONFIG.grid.dotSpacing;
    var dotRadius = WALLPAPER_CONFIG.grid.dotRadius;

    for (var y = dotSpacing / 2; y < height; y += dotSpacing) {
        for (var x = dotSpacing / 2; x < width; x += dotSpacing) {
            ctx.beginPath();
            ctx.arc(x, y, dotRadius, 0, Math.PI * 2);
            ctx.fill();
        }
    }
}

function loadWhitePreset(ctx, width, height) {
    // Fill canvas with white
    ctx.fillStyle = 'white';
    ctx.fillRect(0, 0, width, height);
}

function loadBlackPreset(ctx, width, height) {
    // Fill canvas with black
    ctx.fillStyle = 'black';
    ctx.fillRect(0, 0, width, height);
}
