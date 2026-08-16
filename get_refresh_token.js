#!/usr/bin/env node
/**
 * Spotify Refresh Token Generator for ESP32 (Node.js version)
 * Zero external dependencies (uses Node.js built-in modules).
 */

const http = require('http');
const https = require('https');
const url = require('url');
const readline = require('readline');
const { exec } = require('child_process');

const REDIRECT_URI = 'http://localhost:8888/callback';
const SCOPES = 'user-read-currently-playing user-read-playback-state';

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
});

function question(query) {
    return new Promise(resolve => rl.question(query, resolve));
}

function openBrowser(targetUrl) {
    const start = process.platform === 'darwin' ? 'open' :
                  process.platform === 'win32' ? 'start ""' : 'xdg-open';
    exec(`${start} "${targetUrl}"`);
}

async function main() {
    console.log('='.repeat(65));
    console.log(' Spotify OAuth 2.0 Refresh Token Generator for ESP32');
    console.log('='.repeat(65));

    const clientId = (await question('\nEnter your Spotify Client ID: ')).trim();
    const clientSecret = (await question('Enter your Spotify Client Secret: ')).trim();

    if (!clientId || !clientSecret) {
        console.error('[Error] Client ID and Client Secret cannot be empty!');
        rl.close();
        process.exit(1);
    }

    const authUrl = `https://accounts.spotify.com/authorize?${new URLSearchParams({
        client_id: clientId,
        response_type: 'code',
        redirect_uri: REDIRECT_URI,
        scope: SCOPES
    }).toString()}`;

    console.log('\n[Step 1] Opening browser to authenticate with Spotify...');
    console.log(`If browser does not open automatically, visit:\n${authUrl}\n`);
    openBrowser(authUrl);

    console.log('[Step 2] Waiting for authorization callback on http://localhost:8888/callback ...');

    const server = http.createServer(async (req, res) => {
        const reqUrl = url.parse(req.url, true);
        if (reqUrl.pathname === '/callback') {
            const code = reqUrl.query.code;
            if (code) {
                res.writeHead(200, { 'Content-Type': 'text/html' });
                res.end('<h1>Authorization Successful!</h1><p>You can close this tab and return to your terminal.</p>');
                server.close();

                console.log('[Success] Received Authorization Code!');
                console.log('\n[Step 3] Exchanging Code for Refresh Token...');

                // Request token
                const postData = new URLSearchParams({
                    grant_type: 'authorization_code',
                    code: code,
                    redirect_uri: REDIRECT_URI
                }).toString();

                const authHeader = Buffer.from(`${clientId}:${clientSecret}`).toString('base64');

                const tokenReq = https.request('https://accounts.spotify.com/api/token', {
                    method: 'POST',
                    headers: {
                        'Authorization': `Basic ${authHeader}`,
                        'Content-Type': 'application/x-www-form-urlencoded',
                        'Content-Length': Buffer.byteLength(postData)
                    }
                }, (tokenRes) => {
                    let body = '';
                    tokenRes.on('data', chunk => body += chunk);
                    tokenRes.on('end', () => {
                        try {
                            const data = JSON.parse(body);
                            if (data.refresh_token) {
                                console.log('\n' + '='.repeat(65));
                                console.log(' YOUR SPOTIFY REFRESH TOKEN:');
                                console.log('='.repeat(65));
                                console.log(`\n${data.refresh_token}\n`);
                                console.log('='.repeat(65));
                                console.log('Copy the token above and paste it into config.h:');
                                console.log(`#define SPOTIFY_REFRESH_TOKEN "${data.refresh_token}"`);
                                console.log('='.repeat(65) + '\n');
                            } else {
                                console.error('[Error] Failed to get refresh token:', data);
                            }
                        } catch (e) {
                            console.error('[Error] Failed to parse response:', e.message, body);
                        }
                        rl.close();
                    });
                });

                tokenReq.on('error', (e) => {
                    console.error('[Error] Request failed:', e.message);
                    rl.close();
                });

                tokenReq.write(postData);
                tokenReq.end();
            } else {
                res.writeHead(400, { 'Content-Type': 'text/html' });
                res.end('<h1>Authorization Failed! No code received.</h1>');
            }
        }
    });

    server.listen(8888);
}

main();
