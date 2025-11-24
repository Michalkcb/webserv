#!/usr/bin/env python3
import os
import sys

# CGI response headers
print("Content-Type: text/html\r\n\r\n")

print("<html><body>")
print("<h1>CGI Demo</h1>")

# Handle GET parameters (from QUERY_STRING)
if os.environ.get("REQUEST_METHOD", "") == "GET":
    print(f"<p><b>GET parameters:</b> Hello from GET body</p>")

# Handle POST body (from stdin)
if os.environ.get("REQUEST_METHOD", "") == "POST":
#    content_length = int(os.environ.get("CONTENT_LENGTH", 0))
#    post_data = sys.stdin.read(content_length)
    print(f"<p><b>POST body:</b> Hello from POST body</p>")

print("</body></html>")
