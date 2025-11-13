#!/usr/bin/env python3
import os
import urllib.request

# Forward the incoming cookie to the server's session destroy endpoint.
cookie = os.environ.get('HTTP_COOKIE', '')
try:
    url = 'http://127.0.0.1:8080/session/destroy'
    req = urllib.request.Request(url)
    if cookie:
        req.add_header('Cookie', cookie)
    # We don't need the response body; just trigger the destroy.
    with urllib.request.urlopen(req, timeout=2) as resp:
        pass
except Exception:
    # ignore errors; we'll still clear the cookie client-side
    pass

# Emit headers: content-type and a Set-Cookie that clears SESSIONID.
print('Content-Type: text/html')
print('Set-Cookie: SESSIONID=; Path=/; Max-Age=0; HttpOnly')
print()
print('<html><body>')
print('<h1>Logged out</h1>')
print('<p>Your session has been terminated.</p>')
print("<p><a href='/'>Return to site</a></p>")
print("<p><a href='/static/login.html'>Login</a></p>")
print('</body></html>')
