/**
 * ESP32 OLED Companion Server & Web Dashboard
 * Zero-dependency Node.js server (runs out-of-the-box with `node server.js`).
 */

const http = require('http');
const https = require('https');
const fs = require('fs');
const path = require('path');
const url = require('url');
const querystring = require('querystring');
const os = require('os');

const PORT = process.env.PORT || 3000;
const DATA_DIR = path.join(__dirname, 'data');
const TOKENS_FILE = path.join(DATA_DIR, 'tokens.json');
const PUBLIC_DIR = path.join(__dirname, 'public');

// Ensure data directory exists
if (!fs.existsSync(DATA_DIR)) {
    fs.mkdirSync(DATA_DIR, { recursive: true });
}

// Global state
let spotifyConfig = {
    clientId: process.env.SPOTIFY_CLIENT_ID || '',
    clientSecret: process.env.SPOTIFY_CLIENT_SECRET || '',
    refreshToken: process.env.SPOTIFY_REFRESH_TOKEN || '',
    accessToken: '',
    tokenExpiresAt: 0
};

// Load saved tokens if available
if (fs.existsSync(TOKENS_FILE)) {
    try {
        const saved = JSON.parse(fs.readFileSync(TOKENS_FILE, 'utf8'));
        spotifyConfig = { ...spotifyConfig, ...saved };
        console.log('[Auth] Loaded saved credentials from data/tokens.json');
    } catch (e) {
        console.error('[Auth] Error reading tokens.json:', e.message);
    }
}

// Current playback & lyrics state
let currentPlayback = {
    hasData: false,
    isPlaying: false,
    trackId: '',
    trackName: 'Waiting for Music',
    artistName: 'Play a song on Spotify',
    albumName: '',
    albumArt: '',
    progressMs: 0,
    durationMs: 0,
    lastUpdateMs: Date.now(),
    lyrics: [],
    activeLyric: '...',
    nextLyric: '',
    hasLyrics: false,
    mode: 'live' // 'live' or 'demo'
};

// Connected SSE clients for live push updates
const sseClients = new Set();

// Built-in Demo Tracks for simulation mode
const DEMO_TRACKS = [
    {
        trackName: 'Viva La Vida',
        artistName: 'Coldplay',
        albumName: 'Viva La Vida or Death and All His Friends',
        albumArt: 'https://i.scdn.co/image/ab67616d0000b273e3a479b47e2c9ef8abf98642',
        durationMs: 242000,
        lrc: `[00:01.00]♪ Instrumental Intro ♪
[00:13.50]I used to rule the world
[00:17.20]Seas would rise when I gave the word
[00:21.00]Now in the morning I sleep alone
[00:25.30]Sweep the streets I used to own
[00:29.50]I used to roll the dice
[00:33.20]Feel the fear in my enemy's eyes
[00:37.00]Listen as the crowd would sing
[00:41.00]Now the old king is dead! Long live the king!
[00:45.00]One minute I held the key
[00:49.00]Next the walls were closed on me
[00:53.00]And I discovered that my castles stand
[00:57.00]Upon pillars of salt and pillars of sand
[01:01.00]I hear Jerusalem bells a-ringing
[01:05.00]Roman Cavalry choirs are singing
[01:09.00]Be my mirror, my sword and shield
[01:13.00]My missionaries in a foreign field
[01:17.00]For some reason I can't explain
[01:21.00]Once you'd gone there was never
[01:23.00]Never an honest word
[01:25.00]And that was when I ruled the world
[01:30.00]♪ Orchestral Solo ♪`
    },
    {
        trackName: 'Bohemian Rhapsody',
        artistName: 'Queen',
        albumName: 'A Night at the Opera',
        albumArt: 'https://i.scdn.co/image/ab67616d0000b2737c39dd133836c2c1c0bbefe9',
        durationMs: 354000,
        lrc: `[00:01.00]Is this the real life?
[00:05.00]Is this just fantasy?
[00:08.50]Caught in a landside
[00:11.00]No escape from reality
[00:16.00]Open your eyes, look up to the skies and see
[00:26.50]I'm just a poor boy, I need no sympathy
[00:32.00]Because I'm easy come, easy go
[00:36.00]Little high, little low
[00:40.00]Any way the wind blows doesn't really matter to me
[00:50.00]Mama, just killed a man
[00:55.50]Put a gun against his head, pulled my trigger, now he's dead
[01:03.00]Mama, life had just begun
[01:09.00]But now I've gone and thrown it all away`
    }
];

