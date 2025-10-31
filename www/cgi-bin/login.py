#!/usr/bin/env python3
import os
import sys
import cgi
import uuid

# Parse form data
form = cgi.FieldStorage()
username = form.getvalue("username")
password = form.getvalue("password")

# Dummy authentication
if username == "admin" and password == "secret":
    session_id = f"sess_{uuid.uuid4().hex}"
    # Send cookie (HttpOnly is kept for security) and show the session id in the body
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
    print("<p><a href='/cgi-bin/profile.py'>Go to profile</a></p>")
    print("</body></html>")
else:
    print("Content-Type: text/html")
    print()
    print("<html><body><h1>Login Failed</h1><a href='/static/login.html'>Try again</a></body></html>")
