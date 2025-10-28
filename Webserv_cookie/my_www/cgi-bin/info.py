#!/usr/bin/env python3

import os
import cgi

print("Content-Type: text/html\n")

print("""<!DOCTYPE html>
<html>
<head>
    <title>CGI Environment Info</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        table { border-collapse: collapse; width: 100%; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; }
    </style>
</head>
<body>
    <h1>🔧 CGI Environment Information</h1>
    <h2>Environment Variables:</h2>
    <table>
        <tr><th>Variable</th><th>Value</th></tr>""")

for key, value in sorted(os.environ.items()):
    print(f"        <tr><td>{key}</td><td>{value}</td></tr>")

print("""    </table>
    <p><a href="/">← Back to Home</a></p>
</body>
</html>""")