let demoState = {
    active: false,
    trackIndex: 0,
    startTimeMs: 0,
    paused: false,
    pauseProgressMs: 0
};

// Helper: Save credentials to file
function saveCredentials() {
    try {
        fs.writeFileSync(TOKENS_FILE, JSON.stringify({
            clientId: spotifyConfig.clientId,
            clientSecret: spotifyConfig.clientSecret,
            refreshToken: spotifyConfig.refreshToken
        }, null, 2));
    } catch (e) {
        console.error('[Auth] Failed to save tokens to file:', e.message);
    }
}

// Helper: Get local server IP addresses
function getLocalIpAddresses() {
    const interfaces = os.networkInterfaces();
    const addresses = [];
    for (const k in interfaces) {
        for (const k2 in interfaces[k]) {
            const address = interfaces[k][k2];
            if (address.family === 'IPv4' && !address.internal) {
                addresses.push(address.address);
            }
        }
    }
    return addresses.length > 0 ? addresses : ['127.0.0.1'];
}

// Helper: HTTPS Request promise
function httpsRequest(options, postData = null) {
    return new Promise((resolve, reject) => {
        const req = https.request(options, (res) => {
            let data = '';
            res.on('data', chunk => data += chunk);
            res.on('end', () => resolve({ statusCode: res.statusCode, headers: res.headers, body: data }));
        });
        req.on('error', reject);
        if (postData) req.write(postData);
        req.end();
    });
}

// Refresh Spotify Access Token
async function refreshAccessToken() {
    if (!spotifyConfig.clientId || !spotifyConfig.clientSecret || !spotifyConfig.refreshToken) {
        return false;
    }

    const postData = querystring.stringify({
        grant_type: 'refresh_token',
        refresh_token: spotifyConfig.refreshToken,
        client_id: spotifyConfig.clientId,
        client_secret: spotifyConfig.clientSecret
    });

    try {
        const res = await httpsRequest({
            hostname: 'accounts.spotify.com',
            path: '/api/token',
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded',
                'Content-Length': Buffer.byteLength(postData)
            }
        }, postData);

        if (res.statusCode === 200) {
            const data = JSON.parse(res.body);
            spotifyConfig.accessToken = data.access_token;
            spotifyConfig.tokenExpiresAt = Date.now() + (data.expires_in || 3600) * 1000;
            if (data.refresh_token) {
                spotifyConfig.refreshToken = data.refresh_token;
                saveCredentials();
            }
            console.log('[Spotify] Access token refreshed successfully!');
            return true;
        } else {
            console.error('[Spotify] Refresh token failed HTTP', res.statusCode, res.body);
            return false;
        }
    } catch (e) {
        console.error('[Spotify] Refresh token error:', e.message);
        return false;
    }
}

