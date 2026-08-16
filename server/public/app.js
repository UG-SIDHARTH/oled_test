/**
 * ESP32 OLED Companion Web Dashboard & SSD1306 Hardware Simulator
 */

document.addEventListener('DOMContentLoaded', () => {
    // Canvas & Context Setup (Virtual 128x64 buffer scaled to 256x128)
    const canvas = document.getElementById('oled-canvas');
    const ctx = canvas.getContext('2d');
    ctx.imageSmoothingEnabled = false;

    // UI Elements
    const statusIndicator = document.getElementById('status-indicator');
    const spotifyStatusText = document.getElementById('spotify-status-text');
    const serverIpDisplay = document.getElementById('server-ip-display');
    const trackNameDisplay = document.getElementById('track-name-display');
    const artistNameDisplay = document.getElementById('artist-name-display');
    const albumNameDisplay = document.getElementById('album-name-display');
    const albumArtImg = document.getElementById('album-art-img');
    const webProgressFill = document.getElementById('web-progress-fill');
    const timeElapsedLabel = document.getElementById('time-elapsed-label');
    const timeDurationLabel = document.getElementById('time-duration-label');
    const lyricsListContainer = document.getElementById('lyrics-list-container');
    const lyricsCountBadge = document.getElementById('lyrics-count-badge');
    const syncSourceBadge = document.getElementById('sync-source-badge');
    const btnToggleDemo = document.getElementById('btn-toggle-demo');
    const demoBtnLabel = document.getElementById('demo-btn-label');
    const btnSwitchDemoTrack = document.getElementById('btn-switch-demo-track');
    const btnSpotifyLogin = document.getElementById('btn-spotify-login');
    const btnSaveCredentials = document.getElementById('btn-save-credentials');
    const inputClientId = document.getElementById('input-client-id');
    const inputClientSecret = document.getElementById('input-client-secret');
    const screenViewport = document.getElementById('screen-viewport');
    const themePicker = document.getElementById('oled-theme-picker');

    // Local State
    let currentThemeColor = '#ffffff';
    let currentThemeGlow = 'white';
    let isDemoActive = false;
    let demoTrackIndex = 0;

    let playbackState = {
        connected: false,
        hasData: true,
        isPlaying: true,
        trackName: 'Viva La Vida',
        artistName: 'Coldplay',
        albumName: 'Viva La Vida',
        albumArt: 'https://images.unsplash.com/photo-1614613535308-eb5fbd3d2c17?w=200&auto=format&fit=crop&q=80',
        progressMs: 25000,
        durationMs: 242000,
        activeLyric: 'Now the old king is dead! Long live the king!',
        nextLyric: 'One minute I held the key',
        hasLyrics: true,
        lyrics: [],
        lastClientTimestamp: Date.now()
    };

    // Equalizer & Animation state for 128x64 Canvas
    const EQ_COUNT = 6;
    let eqHeights = [4, 8, 12, 10, 6, 3];
    let eqPeaks = [4, 8, 12, 10, 6, 3];
    let animFrame = 0;
    let headerScroll = 0;
    let lyricScroll = 0;
    let lastHeaderTrack = '';
    let lastActiveLyric = '';
    let lastScrollMs = Date.now();

    // Floating note particles
    const floatingNotes = [
        { x: 14, y: 44, speed: 0.6, phase: 0, type: true },
        { x: 42, y: 38, speed: 0.8, phase: 1.5, type: false },
        { x: 70, y: 48, speed: 0.5, phase: 3.0, type: true },
        { x: 102, y: 32, speed: 0.7, phase: 4.5, type: false }
    ];

    // ========================================================================
    // 1. Toast Notification Helper
    // ========================================================================
    function showToast(message, type = 'success') {
        const container = document.getElementById('toast-container');
        const toast = document.createElement('div');
        toast.className = `toast toast-${type}`;
        toast.textContent = message;
        container.appendChild(toast);
        setTimeout(() => toast.remove(), 4000);
    }

    // ========================================================================
    // 2. Fetch Network & Server Status
    // ========================================================================
    async function fetchNetworkInfo() {
        try {
            const res = await fetch('/api/network-info');
            if (res.ok) {
                const data = await res.json();
                const primaryIp = data.localIps[0] || 'localhost';
                serverIpDisplay.textContent = `${primaryIp}:${data.port}`;
            }
        } catch (e) {
            serverIpDisplay.textContent = 'localhost:3000';
        }
    }
    fetchNetworkInfo();

    // ========================================================================
    // 3. Connect Server-Sent Events (SSE) for Real-Time Live Sync
    // ========================================================================
    function connectSSE() {
        const eventSource = new EventSource('/api/events');

        eventSource.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                updateStateFromServer(data);
            } catch (e) {
                console.error('Failed to parse SSE event:', e);
            }
        };

        eventSource.onerror = () => {
            statusIndicator.className = 'status-indicator status-offline';
            spotifyStatusText.textContent = 'Reconnecting...';
            eventSource.close();
            setTimeout(connectSSE, 3000);
        };
    }
    connectSSE();

    function updateStateFromServer(data) {
        playbackState.connected = data.connected;
        playbackState.hasData = data.hasData;
        playbackState.isPlaying = data.isPlaying;
        playbackState.trackName = data.trackName || 'No Track';
        playbackState.artistName = data.artistName || '';
        playbackState.albumName = data.albumName || '';
        if (data.albumArt) playbackState.albumArt = data.albumArt;
        playbackState.progressMs = data.progressMs || 0;
        playbackState.durationMs = data.durationMs || 0;
        playbackState.activeLyric = data.activeLyric || '...';
        playbackState.nextLyric = data.nextLyric || '';
        playbackState.hasLyrics = data.hasLyrics || false;
        playbackState.lastClientTimestamp = Date.now();
        isDemoActive = (data.mode === 'demo');

        updateWebUI();
    }

    // ========================================================================
    // 4. Update Dashboard Web UI Components
    // ========================================================================
    function updateWebUI() {
        // Status Indicators
        if (playbackState.connected) {
            statusIndicator.className = 'status-indicator status-online';
            spotifyStatusText.textContent = isDemoActive ? 'Demo Mode' : 'Live Syncing';
            syncSourceBadge.textContent = isDemoActive ? 'Demo Simulation' : 'Spotify Connected';
            syncSourceBadge.className = isDemoActive ? 'badge badge-subtle' : 'badge badge-success';
        } else {
            statusIndicator.className = 'status-indicator status-offline';
            spotifyStatusText.textContent = 'Disconnected';
            syncSourceBadge.textContent = 'Waiting for Music';
            syncSourceBadge.className = 'badge badge-subtle';
        }

        // Demo button state
        demoBtnLabel.textContent = isDemoActive ? 'Stop Demo Mode' : 'Run Demo Mode';
        btnToggleDemo.className = isDemoActive ? 'btn btn-secondary' : 'btn btn-primary';

        // Track Info
        trackNameDisplay.textContent = playbackState.trackName;
        artistNameDisplay.textContent = playbackState.artistName;
        albumNameDisplay.textContent = playbackState.albumName || 'Streaming via LRCLIB';
        if (playbackState.albumArt) {
            albumArtImg.src = playbackState.albumArt;
        }

        // Progress Bar
        const progress = playbackState.durationMs > 0 ? (playbackState.progressMs / playbackState.durationMs) * 100 : 0;
        webProgressFill.style.width = `${Math.min(100, Math.max(0, progress))}%`;
        timeElapsedLabel.textContent = formatTime(playbackState.progressMs);
        timeDurationLabel.textContent = formatTime(playbackState.durationMs);

        // Fetch and refresh full lyric list if track changed
        fetchLyricsList();
    }

    let lastFetchedTrack = '';
    async function fetchLyricsList() {
        if (playbackState.trackName === lastFetchedTrack) {
            highlightActiveLyricInList();
            return;
        }
        lastFetchedTrack = playbackState.trackName;

        try {
            const res = await fetch('/api/lyrics');
            if (res.ok) {
                const data = await res.json();
                playbackState.lyrics = data.lyrics || [];
                lyricsCountBadge.textContent = `${playbackState.lyrics.length} Lines Loaded`;
                renderLyricsList(playbackState.lyrics);
            }
        } catch (e) {
            console.error('Error fetching lyrics:', e);
        }
    }

    function renderLyricsList(lyrics) {
        lyricsListContainer.innerHTML = '';
        if (!lyrics || lyrics.length === 0) {
            lyricsListContainer.innerHTML = '<div class="lyric-line-item">♪ No timestamped lyrics available for this song ♪</div>';
            return;
        }

        lyrics.forEach((l, index) => {
            const lineEl = document.createElement('div');
            lineEl.className = 'lyric-line-item';
            lineEl.id = `lyric-line-${index}`;
            lineEl.textContent = `[${formatTime(l.timestampMs)}] ${l.text}`;
            lyricsListContainer.appendChild(lineEl);
        });

        highlightActiveLyricInList();
    }

    function highlightActiveLyricInList() {
        const lyrics = playbackState.lyrics;
        if (!lyrics || lyrics.length === 0) return;

        let activeIdx = -1;
        for (let i = 0; i < lyrics.length; i++) {
            if (lyrics[i].timestampMs <= playbackState.progressMs) {
                activeIdx = i;
            } else {
                break;
            }
        }

        lyrics.forEach((l, i) => {
            const el = document.getElementById(`lyric-line-${i}`);
            if (!el) return;

            if (i === activeIdx) {
                el.className = 'lyric-line-item active-line';
                el.innerHTML = `<span class="lyric-note-glyph">♪</span> <span>[${formatTime(l.timestampMs)}] ${l.text}</span>`;
                el.scrollIntoView({ behavior: 'smooth', block: 'center' });
            } else if (i < activeIdx) {
                el.className = 'lyric-line-item past-line';
                el.textContent = `[${formatTime(l.timestampMs)}] ${l.text}`;
            } else {
                el.className = 'lyric-line-item upcoming-line';
                el.textContent = `[${formatTime(l.timestampMs)}] ${l.text}`;
            }
        });
    }

    function formatTime(ms) {
        const totalSec = Math.floor(ms / 1000);
        const mins = Math.floor(totalSec / 60);
        const secs = totalSec % 60;
        return `${String(mins).padStart(2, '0')}:${String(secs).padStart(2, '0')}`;
    }

    // ========================================================================
    // 5. 128x64 Pixel-Accurate OLED Canvas Emulator Loop (~25 FPS)
    // ========================================================================
    function drawOledCanvas() {
        animFrame++;
        const now = Date.now();

        // Extrapolate local progress milliseconds smoothly
        if (playbackState.isPlaying && playbackState.durationMs > 0) {
            const delta = now - playbackState.lastClientTimestamp;
            playbackState.progressMs += delta;
            if (playbackState.progressMs > playbackState.durationMs) {
                playbackState.progressMs = playbackState.durationMs;
            }
            playbackState.lastClientTimestamp = now;
        }

        // Clear 128x64 display buffer (scaled x2 to 256x128)
        ctx.fillStyle = '#000000';
        ctx.fillRect(0, 0, 256, 128);

        ctx.fillStyle = currentThemeColor;
        ctx.strokeStyle = currentThemeColor;
        ctx.font = '16px "Fira Code", monospace';
        ctx.textBaseline = 'top';

        // Scale factor: 2
        const S = 2;

        // --------------------------------------------------------------------
        // 1. TOP HEADER: Mini Equalizer + Song Title Marquee (y = 0..11)
        // --------------------------------------------------------------------
        // Mini jumping audio badge (3 bars)
        if (playbackState.isPlaying) {
            const b1 = Math.floor(Math.sin(now * 0.010) * 3 + 4);
            const b2 = Math.floor(Math.cos(now * 0.013) * 3.5 + 4.5);
            const b3 = Math.floor(Math.sin(now * 0.008) * 3 + 4);
            ctx.fillRect(0 * S, (9 - b1) * S, 2 * S, b1 * S);
            ctx.fillRect(3 * S, (9 - b2) * S, 2 * S, b2 * S);
            ctx.fillRect(6 * S, (9 - b3) * S, 2 * S, b3 * S);
        } else {
            ctx.fillRect(0 * S, 7 * S, 2 * S, 2 * S);
            ctx.fillRect(3 * S, 7 * S, 2 * S, 2 * S);
            ctx.fillRect(6 * S, 7 * S, 2 * S, 2 * S);
        }

        // Header Track String: "Track - Artist"
        let fullTitle = playbackState.trackName;
        if (playbackState.artistName) fullTitle += ` - ${playbackState.artistName}`;

        if (fullTitle !== lastHeaderTrack) {
            lastHeaderTrack = fullTitle;
            headerScroll = 0;
            lastScrollMs = now;
        }

        if (fullTitle.length > 19) {
            if (now - lastScrollMs > 220) {
                headerScroll++;
                if (headerScroll > fullTitle.length - 14) headerScroll = 0;
                lastScrollMs = now;
            }
            ctx.fillText(fullTitle.substring(headerScroll, headerScroll + 19), 11 * S, 0 * S);
        } else {
            ctx.fillText(fullTitle, 11 * S, 0 * S);
        }

        // Header separator line
        ctx.fillRect(0, 11 * S, 128 * S, 1 * S);

        // --------------------------------------------------------------------
        // 2. CENTER SECTION: Synced Lyrics & Dynamic Animations (y = 14..50)
        // --------------------------------------------------------------------
        const isInstrumental = (!playbackState.hasLyrics || playbackState.activeLyric === '♪ ♪ ♪' || playbackState.activeLyric === '...' || !playbackState.activeLyric);

        if (isInstrumental) {
            // Instrumental Mode: Spinning Vinyl Disc + Dancing Equalizer + Floating Notes
            drawSpinningVinyl(ctx, 24 * S, 31 * S, 14 * S, animFrame);
            drawEqualizerBars(ctx, 52 * S, 18 * S, 38 * S, 26 * S, playbackState.isPlaying);
            drawFloatingNotes(ctx, 96 * S, 122 * S, 14 * S, 48 * S, playbackState.isPlaying);

            ctx.font = '14px "Fira Code", monospace';
            ctx.fillText(playbackState.hasLyrics ? '[Music ♪]' : '[No Lyrics]', 54 * S, 45 * S);
        } else {
            // Synced Lyrics Mode: Active line marquee + Next line preview
            if (playbackState.activeLyric !== lastActiveLyric) {
                lastActiveLyric = playbackState.activeLyric;
                lyricScroll = 0;
            }

            ctx.font = '16px "Fira Code", monospace';
            let activeText = `> ${playbackState.activeLyric}`;
            if (playbackState.activeLyric.length > 18) {
                if (animFrame % 6 === 0) {
                    lyricScroll++;
                    if (lyricScroll > playbackState.activeLyric.length - 12) lyricScroll = 0;
                }
                activeText = `> ${playbackState.activeLyric.substring(lyricScroll, lyricScroll + 18)}`;
            }
            ctx.fillText(activeText, 0 * S, 15 * S);

            // Next lyric preview line
            if (playbackState.nextLyric) {
                ctx.font = '14px "Fira Code", monospace';
                const nextPreview = playbackState.nextLyric.length > 17 ? `${playbackState.nextLyric.substring(0, 17)}..` : playbackState.nextLyric;
                ctx.fillText(`» ${nextPreview}`, 6 * S, 28 * S);
            }

            // Mini visualizer on bottom right of lyrics area
            drawEqualizerBars(ctx, 96 * S, 38 * S, 30 * S, 11 * S, playbackState.isPlaying);
        }

        // --------------------------------------------------------------------
        // 3. FOOTER: Play/Pause Icon, Progress Bar, Time Display (y = 52..63)
        // --------------------------------------------------------------------
        ctx.fillRect(0, 51 * S, 128 * S, 1 * S);

        // Play/Pause icon
        if (playbackState.isPlaying) {
            ctx.beginPath();
            ctx.moveTo(1 * S, 54 * S);
            ctx.lineTo(1 * S, 62 * S);
            ctx.lineTo(7 * S, 58 * S);
            ctx.closePath();
            ctx.fill();
        } else {
            ctx.fillRect(1 * S, 54 * S, 2 * S, 8 * S);
            ctx.fillRect(5 * S, 54 * S, 2 * S, 8 * S);
        }

        // Progress bar (x=12, y=56, w=64, h=5)
        ctx.strokeRect(12 * S, 56 * S, 64 * S, 5 * S);
        if (playbackState.durationMs > 0) {
            const fillW = Math.floor((Math.min(playbackState.progressMs, playbackState.durationMs) / playbackState.durationMs) * 62);
            if (fillW > 0) {
                ctx.fillRect(13 * S, 57 * S, fillW * S, 3 * S);
            }
        }

        // Time text: "01:23"
        ctx.font = '14px "Fira Code", monospace';
        ctx.fillText(formatTime(playbackState.progressMs), 80 * S, 55 * S);

        requestAnimationFrame(drawOledCanvas);
    }

    // Helper: Draw 6-band Equalizer Bars
    function drawEqualizerBars(ctx, x, y, width, height, isPlaying) {
        const barWidth = Math.floor((width - (EQ_COUNT - 1) * 2) / EQ_COUNT);
        const now = Date.now();

        for (let i = 0; i < EQ_COUNT; i++) {
            if (isPlaying) {
                const phase = (now * 0.006 * (i + 1)) + (i * 0.8);
                const target = (Math.sin(phase) * 0.5 + 0.5) * (height - 4) + (Math.random() * 4);
                eqHeights[i] = eqHeights[i] * 0.6 + target * 0.4;
                if (eqHeights[i] > eqPeaks[i]) eqPeaks[i] = eqHeights[i];
                else eqPeaks[i] = Math.max(eqHeights[i], eqPeaks[i] - 0.5);
            } else {
                eqHeights[i] = Math.max(2, eqHeights[i] * 0.8);
                eqPeaks[i] = eqHeights[i];
            }

            const curX = x + i * (barWidth + 2);
            const barH = Math.max(2, Math.min(height, eqHeights[i]));
            const peakH = Math.max(2, Math.min(height, eqPeaks[i]));

            ctx.fillRect(curX, y + height - barH, barWidth, barH);
            if (peakH > barH) {
                ctx.fillRect(curX, y + height - peakH, barWidth, 2);
            }
        }
    }

    // Helper: Draw Spinning Vinyl Record Disc
    function drawSpinningVinyl(ctx, cx, cy, radius, frame) {
        ctx.beginPath();
        ctx.arc(cx, cy, radius, 0, Math.PI * 2);
        ctx.stroke();

        ctx.beginPath();
        ctx.arc(cx, cy, radius * 0.65, 0, Math.PI * 2);
        ctx.stroke();

        ctx.beginPath();
        ctx.arc(cx, cy, 4, 0, Math.PI * 2);
        ctx.fill();

        // 4 rotating spokes
        const angle = (frame % 8) * (Math.PI / 4);
        for (let s = 0; s < 4; s++) {
            const a = angle + (s * Math.PI / 2);
            const x1 = cx + Math.cos(a) * 6;
            const y1 = cy + Math.sin(a) * 6;
            const x2 = cx + Math.cos(a) * (radius - 2);
            const y2 = cy + Math.sin(a) * (radius - 2);
            ctx.beginPath();
            ctx.moveTo(x1, y1);
            ctx.lineTo(x2, y2);
            ctx.stroke();
        }
    }

    // Helper: Draw Floating Notes
    function drawFloatingNotes(ctx, minX, maxX, minY, maxY, isPlaying) {
        if (!isPlaying) return;

        floatingNotes.forEach((n) => {
            n.y -= n.speed * 0.8;
            n.phase += 0.08;
            const swayX = n.x + Math.sin(n.phase) * 6;

            if (n.y < minY) {
                n.y = maxY;
                n.x = minX + Math.random() * (maxX - minX);
            }

            ctx.font = '14px sans-serif';
            ctx.fillText(n.type ? '♪' : '♫', swayX, n.y);
        });
    }

    // Start Canvas Render Loop
    requestAnimationFrame(drawOledCanvas);

    // ========================================================================
    // 6. Theme Switcher (White, Cyan, Amber, Green)
    // ========================================================================
    themePicker.addEventListener('click', (e) => {
        const dot = e.target.closest('.color-dot');
        if (!dot) return;

        document.querySelectorAll('.color-dot').forEach(d => d.classList.remove('active'));
        dot.classList.add('active');

        const colorName = dot.dataset.color;
        currentThemeGlow = colorName;
        screenViewport.className = `screen-viewport oled-theme-${colorName}`;

        if (colorName === 'white') currentThemeColor = '#ffffff';
        else if (colorName === 'cyan') currentThemeColor = '#00e5ff';
        else if (colorName === 'amber') currentThemeColor = '#ffaa00';
        else if (colorName === 'green') currentThemeColor = '#00ff66';
    });

    // ========================================================================
    // 7. Demo Mode Controls
    // ========================================================================
    btnToggleDemo.addEventListener('click', async () => {
        try {
            const res = await fetch('/api/demo', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ active: !isDemoActive, trackIndex: demoTrackIndex })
            });
            const data = await res.json();
            isDemoActive = data.demoActive;
            showToast(isDemoActive ? 'Demo Simulation Started!' : 'Returned to Live Spotify Mode');
        } catch (e) {
            showToast('Failed to toggle demo mode', 'error');
        }
    });

    btnSwitchDemoTrack.addEventListener('click', async () => {
        demoTrackIndex = (demoTrackIndex + 1) % 2;
        try {
            await fetch('/api/demo', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ active: true, trackIndex: demoTrackIndex })
            });
            showToast('Switched Demo Track!');
        } catch (e) {
            showToast('Failed to switch demo track', 'error');
        }
    });

    // ========================================================================
    // 8. Spotify 1-Click OAuth Login & Credential Actions
    // ========================================================================
    btnSpotifyLogin.addEventListener('click', () => {
        const clientId = inputClientId.value.trim();
        const clientSecret = inputClientSecret.value.trim();

        let loginUrl = '/auth/login';
        if (clientId && clientSecret) {
            loginUrl += `?client_id=${encodeURIComponent(clientId)}&client_secret=${encodeURIComponent(clientSecret)}`;
        }
        window.location.href = loginUrl;
    });

    btnSaveCredentials.addEventListener('click', async () => {
        const clientId = inputClientId.value.trim();
        const clientSecret = inputClientSecret.value.trim();

        if (!clientId || !clientSecret) {
            showToast('Please enter both Client ID and Client Secret', 'error');
            return;
        }

        try {
            const res = await fetch('/api/credentials', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ clientId, clientSecret })
            });
            const data = await res.json();
            if (data.success) {
                showToast('Credentials saved successfully!');
            } else {
                showToast(data.error || 'Failed to save credentials', 'error');
            }
        } catch (e) {
            showToast('Error saving credentials', 'error');
        }
    });
});
