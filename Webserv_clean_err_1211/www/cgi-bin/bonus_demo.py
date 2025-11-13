#!/usr/bin/env python3

import os
import sys
import cgi
import cgitb

# Enable CGI error reporting
cgitb.enable()

# Print headers
print("Content-Type: text/html")
print("Set-Cookie: demo_session=abc123; Path=/; HttpOnly")
print("Set-Cookie: user_preference=dark_mode; Path=/; Max-Age=3600")
print("")  # Empty line required between headers and content

# Get environment variables
request_method = os.environ.get('REQUEST_METHOD', 'GET')
query_string = os.environ.get('QUERY_STRING', '')
content_length = os.environ.get('CONTENT_LENGTH', '0')

# Parse form data for POST requests
form_data = {}
if request_method == 'POST':
    form = cgi.FieldStorage()
    for field in form.keys():
        form_data[field] = form.getvalue(field)

print("""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Webserv Bonus Features Demo</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 50px auto;
            padding: 20px;
            background-color: #f5f5f5;
        }
        .container {
            background-color: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        .feature {
            margin: 20px 0;
            padding: 15px;
            background-color: #f8f9fa;
            border-left: 4px solid #28a745;
            border-radius: 5px;
        }
        .code {
            background-color: #f1f1f1;
            padding: 10px;
            border-radius: 5px;
            font-family: monospace;
            margin: 10px 0;
        }
        input, textarea, button {
            margin: 5px 0;
            padding: 10px;
            width: 100%;
            max-width: 300px;
        }
        button {
            background-color: #007bff;
            color: white;
            border: none;
            border-radius: 5px;
            cursor: pointer;
        }
        button:hover {
            background-color: #0056b3;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🎯 Webserv Bonus Features Demo</h1>
        <p>This page demonstrates the advanced bonus features implemented in Webserv!</p>
        
        <div class="feature">
            <h3>🍪 Cookie Management</h3>
            <p>Cookies are automatically set for this session:</p>
            <div class="code">
                Set-Cookie: demo_session=abc123; Path=/; HttpOnly<br>
                Set-Cookie: user_preference=dark_mode; Path=/; Max-Age=3600
            </div>
            <p>Check your browser's developer tools to see the cookies!</p>
        </div>
        
        <div class="feature">
            <h3>📊 Session Management</h3>
            <p>Server-side session tracking with secure session IDs.</p>
            <div class="code">
                Session ID: demo_session_""" + str(hash(os.environ.get('REMOTE_ADDR', '127.0.0.1'))) + """<br>
                Session created: Server-side managed<br>
                Auto-cleanup: Expired sessions removed
            </div>
        </div>
        
        <div class="feature">
            <h3>🗜️ Compression Support</h3>
            <p>Automatic content compression for supported MIME types:</p>
            <div class="code">
                Accept-Encoding: """ + os.environ.get('HTTP_ACCEPT_ENCODING', 'not provided') + """<br>
                Compression: gzip, deflate support<br>
                Auto-detection: Text, HTML, CSS, JS files
            </div>
        </div>
        
        <div class="feature">
            <h3>📥 Range Requests (Partial Content)</h3>
            <p>HTTP Range requests for efficient large file downloads:</p>
            <div class="code">
                Range: bytes=0-1023 (first 1KB)<br>
                Range: bytes=1024- (from 1KB to end)<br>
                Range: bytes=-500 (last 500 bytes)<br>
                Response: 206 Partial Content
            </div>
        </div>
        
        <div class="feature">
            <h3>🔄 Keep-Alive Connections</h3>
            <p>Persistent HTTP connections for better performance:</p>
            <div class="code">
                Connection: """ + os.environ.get('HTTP_CONNECTION', 'not specified') + """<br>
                Keep-Alive: timeout=30, max=100<br>
                Performance: Reduced connection overhead
            </div>
        </div>
        
        <div class="feature">
            <h3>🧪 Interactive Test Form</h3>
            <form method="POST" action="/cgi-bin/bonus_demo.py">
                <label for="test_data">Test Data:</label><br>
                <textarea name="test_data" rows="3" placeholder="Enter some test data...">{}</textarea><br>
                <button type="submit">Test POST Request</button>
            </form>
            """.format(form_data.get('test_data', '')))

if request_method == 'POST':
    print("""
            <div class="code">
                <strong>POST Data Received:</strong><br>""")
    for key, value in form_data.items():
        print(f"                {key}: {value}<br>")
    print("""            </div>""")

print("""
        </div>
        
        <div class="feature">
            <h3>📈 Server Statistics</h3>
            <div class="code">
                Request Method: """ + request_method + """<br>
                Query String: """ + query_string + """<br>
                Content Length: """ + content_length + """<br>
                Client IP: """ + os.environ.get('REMOTE_ADDR', 'unknown') + """<br>
                User Agent: """ + os.environ.get('HTTP_USER_AGENT', 'unknown')[:50] + """...<br>
                Server: """ + os.environ.get('SERVER_SOFTWARE', 'Webserv/1.0') + """
            </div>
        </div>
        
        <hr>
        <p style="text-align: center; color: #666;">
            <small>🚀 Webserv Bonus Features | C++98 Implementation</small>
        </p>
    </div>
</body>
</html>""")
