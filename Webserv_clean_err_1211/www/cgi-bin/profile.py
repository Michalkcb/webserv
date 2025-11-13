#!/usr/bin/env python3
import os
import urllib.request
import urllib.parse
import json

# Read incoming cookies (we'll forward them to the server when querying session)
cookie = os.environ.get("HTTP_COOKIE", "")

# Parse optional ?user=NAME from QUERY_STRING to select which profile to view
qs = os.environ.get("QUERY_STRING", "")
requested_user = ""
if qs:
    parts = qs.split('&')
    for p in parts:
        if p.startswith('user='):
            requested_user = p.split('=', 1)[1]
            break

# Query server-side session store to determine logged-in username.
logged_in_user = ""
try:
    url = 'http://127.0.0.1:8080/session/get?key=username'
    req = urllib.request.Request(url)
    if cookie:
        req.add_header('Cookie', cookie)
    with urllib.request.urlopen(req, timeout=2) as resp:
        body = resp.read().decode('utf-8')
        # Expect JSON like {"key":"username","value":"alice"}
        try:
            j = json.loads(body)
            logged_in_user = j.get('value', '') if isinstance(j, dict) else ''
        except Exception:
            logged_in_user = ''
except Exception:
    logged_in_user = ''

if logged_in_user:
    print("Content-Type: text/html")
    print()
    if requested_user:
        user = requested_user.replace('<', '&lt;').replace('>', '&gt;')
        if logged_in_user == user or logged_in_user == 'admin':
            print(f"<html><body><h1>Profile: {user}</h1><p>Viewing profile for <strong>{user}</strong>.</p><p>Logged in as: {logged_in_user}</p><p><a href='/cgi-bin/logout.py'>Logout</a></p></body></html>")
        else:
            print(f"<html><body><h1>Access Denied</h1><p>You are logged in as <strong>{logged_in_user}</strong> and cannot view <strong>{user}</strong>'s profile.</p><p><a href='/cgi-bin/logout.py'>Back to Login</a></p></body></html>")
    else:
        lu = logged_in_user.replace('<', '&lt;').replace('>', '&gt;')
        print(f"<html><body><h1>Your Profile: {lu}</h1><p>Logged in as {lu}</p><p><a href='/cgi-bin/logout.py'>Logout</a></p></body></html>")
else:
    print("Content-Type: text/html")
    print()
    print("<html><body><h1>Access Denied</h1><a href='/static/login.html'>Login</a></body></html>")
