// ================= script.js – Dashboard Logic =================
// This file runs only on index.html (the dashboard)

const videoStream  = document.getElementById('video-stream');
const cameraStatus = document.getElementById('camera-status');

// ================= Mobile Navigation Toggle =================
function toggleMobileNav() {
    const links = document.getElementById('topnavLinks');
    const toggle = document.getElementById('navToggle');
    if (links) {
        links.classList.toggle('open');
        const isOpen = links.classList.contains('open');
        if (toggle) toggle.textContent = isOpen ? '✕' : '☰';
    }
}

// Close mobile nav when clicking outside
document.addEventListener('click', (e) => {
    const topnav = document.querySelector('.topnav');
    const links = document.getElementById('topnavLinks');
    const toggle = document.getElementById('navToggle');
    if (links && links.classList.contains('open') && topnav && !topnav.contains(e.target)) {
        links.classList.remove('open');
        if (toggle) toggle.textContent = '☰';
    }
});

// ================= Theme Toggle =================
function initTheme() {
    const savedTheme = localStorage.getItem('sgx_theme') || 'dark';
    document.documentElement.setAttribute('data-theme', savedTheme);
    updateThemeToggleBtn(savedTheme);
}

function toggleTheme() {
    const currentTheme = document.documentElement.getAttribute('data-theme') || 'dark';
    // Cycle: dark -> light (white) -> bw (black & white monochrome) -> dark
    let nextTheme = 'light';
    if (currentTheme === 'dark') nextTheme = 'light';
    else if (currentTheme === 'light') nextTheme = 'bw';
    else nextTheme = 'dark';

    document.documentElement.setAttribute('data-theme', nextTheme);
    localStorage.setItem('sgx_theme', nextTheme);
    updateThemeToggleBtn(nextTheme);
}

function updateThemeToggleBtn(theme) {
    const btnText = document.getElementById('themeToggleText');
    const btnIcon = document.getElementById('themeToggleIcon');
    if (!btnText) return;
    if (theme === 'light') {
        btnText.textContent = 'Light';
        if (btnIcon) btnIcon.textContent = '☀️';
    } else if (theme === 'bw') {
        btnText.textContent = 'B & W';
        if (btnIcon) btnIcon.textContent = '🔲';
    } else {
        btnText.textContent = 'Dark';
        if (btnIcon) btnIcon.textContent = '🌙';
    }
}

// Initialize theme immediately
initTheme();

// ---- Logout ----
function logout() {
    localStorage.removeItem('sgx_username');
    sessionStorage.removeItem('sgx_username');
    window.location.href = 'login.html';
}

// ---- Command Sending ----
function sendCommand(action) {
    console.log('Command Sent:', action);
    const cmd = action === 'forward'  ? 'F'
              : action === 'backward' ? 'B'
              : action === 'left'     ? 'L'
              : action === 'right'    ? 'R'
              : 'S';
    fetch(`/action?go=${cmd}`)
        .catch(err => console.error('Command error:', err));
}

// ================= Keyboard Control =================
window.addEventListener('keydown', (event) => {
    if (event.repeat || !event.key) return;
    const key = event.key.toLowerCase();
    if (key === 'w') sendCommand('forward');
    if (key === 'a') sendCommand('left');
    if (key === 's') sendCommand('backward');
    if (key === 'd') sendCommand('right');
});

window.addEventListener('keyup', (event) => {
    if (!event.key) return;
    const key = event.key.toLowerCase();
    if (['w', 'a', 's', 'd'].includes(key)) sendCommand('stop');
});

// ================= Video History =================

/**
 * Fetch the video list from the backend and populate the UI.
 */
async function loadVideoHistory() {
    const list = document.getElementById('videoList');
    const icon = document.getElementById('refreshIcon');

    if (icon) icon.classList.add('spin');

    try {
        const res = await fetch('http://localhost:8000/api/videos');
        if (!res.ok) throw new Error(`Server error: ${res.status}`);

        const data   = await res.json();
        const videos = data.videos || [];

        if (!list) return;
        list.innerHTML = '';

        if (videos.length === 0) {
            list.innerHTML = `
                <div class="vh-empty-state" id="vh-empty">
                    <span>🎞️</span>
                    <p>No recordings found. Start recording to see files here.</p>
                </div>`;
            return;
        }

        videos.forEach((filename, index) => {
            const item = document.createElement('div');
            item.className = 'vh-item';
            item.id        = `vh-item-${index}`;
            item.setAttribute('data-filename', filename);

            const metaText = formatFileMeta(filename);

            item.innerHTML = `
                <div class="vh-item-icon">📹</div>
                <div class="vh-item-info">
                    <div class="vh-item-name">${filename}</div>
                    <div class="vh-item-meta">${metaText}</div>
                </div>
                <div class="vh-item-play-btn">▶</div>`;

            item.onclick = () => playVideo(filename, item);
            list.appendChild(item);
        });

    } catch (err) {
        console.error('Video History fetch error:', err);
        if (list) {
            list.innerHTML = `
                <div class="vh-empty-state">
                    <span>⚠️</span>
                    <p>Could not load videos. Is the backend running?</p>
                </div>`;
        }
    } finally {
        if (icon) icon.classList.remove('spin');
    }
}

/**
 * Extract a human-readable date/time from the filename if possible.
 */
function formatFileMeta(filename) {
    const match = filename.match(/(\d{4}[-_]\d{2}[-_]\d{2})[_\s-]?(\d{2}[-:]\d{2}[-:]\d{2})?/);
    if (match) {
        const datePart = match[1].replace(/_/g, '-');
        const timePart = match[2] ? match[2].replace(/-/g, ':') : '';
        return timePart ? `Recorded: ${datePart} at ${timePart}` : `Recorded: ${datePart}`;
    }
    return '.webm recording';
}

/**
 * Load a selected video into the HTML5 player.
 */
function playVideo(filename, item) {
    const player        = document.getElementById('historyPlayer');
    const source        = document.getElementById('historyPlayerSource');
    const placeholder   = document.getElementById('vh-player-placeholder');
    const nowPlaying    = document.getElementById('vh-now-playing');
    const nowPlayingName = document.getElementById('vh-now-playing-name');

    if (!player || !source) return;

    document.querySelectorAll('.vh-item').forEach(el => el.classList.remove('active'));
    if (item) item.classList.add('active');

    const videoUrl = `http://localhost:8000/records/${encodeURIComponent(filename)}`;
    source.src = videoUrl;
    player.load();
    player.play().catch(e => console.warn('Autoplay blocked:', e));

    player.style.display      = 'block';
    if (placeholder) placeholder.style.display = 'none';

    if (nowPlayingName) nowPlayingName.textContent = `NOW PLAYING: ${filename}`;
    if (nowPlaying) nowPlaying.style.display   = 'flex';

    console.log(`Playing: ${videoUrl}`);
}

// ---- Auto-load video history on page ready ----
document.addEventListener('DOMContentLoaded', () => {
    initTheme();
    loadVideoHistory();
});