#!/usr/bin/env python3
import os

cookie = os.environ.get("HTTP_COOKIE", "")
session_id = ""
for pair in cookie.split(";"):
    if "SESSIONID=" in pair:
        session_id = pair.strip().split("=")[1]

if session_id:
    print("Content-Type: text/html")
    print()
    print(f"<html><body><h1>Welcome back!</h1><p>Your session ID is {session_id}</p></body></html>")
else:
    print("Content-Type: text/html")
    print()
    print("<html><body><h1>Access Denied</h1><a href='/static/login.html'>Login</a></body></html>")
