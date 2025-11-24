#!/usr/bin/env python3
import os
import sys
import cgi
import uuid
import secrets

# Parse form data
form = cgi.FieldStorage()
username = form.getvalue("username")
password = form.getvalue("password")

# Dummy authentication
# Allow three demo users: admin (secret), alice (secret) and bob (builder)
if (username == "admin" and password == "secret") \
    or (username == "alice" and password == "secret_a") \
    or (username == "bob" and password == "secret_b"):
    # Use a cryptographically strong random token for the session id
    session_id = "sess_" + secrets.token_hex(16)
    # Register username in server-side session store BEFORE sending any
    # headers to the client. This ensures the server mapping exists when
    # the browser receives the cookie and makes follow-up requests.
    reg_ok = False
    try:
        import urllib.request, urllib.parse
        qs = urllib.parse.urlencode({'key': 'username', 'value': username})
        url = 'http://127.0.0.1:8080/session/set?' + qs
        req = urllib.request.Request(url)
        req.add_header('Cookie', 'SESSIONID=' + session_id)
        with urllib.request.urlopen(req, timeout=2) as resp:
            resp.read()
            reg_ok = True
    except Exception:
        # Registration failed; we'll still return a cookie but note failure
        # in the returned HTML. In production you'd probably abort on failure.
        reg_ok = False

    # Send SESSIONID cookie to client. We no longer emit a USER cookie;
    # server-side session store is authoritative.
    print("Content-Type: text/html")
    print(f"Set-Cookie: SESSIONID={session_id}; Path=/; HttpOnly")
    print()
    # Include the session id in the returned HTML so the user (or a demo page) can see it
    print("<html>")
    print("<head><meta charset=\"utf-8\"><title>Welcome</title></head>")
    print("<body>")
    print(f"<h1>Welcome, {username}!</h1>")
    print("<p>Session started.</p>")
    print(f"<p><strong>Session ID:</strong> <code id=\"sid\">{session_id}</code></p>")
    # Present links so the user can choose which profile to visit
    print("<p>View profiles:</p>")
    print("<ul>")
    print(f"<li><a href='/cgi-bin/profile.py?user=admin'>Admin's profile</a></li>")
    print(f"<li><a href='/cgi-bin/profile.py?user=alice'>Alice's profile</a></li>")
    print(f"<li><a href='/cgi-bin/profile.py?user=bob'>Bob's profile</a></li>")
    print("</ul>")
    print("</body></html>")
    # If server-side registration failed, show a small warning in the page
    if not reg_ok:
        print("<p><em>Warning: failed to register server-side session.</em></p>")
else:
    print("Content-Type: text/html")
    print()
    print("<html><body><h1>Login Failed</h1><a href='/static/login.html'>Try again</a></body></html>")
