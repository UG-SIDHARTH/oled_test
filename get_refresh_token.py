#!/usr/bin/env python3
"""
Spotify Refresh Token Generator for ESP32
Run this script on your computer to log in to Spotify and retrieve your SPOTIFY_REFRESH_TOKEN.
Zero external dependencies required (uses built-in Python standard libraries).
"""

import sys
import urllib.parse
import urllib.request
import json
import base64
import webbrowser
from http.server import HTTPServer, BaseHTTPRequestHandler

REDIRECT_URI = "http://localhost:8888/callback"
SCOPES = "user-read-currently-playing user-read-playback-state"

auth_code = None

class CallbackHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        global auth_code
        parsed_path = urllib.parse.urlparse(self.path)
        query = urllib.parse.parse_qs(parsed_path.query)

        if "code" in query:
            auth_code = query["code"][0]
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(b"<h1>Authorization Successful!</h1><p>You can close this tab and return to your terminal.</p>")
        else:
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b"<h1>Authorization Failed!</h1>")

    def log_message(self, format, *args):
        return  # Suppress default server logs

def main():
    print("=" * 65)
    print(" Spotify OAuth 2.0 Refresh Token Generator for ESP32")
    print("=" * 65)

    client_id = input("\nEnter your Spotify Client ID: ").strip()
    client_secret = input("Enter your Spotify Client Secret: ").strip()

    if not client_id or not client_secret:
        print("[Error] Client ID and Client Secret cannot be empty!")
        sys.exit(1)

    # Construct Auth URL
    auth_params = {
        "client_id": client_id,
        "response_type": "code",
        "redirect_uri": REDIRECT_URI,
        "scope": SCOPES,
    }
    auth_url = "https://accounts.spotify.com/authorize?" + urllib.parse.urlencode(auth_params)

    print("\n[Step 1] Opening browser to authenticate with Spotify...")
    print(f"If browser doesn't open automatically, visit:\n{auth_url}\n")
    webbrowser.open(auth_url)

    # Start local HTTP server to receive callback code
    server_address = ("", 8888)
    httpd = HTTPServer(server_address, CallbackHandler)
    print("[Step 2] Waiting for authorization callback on http://localhost:8888/callback ...")
    
    while auth_code is None:
        httpd.handle_request()

    print("[Success] Received Authorization Code!")

    # Exchange Authorization Code for Refresh Token
    print("\n[Step 3] Exchanging Code for Refresh Token...")
    token_url = "https://accounts.spotify.com/api/token"
    
    auth_header = base64.b64encode(f"{client_id}:{client_secret}".encode()).decode()
    headers = {
        "Authorization": f"Basic {auth_header}",
        "Content-Type": "application/x-www-form-urlencoded"
    }

    data = urllib.parse.urlencode({
        "grant_type": "authorization_code",
        "code": auth_code,
        "redirect_uri": REDIRECT_URI
    }).encode("utf-8")

    req = urllib.request.Request(token_url, data=data, headers=headers, method="POST")

    try:
        with urllib.request.urlopen(req) as response:
            res_body = response.read().decode("utf-8")
            token_json = json.loads(res_body)

            refresh_token = token_json.get("refresh_token")
            access_token = token_json.get("access_token")

            print("\n" + "=" * 65)
            print(" YOUR SPOTIFY REFRESH TOKEN:")
            print("=" * 65)
            print(f"\n{refresh_token}\n")
            print("=" * 65)
            print("Copy the token above and paste it into config.h:")
            print('#define SPOTIFY_REFRESH_TOKEN "' + str(refresh_token) + '"')
            print("=" * 65 + "\n")

    except Exception as e:
        print(f"[Error] Failed to obtain refresh token: {e}")

if __name__ == "__main__":
    main()