// Fetch currently playing track from Spotify
async function fetchCurrentlyPlaying() {
    if (demoState.active) return; // Skip Spotify poll if in demo simulation mode

    if (!spotifyConfig.accessToken || Date.now() > spotifyConfig.tokenExpiresAt - 300000) {
        const refreshed = await refreshAccessToken();
        if (!refreshed) return;
    }

    try {
        const res = await httpsRequest({
            hostname: 'api.spotify.com',
            path: '/v1/me/player/currently-playing',
            method: 'GET',
            headers: {
                'Authorization': `Bearer ${spotifyConfig.accessToken}`
            }
        });

        if (res.statusCode === 200 && res.body) {
            const data = JSON.parse(res.body);
            const isPlaying = data.is_playing || false;
            const progressMs = data.progress_ms || 0;
            const item = data.item;

            if (item) {
                const trackId = item.id || '';
                const trackName = item.name || 'Unknown Track';
                const artistName = item.artists && item.artists[0] ? item.artists[0].name : 'Unknown Artist';
                const albumName = item.album ? item.album.name : '';
                const albumArt = item.album && item.album.images && item.album.images[0] ? item.album.images[0].url : '';
                const durationMs = item.duration_ms || 0;

                const trackChanged = (trackId !== currentPlayback.trackId);

                currentPlayback = {
                    ...currentPlayback,
                    hasData: true,
                    isPlaying,
                    trackId,
                    trackName,
                    artistName,
                    albumName,
                    albumArt,
                    progressMs,
                    durationMs,
                    lastUpdateMs: Date.now(),
                    mode: 'live'
                };

                if (trackChanged) {
                    console.log(`[Spotify] Now Playing: "${trackName}" by "${artistName}"`);
                    fetchSyncedLyrics(trackName, artistName, albumName, durationMs);
                } else {
                    updateActiveLyrics();
                    broadcastState();
                }
            }
        } else if (res.statusCode === 204) {
            currentPlayback.isPlaying = false;
            currentPlayback.hasData = false;
            broadcastState();
        } else if (res.statusCode === 401) {
            spotifyConfig.accessToken = '';
            await refreshAccessToken();
        }
    } catch (e) {
        console.error('[Spotify] Polling error:', e.message);
    }
}

// Parse standard LRC timestamped lyrics
function parseLRC(lrcText) {
    const lines = lrcText.split('\n');
    const parsed = [];

    for (let line of lines) {
        line = line.trim();
        const match = line.match(/\[(\d+):(\d+)(?:\.(\d+))?\](.*)/);
        if (match) {
            const mins = parseInt(match[1], 10);
            const secs = parseInt(match[2], 10);
            let ms = 0;
            if (match[3]) {
                if (match[3].length === 2) ms = parseInt(match[3], 10) * 10;
                else ms = parseInt(match[3].substring(0, 3).padEnd(3, '0'), 10);
            }
            const totalMs = (mins * 60 + secs) * 1000 + ms;
            const text = match[4].trim();
            parsed.push({ timestampMs: totalMs, text });
        }
    }
    return parsed.sort((a, b) => a.timestampMs - b.timestampMs);
}

// Fetch Synced Lyrics from LRCLIB
async function fetchSyncedLyrics(trackName, artistName, albumName, durationMs) {
    try {
        const query = querystring.stringify({
            track_name: trackName,
            artist_name: artistName,
            album_name: albumName || '',
            duration: durationMs ? Math.round(durationMs / 1000) : ''
        });

        const res = await httpsRequest({
            hostname: 'lrclib.net',
            path: `/api/get?${query}`,
            method: 'GET',
            headers: { 'User-Agent': 'ESP32-OLED-Spotify-Server/1.0' }
        });

        if (res.statusCode === 200 && res.body) {
            const data = JSON.parse(res.body);
            if (data.syncedLyrics) {
                currentPlayback.lyrics = parseLRC(data.syncedLyrics);
                currentPlayback.hasLyrics = currentPlayback.lyrics.length > 0;
                console.log(`[Lyrics] Loaded ${currentPlayback.lyrics.length} synced lyric lines for "${trackName}"`);
            } else {
                currentPlayback.lyrics = [];
                currentPlayback.hasLyrics = false;
                console.log(`[Lyrics] No synced lyrics available for "${trackName}"`);
            }
        } else {
            currentPlayback.lyrics = [];
            currentPlayback.hasLyrics = false;
        }
    } catch (e) {
        console.error('[Lyrics] Fetch error:', e.message);
        currentPlayback.lyrics = [];
        currentPlayback.hasLyrics = false;
    }

    updateActiveLyrics();
    broadcastState();
}

// Compute active and next lyric based on extrapolated milliseconds
function updateActiveLyrics() {
    let currentMs = currentPlayback.progressMs;
    if (currentPlayback.isPlaying) {
        currentMs += (Date.now() - currentPlayback.lastUpdateMs);
        if (currentPlayback.durationMs > 0 && currentMs > currentPlayback.durationMs) {
            currentMs = currentPlayback.durationMs;
        }
    }

    const lyrics = currentPlayback.lyrics;
    if (!lyrics || lyrics.length === 0) {
        currentPlayback.activeLyric = currentPlayback.hasData ? '♪ Instrumental ♪' : '...';
        currentPlayback.nextLyric = '';
        return;
    }

    let activeIdx = -1;
    for (let i = 0; i < lyrics.length; i++) {
        if (lyrics[i].timestampMs <= currentMs) {
            activeIdx = i;
        } else {
            break;
        }
    }

    if (activeIdx >= 0) {
        currentPlayback.activeLyric = lyrics[activeIdx].text || '♪ ♪ ♪';
        currentPlayback.nextLyric = (activeIdx + 1 < lyrics.length) ? lyrics[activeIdx + 1].text : '';
    } else {
        currentPlayback.activeLyric = lyrics[0].timestampMs > 10000 ? '♪ Intro ♪' : '...';
        currentPlayback.nextLyric = lyrics[0].text;
    }
}

// Broadcast current state to all connected SSE browser clients
function broadcastState() {
    updateActiveLyrics();
    const payload = JSON.stringify(getPublicStatus());
    for (const client of sseClients) {
        try {
            client.write(`data: ${payload}\n\n`);
        } catch (e) {
            sseClients.delete(client);
        }
    }
}

// Return formatted status for ESP32 and Web Client
function getPublicStatus() {
    let estProgress = currentPlayback.progressMs;
    if (currentPlayback.isPlaying) {
        estProgress += (Date.now() - currentPlayback.lastUpdateMs);
        if (currentPlayback.durationMs > 0 && estProgress > currentPlayback.durationMs) {
            estProgress = currentPlayback.durationMs;
        }
    }

    return {
        connected: Boolean(spotifyConfig.refreshToken || demoState.active),
        hasData: currentPlayback.hasData,
        isPlaying: currentPlayback.isPlaying,
        trackId: currentPlayback.trackId,
        trackName: currentPlayback.trackName,
        artistName: currentPlayback.artistName,
        albumName: currentPlayback.albumName,
        albumArt: currentPlayback.albumArt,
        progressMs: estProgress,
        durationMs: currentPlayback.durationMs,
        activeLyric: currentPlayback.activeLyric,
        nextLyric: currentPlayback.nextLyric,
        hasLyrics: currentPlayback.hasLyrics,
        totalLyricLines: currentPlayback.lyrics.length,
        mode: currentPlayback.mode,
        serverTime: Date.now()
    };
}

// Periodic loops
setInterval(fetchCurrentlyPlaying, 2500);

// Demo loop timer (runs at 100ms interval if demo mode is on)
setInterval(() => {
    if (demoState.active) {
        const demoTrack = DEMO_TRACKS[demoState.trackIndex];
        if (!demoState.paused) {
            const elapsed = (Date.now() - demoState.startTimeMs) % (demoTrack.durationMs + 3000);
            currentPlayback.hasData = true;
            currentPlayback.isPlaying = true;
            currentPlayback.trackId = `demo-${demoState.trackIndex}`;
            currentPlayback.trackName = demoTrack.trackName;
            currentPlayback.artistName = demoTrack.artistName;
            currentPlayback.albumName = demoTrack.albumName;
            currentPlayback.albumArt = demoTrack.albumArt;
            currentPlayback.durationMs = demoTrack.durationMs;
            currentPlayback.progressMs = elapsed;
            currentPlayback.lastUpdateMs = Date.now();
            currentPlayback.mode = 'demo';
        }
        updateActiveLyrics();
        broadcastState();
    }
}, 200);

// ============================================================================
// HTTP Request Router & Static File Handler
// ============================================================================
const MIME_TYPES = {
    '.html': 'text/html; charset=UTF-8',
    '.css': 'text/css; charset=UTF-8',
    '.js': 'application/javascript; charset=UTF-8',
    '.json': 'application/json; charset=UTF-8',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon'
};

const server = http.createServer(async (req, res) => {
    const parsedUrl = url.parse(req.url, true);
    const pathname = parsedUrl.pathname;

    // CORS Headers for API calls from other devices/ESP32
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, Authorization');

    if (req.method === 'OPTIONS') {
        res.writeHead(204);
        res.end();
        return;
    }

    // ------------------------------------------------------------------------
    // API: Real-time Server-Sent Events (SSE) stream for web UI
    // ------------------------------------------------------------------------
    if (pathname === '/api/events') {
        res.writeHead(200, {
            'Content-Type': 'text/event-stream',
            'Cache-Control': 'no-cache',
            'Connection': 'keep-alive'
        });
        res.write(`data: ${JSON.stringify(getPublicStatus())}\n\n`);
        sseClients.add(res);

        req.on('close', () => {
            sseClients.delete(res);
        });
        return;
    }

    // ------------------------------------------------------------------------
    // API: Current Playback Status (Lightweight JSON for ESP32 & Browser)
    // ------------------------------------------------------------------------
    if (pathname === '/api/status') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify(getPublicStatus()));
        return;
    }

    // ------------------------------------------------------------------------
    // API: Full Synced Lyrics Array
    // ------------------------------------------------------------------------
    if (pathname === '/api/lyrics') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            trackName: currentPlayback.trackName,
            artistName: currentPlayback.artistName,
            hasLyrics: currentPlayback.hasLyrics,
            lyrics: currentPlayback.lyrics
        }));
        return;
    }

    // ------------------------------------------------------------------------
    // API: Server Network & Host Info
    // ------------------------------------------------------------------------
    if (pathname === '/api/network-info') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            port: PORT,
            localIps: getLocalIpAddresses(),
            hostname: os.hostname(),
            platform: os.platform()
        }));
        return;
    }

    // ------------------------------------------------------------------------
    // API: Spotify OAuth Login Initiate
    // ------------------------------------------------------------------------
    if (pathname === '/auth/login') {
        const clientId = parsedUrl.query.client_id || spotifyConfig.clientId;
        const clientSecret = parsedUrl.query.client_secret || spotifyConfig.clientSecret;

        if (clientId && clientSecret) {
            spotifyConfig.clientId = clientId;
            spotifyConfig.clientSecret = clientSecret;
            saveCredentials();
        }

        if (!spotifyConfig.clientId) {
            res.writeHead(400, { 'Content-Type': 'text/html' });
            res.end('<h3>Error: Spotify Client ID is missing. Configure it on the dashboard.</h3>');
            return;
        }

        const host = req.headers.host || `localhost:${PORT}`;
        const redirectUri = `http://${host}/callback`;

        const authParams = querystring.stringify({
            client_id: spotifyConfig.clientId,
            response_type: 'code',
            redirect_uri: redirectUri,
            scope: 'user-read-currently-playing user-read-playback-state user-modify-playback-state'
        });

        res.writeHead(302, { 'Location': `https://accounts.spotify.com/authorize?${authParams}` });
        res.end();
        return;
    }

    // ------------------------------------------------------------------------
    // API: Spotify OAuth Callback
    // ------------------------------------------------------------------------
    if (pathname === '/callback') {
        const code = parsedUrl.query.code;
        const error = parsedUrl.query.error;

        if (error || !code) {
            res.writeHead(302, { 'Location': `/?error=${encodeURIComponent(error || 'Authorization failed')}` });
            res.end();
            return;
        }

        const host = req.headers.host || `localhost:${PORT}`;
        const redirectUri = `http://${host}/callback`;

        const postData = querystring.stringify({
            grant_type: 'authorization_code',
            code: code,
            redirect_uri: redirectUri
        });

        const authHeader = Buffer.from(`${spotifyConfig.clientId}:${spotifyConfig.clientSecret}`).toString('base64');

        try {
            const tokenRes = await httpsRequest({
                hostname: 'accounts.spotify.com',
                path: '/api/token',
                method: 'POST',
                headers: {
                    'Authorization': `Basic ${authHeader}`,
                    'Content-Type': 'application/x-www-form-urlencoded',
                    'Content-Length': Buffer.byteLength(postData)
                }
            }, postData);

            if (tokenRes.statusCode === 200) {
                const data = JSON.parse(tokenRes.body);
                spotifyConfig.accessToken = data.access_token;
                spotifyConfig.refreshToken = data.refresh_token;
                spotifyConfig.tokenExpiresAt = Date.now() + (data.expires_in || 3600) * 1000;
                saveCredentials();
                demoState.active = false;

                console.log('[Auth] Spotify OAuth authentication successful!');
                fetchCurrentlyPlaying();

                res.writeHead(302, { 'Location': '/?connected=true' });
                res.end();
            } else {
                console.error('[Auth] Token exchange failed:', tokenRes.body);
                res.writeHead(302, { 'Location': `/?error=token_exchange_failed` });
                res.end();
            }
        } catch (e) {
            console.error('[Auth] OAuth error:', e.message);
            res.writeHead(302, { 'Location': `/?error=${encodeURIComponent(e.message)}` });
            res.end();
        }
        return;
    }

    // ------------------------------------------------------------------------
    // API: Save Credentials or Tokens directly via POST
    // ------------------------------------------------------------------------
    if (pathname === '/api/credentials' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', async () => {
            try {
                const data = JSON.parse(body);
                if (data.clientId) spotifyConfig.clientId = data.clientId.trim();
                if (data.clientSecret) spotifyConfig.clientSecret = data.clientSecret.trim();
                if (data.refreshToken) spotifyConfig.refreshToken = data.refreshToken.trim();

                saveCredentials();
                if (spotifyConfig.refreshToken) {
                    await refreshAccessToken();
                    fetchCurrentlyPlaying();
                }

                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ success: true, message: 'Credentials updated successfully' }));
            } catch (e) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ success: false, error: e.message }));
            }
        });
        return;
    }

    // ------------------------------------------------------------------------
    // API: Toggle Offline Demo Mode
    // ------------------------------------------------------------------------
    if (pathname === '/api/demo' && req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            try {
                const data = JSON.parse(body || '{}');
                if (typeof data.active === 'boolean') {
                    demoState.active = data.active;
                } else {
                    demoState.active = !demoState.active;
                }

                if (demoState.active) {
                    demoState.trackIndex = (data.trackIndex !== undefined) ? data.trackIndex % DEMO_TRACKS.length : 0;
                    demoState.startTimeMs = Date.now();
                    demoState.paused = false;

                    const track = DEMO_TRACKS[demoState.trackIndex];
                    currentPlayback.lyrics = parseLRC(track.lrc);
                    currentPlayback.hasLyrics = true;
                    currentPlayback.mode = 'demo';
                    console.log(`[Demo] Started demo track: "${track.trackName}"`);
                } else {
                    currentPlayback.mode = 'live';
                    fetchCurrentlyPlaying();
                }

                broadcastState();
                res.writeHead(200, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ success: true, demoActive: demoState.active, track: DEMO_TRACKS[demoState.trackIndex] }));
            } catch (e) {
                res.writeHead(400, { 'Content-Type': 'application/json' });
                res.end(JSON.stringify({ success: false, error: e.message }));
            }
        });
        return;
    }

    // ------------------------------------------------------------------------
    // API: Generate ESP32 config.h File Content
    // ------------------------------------------------------------------------
    if (pathname === '/api/generate-config') {
        const primaryIp = getLocalIpAddresses()[0] || '192.168.1.100';
        const configContent = `// Auto-generated config.h for ESP32 OLED Display
#ifndef CONFIG_H
#define CONFIG_H

// Hardware & Display
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_RESET          -1
#define SCREEN_ADDRESS      0x3C
#define OLED_SDA            21
#define OLED_SCL            22

// Demo Mode Toggle
#define DEMO_MODE           ${demoState.active ? 'true' : 'false'}

// WiFi Configuration
#define WIFI_SSID           "UG_SIDHARTH"
#define WIFI_PASSWORD       "Sidharth@18"

// Spotify Credentials
#define SPOTIFY_CLIENT_ID     "${spotifyConfig.clientId || 'YOUR_CLIENT_ID'}"
#define SPOTIFY_CLIENT_SECRET "${spotifyConfig.clientSecret || 'YOUR_CLIENT_SECRET'}"
#define SPOTIFY_REFRESH_TOKEN "${spotifyConfig.refreshToken || 'YOUR_REFRESH_TOKEN'}"

// Server PC Endpoint (Optional: ESP32 can fetch from local server)
#define SERVER_HOST         "${primaryIp}"
#define SERVER_PORT         ${PORT}

// Timing
#define SPOTIFY_POLL_INTERVAL 3000
#define MAX_LYRIC_LINES       300
#define DISPLAY_FPS           25

#endif // CONFIG_H
`;
        res.writeHead(200, {
            'Content-Type': 'text/plain; charset=UTF-8',
            'Content-Disposition': 'attachment; filename="config.h"'
        });
        res.end(configContent);
        return;
    }

    // ------------------------------------------------------------------------
    // Static File Server (`public/` directory)
    // ------------------------------------------------------------------------
    let filePath = path.join(PUBLIC_DIR, pathname === '/' ? 'index.html' : pathname);

    // Prevent directory traversal
    if (!filePath.startsWith(PUBLIC_DIR)) {
        res.writeHead(403, { 'Content-Type': 'text/plain' });
        res.end('Forbidden');
        return;
    }

    fs.stat(filePath, (err, stats) => {
        if (err || !stats.isFile()) {
            // Fallback to index.html for SPA routes
            const fallbackPath = path.join(PUBLIC_DIR, 'index.html');
            if (fs.existsSync(fallbackPath)) {
                res.writeHead(200, { 'Content-Type': 'text/html; charset=UTF-8' });
                fs.createReadStream(fallbackPath).pipe(res);
            } else {
                res.writeHead(404, { 'Content-Type': 'text/plain' });
                res.end('File Not Found');
            }
            return;
        }

        const ext = path.extname(filePath).toLowerCase();
        const contentType = MIME_TYPES[ext] || 'application/octet-stream';

        res.writeHead(200, { 'Content-Type': contentType });
        fs.createReadStream(filePath).pipe(res);
    });
});

server.listen(PORT, '0.0.0.0', () => {
    console.log('\n' + '='.repeat(68));
    console.log(' ✨ ESP32 OLED Companion Server & Web Dashboard is Running!');
    console.log('='.repeat(68));
    console.log(` Local Access:      http://localhost:${PORT}`);
    const ips = getLocalIpAddresses();
    ips.forEach(ip => {
        console.log(` Network Access:    http://${ip}:${PORT}`);
    });
    console.log('='.repeat(68));
    console.log(' Ready to connect with Spotify & ESP32!\n');
});
